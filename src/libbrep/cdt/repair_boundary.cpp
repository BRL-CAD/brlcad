/*                    R E P A I R _ B O U N D A R Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */
/** @file cdt/repair_boundary.cpp
 *
 * Topology-constrained patches and bounded boundary repair.
 */

#include "common.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <map>
#include <memory>
#include <limits>
#include <numeric>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "bu/str.h"
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
#include "cdt.h"
#include "assembly.h"
#include "repair_boundary.h"
#include "surface.h"

#define MAX_BOUNDARY_STRIP_CORRESPONDENCE_TESTS 16777216
#define MAX_BOUNDARY_STRIP_REFINEMENT_POINTS 1048576

bool
repair_hybrid_fallback_preflight(size_t geometric_degenerate_faces,
	size_t invalid_vertex_links, size_t face_count,
	size_t unmatched_edges, size_t local_repair_limit)
{
    const size_t link_limit = geometric_degenerate_faces <= SIZE_MAX / 4 ?
	4 * geometric_degenerate_faces : SIZE_MAX;
    const bool disproportionate_links = geometric_degenerate_faces > 0 &&
	local_repair_limit > 0 &&
	invalid_vertex_links > local_repair_limit &&
	invalid_vertex_links > link_limit;
    const size_t degenerate_limit = local_repair_limit <= SIZE_MAX / 8 ?
	8 * local_repair_limit : SIZE_MAX;
    const bool dense_degeneracy = face_count > 0 &&
	local_repair_limit > 0 &&
	geometric_degenerate_faces > degenerate_limit &&
	geometric_degenerate_faces > face_count / 10;
    const size_t large_mesh_limit = local_repair_limit <= SIZE_MAX / 32 ?
	32 * local_repair_limit : SIZE_MAX;
    const bool large_flat_cracked_hybrid = unmatched_edges > 0 &&
	local_repair_limit > 0 && unmatched_edges <= local_repair_limit &&
	face_count > large_mesh_limit &&
	geometric_degenerate_faces > degenerate_limit;
    return disproportionate_links || dense_degeneracy ||
	large_flat_cracked_hybrid;
}

repair_triangle_edge_key
repair_triangle_edge(const repair_point_key &first,
	const repair_point_key &second)
{
    repair_triangle_edge_key edge = {first, second};
    if (edge[1] < edge[0])
	std::swap(edge[0], edge[1]);
    return edge;
}

size_t
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

fastf_t
repair_adaptive_hole_area_budget(
	const struct brep_cdt_repair_settings *settings,
	const struct bg_trimesh_repair_settings *mesh_settings,
	const struct bg_trimesh_repair_report *mesh_report)
{
    /* The percentage here limits which holes may be proposed by the final
     * triangle repair.  It does not relax the later aggregate comparison to
     * the B-Rep: area change and sampled surface deviation still determine
     * whether the candidate is accepted. */
    if (!settings || !mesh_settings || !mesh_report ||
	    !mesh_settings->fill_holes ||
	    !(settings->max_adaptive_hole_area_percent >
		mesh_settings->max_hole_area_percent) ||
	    mesh_report->unmatched_edges < 3 ||
	    (size_t)mesh_report->unmatched_edges >
		mesh_settings->max_hole_edges ||
	    (size_t)mesh_report->excess_edges >
		mesh_settings->max_hole_edges || mesh_report->misoriented_edges)
	return 0.0;
    return settings->max_adaptive_hole_area_percent;
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
    if (repair_adaptive_hole_edge_budget(&settings, &report))
	return 5;
    settings.mesh.max_hole_area_percent = 1.0;
    settings.max_adaptive_hole_area_percent = 4.0;
    settings.mesh.max_hole_edges = 256;
    report.unmatched_edges = 23;
    if (!NEAR_EQUAL(repair_adaptive_hole_area_budget(&settings,
	    &settings.mesh, &report), 4.0, SMALL_FASTF))
	return 6;
    report.excess_edges = 2;
    if (!NEAR_EQUAL(repair_adaptive_hole_area_budget(&settings,
	    &settings.mesh, &report), 4.0, SMALL_FASTF))
	return 7;
    report.excess_edges = 257;
    if (!NEAR_ZERO(repair_adaptive_hole_area_budget(&settings,
	    &settings.mesh, &report), SMALL_FASTF))
	return 8;
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
    sample.source_point = p3d;
    samples.push_back(sample);
    if (source_point)
	*source_point = p3d;
    return true;
}

repair_fast_constraint_store
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

typedef std::pair<ON_3dPoint *, ON_3dPoint *> repair_source_edge;

static repair_source_edge
repair_ordered_source_edge(ON_3dPoint *first, ON_3dPoint *second)
{
    if (std::less<ON_3dPoint *>()(second, first))
	std::swap(first, second);
    return repair_source_edge(first, second);
}

/* Boundary comparisons require an oriented patch without branching edges.
 * Interior edges must occur once in each direction and every boundary
 * vertex must have degree two. */
static bool
repair_patch_indexed_boundary(const int *faces, int face_count,
	size_t vertex_count, std::set<std::pair<int, int>> &boundary)
{
    boundary.clear();
    if (!faces || face_count <= 0 || vertex_count < 3)
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
		    (size_t)from >= vertex_count ||
		    (size_t)to >= vertex_count)
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
	boundary.insert(entry.first);
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
repair_patch_source_boundary(const int *faces, int face_count,
	const std::vector<ON_3dPoint *> &sources,
	std::set<repair_source_edge> &boundary)
{
    boundary.clear();
    std::set<std::pair<int, int>> indexed_boundary;
    if (!repair_patch_indexed_boundary(faces, face_count, sources.size(),
	    indexed_boundary))
	return false;
    for (const auto &edge : indexed_boundary) {
	ON_3dPoint *first = sources[(size_t)edge.first];
	ON_3dPoint *second = sources[(size_t)edge.second];
	if (!first || !second || first == second)
	    return false;
	boundary.insert(repair_ordered_source_edge(first, second));
    }
    return true;
}

bool
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

bool
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

/* A fan folds when its center lies outside a concave boundary's kernel.
 * A simple orthogonal projection instead defines an embedded disk using the
 * exact boundary vertices.  Keep an existing fan when a valid chart proves
 * its center lies in the polygon kernel; otherwise prefer the least-area
 * valid projected triangulation.  Source fidelity is checked by the caller.
 * Failure to find a chart does not establish that the spatial ring is bad. */
static bool
repair_projected_disk(const std::vector<ON_3dPoint> &boundary,
	const ON_3dPoint &center, std::vector<int> &replacement)
{
    if (boundary.size() < 3 || boundary.size() > (size_t)INT_MAX ||
	    !center.IsValid())
	return false;
    std::vector<int> outline(boundary.size());
    std::iota(outline.begin(), outline.end(), 0);
    std::set<std::pair<int, int>> expected_boundary;
    for (size_t point = 0; point < boundary.size(); ++point)
	expected_boundary.insert(std::minmax((int)point,
	    (int)((point + 1) % boundary.size())));
    std::vector<int> best_faces;
    double best_area = std::numeric_limits<double>::infinity();
    for (int normal_axis = 0; normal_axis < 3; ++normal_axis) {
	const int x = (normal_axis + 1) % 3;
	const int y = (normal_axis + 2) % 3;
	std::vector<detria::PointD> points;
	points.reserve(boundary.size());
	double coordinate_scale = 0.0;
	for (const ON_3dPoint &point : boundary) {
	    if (!point.IsValid())
		return false;
	    const ON_3dVector offset = point - center;
	    points.push_back({offset[x], offset[y]});
	    coordinate_scale = std::max(coordinate_scale,
		std::max(std::fabs(offset[x]), std::fabs(offset[y])));
	}
	detria::Triangulation<detria::PointD, int> triangulation;
	triangulation.setPoints(points);
	triangulation.addOutline(outline);
	if (!triangulation.triangulate(true))
	    continue;
	double signed_area = 0.0;
	for (size_t point = 0; point < points.size(); ++point) {
	    const detria::PointD &first = points[point];
	    const detria::PointD &second = points[(point + 1) % points.size()];
	    signed_area += first.x * second.y - second.x * first.y;
	}
	if (!std::isfinite(signed_area) || !(std::fabs(signed_area) > 0.0))
	    continue;
	const double orientation = signed_area > 0.0 ? 1.0 : -1.0;
	/* Roundoff allowance for the projected determinant, independent of
	 * modeling tolerance and the policy for thin-sheet intersections. */
	constexpr double determinant_roundoff_factor = 64.0;
	const double orientation_roundoff = determinant_roundoff_factor *
	    std::numeric_limits<double>::epsilon() * coordinate_scale *
	    coordinate_scale;
	bool fan_in_kernel = true;
	for (size_t point = 0; point < points.size(); ++point) {
	    const detria::PointD &first = points[point];
	    const detria::PointD &second = points[(point + 1) % points.size()];
	    if (orientation * (first.x * second.y - second.x * first.y) <
		    -orientation_roundoff) {
		fan_in_kernel = false;
		break;
	    }
	}
	if (fan_in_kernel)
	    return false;
	std::vector<int> candidate;
	double area = 0.0;
	bool valid = true;
	triangulation.forEachTriangle([&](const detria::Triangle<int> triangle) {
	    const ON_3dPoint vertices[3] = {boundary[(size_t)triangle.x],
		boundary[(size_t)triangle.y], boundary[(size_t)triangle.z]};
	    if (assembled_mesh_triangle_degenerate(vertices)) {
		valid = false;
		return;
	    }
	    area += 0.5 * ON_CrossProduct(vertices[1] - vertices[0],
		vertices[2] - vertices[0]).Length();
	    candidate.insert(candidate.end(), {triangle.x,
		orientation > 0.0 ? triangle.y : triangle.z,
		orientation > 0.0 ? triangle.z : triangle.y});
	}, false);
	std::set<std::pair<int, int>> candidate_boundary;
	if (valid && candidate.size() / 3 == boundary.size() - 2 &&
		std::isfinite(area) && area < best_area &&
		repair_patch_indexed_boundary(candidate.data(),
		    (int)(candidate.size() / 3), boundary.size(),
		    candidate_boundary) && candidate_boundary == expected_boundary) {
	    best_faces.swap(candidate);
	    best_area = area;
	}
    }
    if (best_faces.empty())
	return false;
    replacement.swap(best_faces);
    return true;
}

/* A new disk diagonal may share its endpoint indices with an unrelated
 * retained edge.  Joining those edges would create four incident faces.
 * Subdivide only the new diagonal, preserving both the patch geometry and
 * the retained mesh.  This also permits distinct sheets to touch under the
 * caller's existing intersection policy. */
static bool
repair_split_conflicting_patch_edges(std::vector<int> &patch_faces,
	std::vector<fastf_t> &vertices, const std::vector<int> &retained_faces)
{
    typedef std::pair<int, int> mesh_edge;
    std::map<mesh_edge, size_t> patch_edges;
    for (size_t face = 0; face < patch_faces.size() / 3; ++face) {
	for (int corner = 0; corner < 3; ++corner)
	    patch_edges[std::minmax(patch_faces[face * 3 + corner],
		patch_faces[face * 3 + (corner + 1) % 3])]++;
    }
    std::set<mesh_edge> conflicts;
    for (size_t face = 0; face < retained_faces.size() / 3; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const mesh_edge edge = std::minmax(retained_faces[face * 3 + corner],
		retained_faces[face * 3 + (corner + 1) % 3]);
	    const auto found = patch_edges.find(edge);
	    if (found != patch_edges.end() && found->second == 2)
		conflicts.insert(edge);
	}
    }
    for (const mesh_edge &edge : conflicts) {
	if (vertices.size() / 3 >= (size_t)INT_MAX)
	    return false;
	const ON_3dPoint first(&vertices[(size_t)edge.first * 3]);
	const ON_3dPoint second(&vertices[(size_t)edge.second * 3]);
	const ON_3dPoint midpoint = first + 0.5 * (second - first);
	if (!midpoint.IsValid() || !(midpoint.DistanceTo(first) > 0.0) ||
		!(midpoint.DistanceTo(second) > 0.0))
	    return false;
	const int middle = (int)(vertices.size() / 3);
	vertices.insert(vertices.end(), {midpoint.x, midpoint.y, midpoint.z});
	std::vector<int> split_faces;
	size_t split_count = 0;
	for (size_t face = 0; face < patch_faces.size() / 3; ++face) {
	    bool split = false;
	    for (int corner = 0; corner < 3; ++corner) {
		const int from = patch_faces[face * 3 + corner];
		const int to = patch_faces[face * 3 + (corner + 1) % 3];
		const mesh_edge candidate_edge = std::minmax(from, to);
		if (candidate_edge != edge)
		    continue;
		const int opposite = patch_faces[face * 3 + (corner + 2) % 3];
		split_faces.insert(split_faces.end(),
		    {from, middle, opposite, middle, to, opposite});
		split = true;
		split_count++;
		break;
	    }
	    if (!split)
		split_faces.insert(split_faces.end(),
		    patch_faces.begin() + (ptrdiff_t)(face * 3),
		    patch_faces.begin() + (ptrdiff_t)(face * 3 + 3));
	}
	if (split_count != 2)
	    return false;
	patch_faces.swap(split_faces);
    }
    return true;
}

/* Some imported faces have a trustworthy topological disk boundary but no
 * globally consistent pullback: loose incident edge curves can cross on the
 * source surface even though their authoritative 3-D samples form the local
 * boundary expected by adjacent faces.  Preserve every shared sample and
 * prefer a simple projected chart when a single interior point would fold
 * the disk.  Approximation remains restricted to this failed source face. */
bool
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
    std::vector<ON_3dPoint> boundary_points;
    boundary_points.reserve(boundary.size());
    for (const disk_point &point : boundary)
	boundary_points.push_back(point.point);
    if (repair_projected_disk(boundary_points, center, patch.faces)) {
	patch.vertices.resize(boundary.size() * 3);
	patch.source_points.resize(boundary.size());
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

bool
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

struct repair_replacement_mesh {
    std::vector<int> faces;
    std::vector<fastf_t> vertices;
    std::vector<int> sources;
    std::vector<int> remap;

    int synchronize()
    {
	std::vector<int> synchronized(faces.size());
	const int result = bg_trimesh_sync(synchronized.data(), faces.data(),
	    (int)(faces.size() / 3));
	if (result >= 0)
	    faces.swap(synchronized);
	return result;
    }

    bool compact(int &invalid_vertex)
    {
	remap.assign(vertices.size() / 3, -1);
	int count = 0;
	for (int &vertex : faces) {
	    if (vertex < 0 || (size_t)vertex >= remap.size()) {
		invalid_vertex = vertex;
		return false;
	    }
	    if (remap[(size_t)vertex] < 0)
		remap[(size_t)vertex] = count++;
	    vertex = remap[(size_t)vertex];
	}
	std::vector<fastf_t> compact_vertices((size_t)count * 3);
	for (size_t old = 0; old < remap.size(); ++old) {
	    if (remap[old] >= 0)
		memcpy(&compact_vertices[(size_t)remap[old] * 3],
		    &vertices[old * 3], 3 * sizeof(fastf_t));
	}
	vertices.swap(compact_vertices);
	return true;
    }

    void install(int **output_faces, int *face_count,
	    fastf_t **output_vertices, int *vertex_count,
	    std::vector<int> &output_sources)
    {
	cdt_owned_array<int> new_faces((int *)bu_malloc(
	    faces.size() * sizeof(int), "boundary replacement faces"));
	cdt_owned_array<fastf_t> new_vertices((fastf_t *)bu_malloc(
	    vertices.size() * sizeof(fastf_t), "boundary replacement vertices"));
	memcpy(new_faces.get(), faces.data(), faces.size() * sizeof(int));
	memcpy(new_vertices.get(), vertices.data(), vertices.size() * sizeof(fastf_t));
	bu_free(*output_faces, "pre-boundary replacement faces");
	bu_free(*output_vertices, "pre-boundary replacement vertices");
	*output_faces = new_faces.release();
	*output_vertices = new_vertices.release();
	*face_count = (int)(faces.size() / 3);
	*vertex_count = (int)(vertices.size() / 3);
	output_sources.swap(sources);
    }
};

/* Validation reports facts; each recovery stage retains its own acceptance
 * policy, including deliberately deferred vertex-link and solid checks.
 */
struct repair_replacement_validation {
    assembled_mesh_validation geometry;
    bg_trimesh_solid_errors edges = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int solid_result;

    explicit repair_replacement_validation(repair_replacement_mesh &mesh)
    {
	(void)assembled_mesh_validate((int)(mesh.vertices.size() / 3),
	    (int)(mesh.faces.size() / 3), mesh.vertices.data(),
	    mesh.faces.data(), &geometry, false);
	solid_result = bg_trimesh_solid2((int)(mesh.vertices.size() / 3),
	    (int)(mesh.faces.size() / 3), mesh.vertices.data(),
	    mesh.faces.data(), &edges);
    }
    ~repair_replacement_validation() { bg_free_trimesh_solid_errors(&edges); }
    repair_replacement_validation(const repair_replacement_validation &) = delete;
    repair_replacement_validation &operator=(const repair_replacement_validation &) = delete;
};

typedef std::pair<int, int> repair_indexed_edge;
struct repair_edge_use {
    int face;
    int from;
    int to;
};

struct repair_rigorous_boundary {
    int source_face = -1;
    std::vector<repair_edge_use> edges;
    std::vector<std::vector<int>> rings;
    double approximate_area = 0.0;
    int replaced_faces = 0;
};

static bool
repair_collect_rigorous_boundary(ON_Brep_CDT_State *s_cdt,
	const int *faces, int face_count, const fastf_t *vertices,
	int vertex_count, int rigorous_face_count,
	const std::vector<int> &source_faces, int requested_source_face,
	const std::set<repair_indexed_edge> *requested_boundary,
	size_t max_boundary_edges, repair_rigorous_boundary &result)
{
    const bool debug_topology = getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    int &source_face = result.source_face;
    auto &boundary = result.edges;
    auto &rings = result.rings;
    double &approximate_face_area = result.approximate_area;
    int &replaced_face_count = result.replaced_faces;
    const auto reject_boundary = [&](const char *reason) {
	if (debug_topology)
	    bu_log("Rigorous-boundary face repair rejected: %s\n", reason);
	return false;
    };

    std::set<int> approximate_sources;
    for (int face = rigorous_face_count; face < face_count; ++face) {
	if (source_faces[(size_t)face] < 0)
	    return reject_boundary("an approximate triangle has no source face");
	approximate_sources.insert(source_faces[(size_t)face]);
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[(size_t)face * 3 + corner];
	    if (vertex < 0 || vertex >= vertex_count)
		return false;
	}
    }
    if ((requested_source_face < 0 && approximate_sources.size() != 1) ||
	    (requested_source_face >= 0 && approximate_sources.find(
		requested_source_face) == approximate_sources.end()))
	return reject_boundary("the approximate suffix does not identify the "
	    "requested source face");
    source_face = requested_source_face >= 0 ?
	requested_source_face : *approximate_sources.begin();
    if (source_face < 0 || source_face >= s_cdt->orig_brep->m_F.Count())
	return reject_boundary("the source face index is out of range");

    std::map<repair_indexed_edge, std::vector<repair_edge_use>> incidence;
    const int incidence_face_count = requested_boundary ? face_count :
	rigorous_face_count;
    for (int face = 0; face < incidence_face_count; ++face) {
	if (requested_boundary && face >= rigorous_face_count &&
		source_faces[(size_t)face] == source_face)
	    continue;
	for (int corner = 0; corner < 3; ++corner) {
	    const int from = faces[(size_t)face * 3 + corner];
	    const int to = faces[(size_t)face * 3 + (corner + 1) % 3];
	    if (from == to)
		continue;
	    if (from < 0 || to < 0 || from >= vertex_count ||
		    to >= vertex_count)
		return reject_boundary("mesh incidence has an invalid vertex");
	    const repair_indexed_edge key = std::minmax(from, to);
	    /* A requested ring needs all uses of its edges, but no incidence
	     * storage for the rest of the retained mesh.  Keep vertex validation
	     * above this filter so unrelated malformed indices still reject. */
	    if (requested_boundary && requested_boundary->find(key) ==
		    requested_boundary->end())
		continue;
	    incidence[key].push_back({face, from, to});
	}
    }

    std::set<repair_indexed_edge> selected_boundary;
    for (const auto &edge : incidence) {
	if (edge.second.size() > 2) {
	    if (debug_topology) {
		bu_log("Rigorous-boundary retained edge %d-%d has %zu uses:",
		    edge.first.first, edge.first.second, edge.second.size());
		for (const repair_edge_use &use : edge.second)
		    bu_log(" triangle %d/source %d [%d,%d,%d]", use.face,
			(size_t)use.face < source_faces.size() ?
			source_faces[(size_t)use.face] : -1,
			faces[(size_t)use.face * 3],
			faces[(size_t)use.face * 3 + 1],
			faces[(size_t)use.face * 3 + 2]);
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
	    const repair_edge_use &retained = edge.second.front();
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
	    for (const repair_indexed_edge &edge : *requested_boundary) {
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

    const fastf_t *input_vertices = vertices;
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

    for (int face = rigorous_face_count; face < face_count; ++face) {
	if (source_faces[(size_t)face] != source_face)
	    continue;
	replaced_face_count++;
	const ON_3dPoint first(&input_vertices[(size_t)
	    faces[(size_t)face * 3] * 3]);
	const ON_3dPoint second(&input_vertices[(size_t)
	    faces[(size_t)face * 3 + 1] * 3]);
	const ON_3dPoint third(&input_vertices[(size_t)
	    faces[(size_t)face * 3 + 2] * 3]);
	approximate_face_area += 0.5 *
	    ON_CrossProduct(second - first, third - first).Length();
    }
    if (!replaced_face_count)
	return false;
    return true;
}

static bool
repair_triangulate_chart(size_t boundary_count,
	bool debug_topology, std::vector<int> &patch_faces,
	const std::vector<detria::PointD> &points,
	const std::vector<std::vector<int>> &contours,
	const std::vector<int> &point_vertices, bool log_chart)
{
    if (points.size() != boundary_count || contours.size() < 2)
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
    const size_t expected_faces = boundary_count +
	2 * (contours.size() - 1) - 2;
    if (result.size() / 3 != expected_faces)
	return false;
    patch_faces.swap(result);
    return true;
}

static bool
repair_triangulate_clean_chart(size_t boundary_count,
	bool debug_topology, std::vector<int> &patch_faces,
	const std::vector<detria::PointD> &points,
	const std::vector<std::vector<int>> &contours,
	const std::vector<int> &point_vertices)
{
    if (points.size() != boundary_count || contours.size() < 2 ||
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
    cdt_owned_array<int> clean_faces;
    int clean_face_count = 0;
    cdt_owned_array<point2d_t> clean_points;
    int clean_point_count = 0;
    std::vector<int> clean_constraints;
    clean_constraints.reserve(boundary_count * 2);
    for (const std::vector<int> &contour : contours) {
	for (size_t point = 0; point < contour.size(); ++point) {
	    clean_constraints.push_back(contour[point]);
	    clean_constraints.push_back(
		contour[(point + 1) % contour.size()]);
	}
    }
    const int clean_result =
	bg_nested_poly_triangulate_clean(
	cdt_array_address(clean_faces), &clean_face_count, cdt_array_address(clean_points),
	&clean_point_count, contours[outline].data(),
	contours[outline].size(), hole_arrays.data(),
	hole_counts.data(), hole_arrays.size(), NULL, 0,
	clean_constraints.data(), boundary_count,
	(const point2d_t *)input_points.data(), points.size(), NULL);
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
    for (size_t refinement = 0; valid && refinement < boundary_count;
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
	    boundary_count);

    if (!valid)
	return false;
    patch_faces.swap(result);
    if (debug_topology)
	bu_log("Rigorous-boundary chart accepted a Clipper-cleaned "
	    "%zu-triangle topology without moving boundary points\n",
	    patch_faces.size() / 3);
    return true;
}

/* When a failed-face triangulation cannot reproduce its authoritative
 * neighbors, discard only that approximate face and recover the exact indexed
 * rings exposed by the retained rigorous prefix.  Close one ring with a
 * bounded fan.  For holes, try the source chart, topology-preserving Clipper
 * cleanup, a bounded inferred chart, and finally a certified common plane.
 * No path moves or welds a retained rigorous vertex. */
bool
repair_failed_face_from_rigorous_boundary(
	struct ON_Brep_CDT_State *s_cdt, int **faces, int *face_count,
	fastf_t **vertices, int *vertex_count, int rigorous_face_count,
	std::vector<int> &source_faces, double allowed_deviation,
	size_t max_boundary_edges, repair_degenerate_neighborhood_stats *stats,
	int requested_source_face,
	const std::set<std::pair<int, int>> *requested_boundary,
	bool require_solid, std::vector<int> *vertex_remap)
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
    repair_rigorous_boundary retained;
    if (!repair_collect_rigorous_boundary(s_cdt, *faces, *face_count,
	    *vertices, *vertex_count, rigorous_face_count, source_faces,
	    requested_source_face, requested_boundary, max_boundary_edges, retained))
	return false;
    const int source_face = retained.source_face;
    const auto &boundary = retained.edges;
    const auto &rings = retained.rings;
    const double approximate_face_area = retained.approximate_area;
    const int replaced_face_count = retained.replaced_faces;
    const fastf_t *input_vertices = *vertices;
    const std::vector<int> &ring = rings.front();
    const ON_BrepFace &brep_face = s_cdt->orig_brep->m_F[source_face];
    const ON_Surface *surface = brep_face.SurfaceOf();
    std::vector<int> patch_faces;
    std::vector<fastf_t> added_vertices;
    double center_distance = 0.0;
    bool inferred_topology = false;
    bool projected_disk = false;
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
	std::vector<ON_3dPoint> boundary_points;
	boundary_points.reserve(ring.size());
	for (int vertex : ring)
	    boundary_points.push_back(ON_3dPoint(
		&input_vertices[(size_t)vertex * 3]));
	std::vector<int> projected_faces;
	if (repair_projected_disk(boundary_points, center, projected_faces)) {
	    for (int &vertex : projected_faces)
		vertex = ring[(size_t)vertex];
	    patch_faces.swap(projected_faces);
	    added_vertices.clear();
	    center_distance = 0.0;
	    projected_disk = true;
	}
    } else {
	/* Every chart below is used only to choose triangle connectivity.  The
	 * output points are always the exact 3-D vertices supplied by rigorous
	 * neighboring faces. */

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
	    if (chart_valid && !repair_triangulate_chart(boundary.size(),
		    debug_topology, patch_faces, surface_points, surface_rings,
		    surface_to_vertex, true))
		(void)repair_triangulate_clean_chart(boundary.size(), debug_topology,
		    patch_faces, surface_points, surface_rings, surface_to_vertex);
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
		if (!repair_triangulate_chart(boundary.size(), debug_topology, patch_faces,
		rotated, surface_rings,
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
	if (!repair_triangulate_chart(boundary.size(), debug_topology, patch_faces,
		chart_points, chart_rings, chart_to_vertex,
		true))
	    return false;
    }
    }

    repair_replacement_mesh candidate;
    auto &candidate_faces = candidate.faces;
    auto &candidate_sources = candidate.sources;
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
    auto &candidate_vertices = candidate.vertices;
    candidate_vertices.assign(*vertices, *vertices + (size_t)*vertex_count * 3);
    candidate_vertices.insert(candidate_vertices.end(),
	added_vertices.begin(), added_vertices.end());
    if (projected_disk && !repair_split_conflicting_patch_edges(patch_faces,
	    candidate_vertices, candidate_faces))
	return false;
    candidate_faces.insert(candidate_faces.end(), patch_faces.begin(),
	patch_faces.end());
    candidate_sources.insert(candidate_sources.end(),
	patch_faces.size() / 3, source_face);

    candidate.synchronize();
    int invalid_vertex = -1;
    if (!candidate.compact(invalid_vertex))
	return false;
    const repair_replacement_validation checked(candidate);
    const auto &validation = checked.geometry;
    const auto &solid_errors = checked.edges;
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
		    &candidate_vertices[(size_t)vertex * 3]);
	    }
	    if (assembled_mesh_triangle_degenerate(points))
		++deferred_degenerate_faces;
	}
    }
    if (validation.invalid_indices || validation.nonfinite_vertices ||
	    validation.unused_vertices ||
	    validation.degenerate_faces > deferred_degenerate_faces ||
	    (require_solid && validation.invalid_vertex_links) ||
	    (require_solid && checked.solid_result)) {
	if (debug_topology)
	    bu_log("Rigorous-boundary face repair rejected after assembly: "
		"boundary %zu, degenerate %zu, invalid links %zu, "
		"unmatched %d, excess %d, misoriented %d\n",
		ring.size(), validation.degenerate_faces,
		validation.invalid_vertex_links, solid_errors.unmatched.count,
		solid_errors.excess.count, solid_errors.misoriented.count);

	return false;
    }

    stats->components = 1;
    stats->removed_faces = replaced_face_count;
    stats->added_faces = (int)(patch_faces.size() / 3);
    stats->boundary_edges = (int)boundary.size();
    stats->max_center_offset = center_distance;
    stats->inferred_topology = inferred_topology;
    candidate.install(faces, face_count, vertices, vertex_count, source_faces);
    stats->source_faces.insert(source_face);
    if (vertex_remap)
	vertex_remap->assign(candidate.remap.begin(), candidate.remap.end());
    return true;
}

static int repair_near_boundary_pair_contract(void);
static int repair_weld_roundoff_contract(void);

static bool
repair_split_directed_euler_cycles(const std::vector<int> &circuit,
	size_t edge_count, std::vector<std::vector<int>> &rings)
{
    rings.clear();
    std::vector<int> pending_ring;
    std::map<int, size_t> ring_position;
    size_t ring_edges = 0;
    for (int vertex : circuit) {
	const auto prior = ring_position.find(vertex);
	if (prior == ring_position.end()) {
	    ring_position[vertex] = pending_ring.size();
	    pending_ring.push_back(vertex);
	    continue;
	}
	const size_t cycle_start = prior->second;
	std::vector<int> ring(pending_ring.begin() + cycle_start,
	    pending_ring.end());
	if (ring.size() < 3)
	    return false;
	ring_edges += ring.size();
	rings.push_back(std::move(ring));
	for (size_t point = cycle_start + 1; point < pending_ring.size();
		point++)
	    ring_position.erase(pending_ring[point]);
	pending_ring.resize(cycle_start + 1);
    }
    return pending_ring.size() == 1 && ring_edges == edge_count;
}


static int
repair_degenerate_neighborhood_contract(void)
{
    std::vector<std::vector<int>> weak_rings;
    if (!repair_split_directed_euler_cycles({0, 1, 2, 0, 3, 4, 0}, 6,
	    weak_rings) || weak_rings !=
	    std::vector<std::vector<int>>({{0, 1, 2}, {0, 3, 4}}) ||
	    repair_split_directed_euler_cycles({0, 1, 0}, 2, weak_rings))
	return 1;

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
    const bool repaired = repair_degenerate_neighborhoods(
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
	return 2;

    /* The same independently flat island may come from the rigorous prefix.
     * Source attribution plus geometric degeneracy is sufficient to consider
     * it, but the complete solid gate must still approve its removal. */
    face_count = 14;
    vertex_count = 8;
    faces = (int *)bu_malloc(sizeof(input_faces),
	"rigorous degenerate neighborhood contract faces");
    vertices = (fastf_t *)bu_malloc(sizeof(input_vertices),
	"rigorous degenerate neighborhood contract vertices");
    memcpy(faces, input_faces, sizeof(input_faces));
    memcpy(vertices, input_vertices, sizeof(input_vertices));
    sources.assign(14, -1);
    sources[12] = 0;
    sources[13] = 0;
    stats = repair_degenerate_neighborhood_stats();
    const bool rigorous_repaired =
	repair_degenerate_neighborhoods(&faces, &face_count,
	    &vertices, &vertex_count, 14, sources, 0.1, 16, &stats);
    validation = assembled_mesh_validation();
    const bool rigorous_valid = rigorous_repaired && face_count == 12 &&
	vertex_count == 8 && stats.components == 1 &&
	stats.removed_faces == 2 && stats.added_faces == 0 &&
	stats.source_faces == std::set<int>({0}) && sources.size() == 12 &&
	assembled_mesh_validate(vertex_count, face_count, vertices, faces,
	    &validation, false) && !validation.degenerate_faces &&
	!validation.invalid_vertex_links &&
	!bg_trimesh_solid2(vertex_count, face_count, vertices, faces, NULL);
    bu_free(faces, "rigorous degenerate neighborhood contract result faces");
    bu_free(vertices,
	"rigorous degenerate neighborhood contract result vertices");
    if (!rigorous_valid)
	return 3;

    /* Removing a closed all-degenerate island must reject the transaction.
     * Edge-only checks are vacuously true for an empty candidate, but an
     * empty result is not a valid mesh and previously reached bu_malloc(0). */
    const fastf_t empty_vertices[2][3] = {
	{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}
    };
    const int empty_faces[2][3] = {{0, 1, 0}, {0, 1, 0}};
    face_count = 2;
    vertex_count = 2;
    faces = (int *)bu_malloc(sizeof(empty_faces),
	"empty degenerate neighborhood contract faces");
    vertices = (fastf_t *)bu_malloc(sizeof(empty_vertices),
	"empty degenerate neighborhood contract vertices");
    memcpy(faces, empty_faces, sizeof(empty_faces));
    memcpy(vertices, empty_vertices, sizeof(empty_vertices));
    sources.assign(2, 0);
    stats = repair_degenerate_neighborhood_stats();
    const bool empty_rejected = !repair_degenerate_neighborhoods(
	&faces, &face_count, &vertices, &vertex_count, 0, sources, 0.1,
	16, &stats) && face_count == 2 && vertex_count == 2 &&
	sources == std::vector<int>({0, 0}) &&
	!memcmp(faces, empty_faces, sizeof(empty_faces)) &&
	!memcmp(vertices, empty_vertices, sizeof(empty_vertices));
    bu_free(faces, "empty degenerate neighborhood contract result faces");
    bu_free(vertices,
	"empty degenerate neighborhood contract result vertices");
    if (!empty_rejected)
	return 4;

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
    const bool grown = repair_degenerate_neighborhoods(&faces,
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
    return grown_valid ? 0 : 5;
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
    if (!repair_hybrid_fallback_preflight(23, 458, 4000, 0, 256))
	return 50;
    if (repair_hybrid_fallback_preflight(236, 458, 4000, 0, 256))
	return 51;
    if (!repair_hybrid_fallback_preflight(136, 1044, 4000, 0, 256))
	return 52;
    if (repair_hybrid_fallback_preflight(23, 200, 4000, 0, 256))
	return 53;
    /* A hybrid dominated by flat triangles is also unsuitable for generic
	 * hole filling, but a sparse flat population remains locally repairable. */
    if (!repair_hybrid_fallback_preflight(9349, 0, 70036, 0, 256))
	return 54;
    if (repair_hybrid_fallback_preflight(3414, 0, 179978, 0, 256))
	return 55;
    if (!repair_hybrid_fallback_preflight(5011, 3, 530925, 3, 256))
	return 56;
    if (repair_hybrid_fallback_preflight(100, 3, 530925, 3, 256))
	return 57;
    return 0;
}

/* Pair only opposite open edges whose endpoints are locally coincident.
 * This is narrower than whole-mesh coordinate welding: distinct rigorous
 * vertices are never merged, and no edit is accepted unless every indexed
 * edge becomes two-sided. */
bool
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
size_t
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

/* A rigorous or constrained fallback can be topologically closed while
 * containing small regions whose source face collapsed to a line.  Removing
 * those triangles globally reopens every seam.  Instead, replace only complete
 * attributed edge-connected degenerate neighborhoods, retaining every
 * perimeter index.  A simple boundary is spanned from its centroid when
 * possible; if the ring itself is numerically flat, move only the new interior
 * point a bounded distance in a tangent direction inferred from the retained
 * neighboring triangles. */
struct repair_degenerate_components {
    std::map<repair_indexed_edge, std::vector<repair_edge_use>> edge_uses;
    std::map<int, std::vector<int>> faces;
};

static bool
repair_collect_degenerate_components(const int *input_faces,
	int input_face_count, const fastf_t *input_vertices, int input_vertex_count,
	int rigorous_face_count, const std::vector<int> &source_faces,
	repair_degenerate_components &result)
{
    const bool debug_topology = getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY") != NULL;
    auto &edge_uses = result.edge_uses;
    auto &components = result.faces;
    std::vector<bool> degenerate((size_t)input_face_count, false);
    std::vector<int> degenerate_faces;

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
	    if (source_faces[(size_t)face] < 0) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: triangle "
			"%d is unattributed (rigorous prefix %d, source %d)\n",
			face, rigorous_face_count,
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
	for (const repair_edge_use &use : edge.second) {
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

    for (size_t local = 0; local < degenerate_faces.size(); ++local)
	components[component_root((int)local)].push_back(
	    degenerate_faces[local]);

    return true;
}

bool
repair_degenerate_neighborhoods(int **faces, int *face_count,
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

    const int input_face_count = *face_count;
    const int input_vertex_count = *vertex_count;
    const int *input_faces = *faces;
    const fastf_t *input_vertices = *vertices;
    repair_degenerate_components discovered;
    if (!repair_collect_degenerate_components(input_faces, input_face_count,
	    input_vertices, input_vertex_count, rigorous_face_count, source_faces,
	    discovered))
	return false;
    const auto &edge_uses = discovered.edge_uses;
    const auto &components = discovered.faces;

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
	std::set<repair_indexed_edge> component_edges;
	const auto add_component_edges = [&](int face) {
	    for (int corner = 0; corner < 3; ++corner) {
		const int from = input_faces[(size_t)face * 3 + corner];
		const int to = input_faces[(size_t)face * 3 +
		    (corner + 1) % 3];
		if (from != to)
		    component_edges.insert(std::minmax(from, to));
	    }
	};
	for (int face : component) {
	    component_sources.insert(source_faces[(size_t)face]);
	    add_component_edges(face);
	}
	std::vector<repair_edge_use> boundary;
	std::map<repair_indexed_edge, int> retained_neighbors;
	const size_t max_component_faces = std::max((size_t)16,
	    max_boundary_edges <= SIZE_MAX / 4 ? 4 * max_boundary_edges :
	    max_boundary_edges);
	for (;;) {
	    boundary.clear();
	    retained_neighbors.clear();
	    std::set<int> forced_grow_faces;
	    for (const repair_indexed_edge &edge_key : component_edges) {
		const auto edge_iter = edge_uses.find(edge_key);
		if (edge_iter == edge_uses.end())
		    continue;
		const auto &edge = *edge_iter;
		bool component_incident = false;
		std::vector<repair_edge_use> retained_uses;
		for (const repair_edge_use &use : edge.second) {
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
		    for (const repair_edge_use &retained : retained_uses) {
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
		const repair_edge_use &retained = retained_uses.front();
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
			add_component_edges(face);
		    }
		}
		continue;
	    }
	    bool boundary_ready = boundary.empty() || boundary.size() >= 3;
	    std::map<int, size_t> boundary_degree;
	    for (const repair_edge_use &edge : boundary) {
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
		add_component_edges(face);
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
		    for (const repair_edge_use &use : edge.second) {
			if (component_faces.find(use.face) !=
				component_faces.end()) {
			    incident = true;
			    break;
			}
		    }
		    if (!incident)
			continue;
		    std::string uses;
		    for (const repair_edge_use &use : edge.second) {
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
	std::map<int, size_t> boundary_indegree;
	std::map<int, size_t> boundary_outdegree;
	for (size_t edge_index = 0; edge_index < boundary.size();
		edge_index++) {
	    const repair_edge_use &edge = boundary[edge_index];
	    boundary_adjacency[edge.from].push_back({edge.to, edge_index});
	    boundary_adjacency[edge.to];
	    boundary_outdegree[edge.from]++;
	    boundary_indegree[edge.to]++;
	}
	for (const auto &vertex : boundary_adjacency) {
	    const size_t indegree = boundary_indegree[vertex.first];
	    const size_t outdegree = boundary_outdegree[vertex.first];
	    if (!indegree || indegree != outdegree) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary has directed degree %zu in/%zu out at vertex "
			"%d\n", boundary.size(), indegree, outdegree,
			vertex.first);
		return false;
	    }
	}
	/* Directed Hierholzer traversal preserves the orientation required by the
	 * retained neighbors while permitting weakly-simple boundaries that touch
	 * at a vertex.  The later topology-only split can separate pinched links
	 * without moving a point. */
	std::vector<bool> used_boundary(boundary.size(), false);
	std::vector<int> stack;
	std::vector<int> circuit;
	const int start = boundary.front().from;
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
	/* Split a weakly-simple Euler circuit at repeated vertices.  A separate
	 * center for each simple cycle prevents four or more fan triangles from
	 * sharing one center-to-boundary edge. */
	std::vector<std::vector<int>> rings;
	if (!repair_split_directed_euler_cycles(circuit, boundary.size(),
		rings)) {
	    if (debug_topology)
		bu_log("Degenerate neighborhood repair rejected: split directed "
		    "cycles do not cover %zu boundary edges\n", boundary.size());
	    return false;
	}

	const auto append_ring_patch = [&](const std::vector<int> &ring) {
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
	bool normal_available = average_normal.Unitize();
	if (!normal_available) {
	    ON_3dVector boundary_normal(0.0, 0.0, 0.0);
	    for (size_t point = 0; point < ring.size(); ++point) {
		const ON_3dPoint a(&input_vertices[
		    (size_t)ring[point] * 3]);
		const ON_3dPoint b(&input_vertices[
		    (size_t)ring[(point + 1) % ring.size()] * 3]);
		boundary_normal += ON_CrossProduct(a - center, b - center);
	    }
	    if (boundary_normal.Unitize()) {
		average_normal = boundary_normal;
		normal_available = true;
	    }
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
	ON_3dVector tangent(0.0, 0.0, 0.0);
	if (normal_available)
	    tangent = ON_CrossProduct(average_normal, axis);
	if (!tangent.Unitize()) {
	    /* A fully collinear neighborhood has no geometrically meaningful
	     * normal.  Prefer the direction away from retained neighboring
	     * vertices; if that also collapses, choose a stable perpendicular to
	     * the longest boundary edge.  The displacement remains bounded by the
	     * caller's deviation and the whole mesh is validated transactionally. */
	    tangent = average_outside - (average_outside * axis) * axis;
	}
	if (!tangent.Unitize()) {
	    const ON_3dVector reference =
		std::fabs(axis.x) <= std::fabs(axis.y) &&
		std::fabs(axis.x) <= std::fabs(axis.z) ?
		ON_3dVector(1.0, 0.0, 0.0) :
		(std::fabs(axis.y) <= std::fabs(axis.z) ?
		ON_3dVector(0.0, 1.0, 0.0) :
		ON_3dVector(0.0, 0.0, 1.0));
	    tangent = ON_CrossProduct(axis, reference);
	    if (!tangent.Unitize()) {
		if (debug_topology)
		    bu_log("Degenerate neighborhood repair rejected: %zu-edge "
			"boundary has no usable tangent\n", ring.size());
		return false;
	    }
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
	return true;
	};
	for (const std::vector<int> &ring : rings) {
	    if (!append_ring_patch(ring))
		return false;
	}
    }

    repair_replacement_mesh candidate;
    auto &candidate_faces = candidate.faces;
    auto &candidate_sources = candidate.sources;
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
    auto &candidate_vertices = candidate.vertices;
    candidate_vertices.assign(input_vertices,
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

    /* A closed degenerate island may account for every input triangle.  It
     * is safe to discard only when a nonempty retained mesh remains: an empty
     * face list vacuously passes edge checks, but is not a mesh repair result
     * and must never reach zero-size allocation or mesh validation APIs. */
    if (candidate_faces.empty()) {
	if (debug_topology)
	    bu_log("Degenerate neighborhood repair rejected: removing %d "
		"triangles would leave an empty mesh\n", input_face_count);
	return false;
    }

    const int sync_result = candidate.synchronize();
    if (debug_topology)
	bu_log("Degenerate neighborhood repair orientation sync result %d\n",
	    sync_result);
    int invalid_vertex = -1;
    if (!candidate.compact(invalid_vertex)) {
	if (debug_topology)
	    bu_log("Degenerate neighborhood repair rejected: replacement "
		"index %d outside %zu points\n", invalid_vertex, candidate.remap.size());
	return false;
    }
    const repair_replacement_validation checked(candidate);
    const auto &validation = checked.geometry;
    const auto &solid_errors = checked.edges;
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

	return false;
    }

    candidate.install(faces, face_count, vertices, vertex_count, source_faces);
    stats->components = (int)components.size();
    stats->removed_faces = (int)std::count(remove_face.begin(),
	remove_face.end(), true);
    return true;
}

static bool
repair_patch_triangle_normal(const ON_3dPoint points[3], ON_3dVector &cross)
{
    cross = ON_CrossProduct(points[1] - points[0], points[2] - points[0]);
    const double longest_squared = std::max(
	points[0].DistanceToSquared(points[1]), std::max(
	points[1].DistanceToSquared(points[2]),
	points[2].DistanceToSquared(points[0])));
    const double rounding_error_factor = 64.0;
    return longest_squared > 0.0 && cross.IsValid() &&
	!(cross.Length() <= rounding_error_factor *
	std::numeric_limits<double>::epsilon() * longest_squared);
}

/* The caller also reverses any parameter or point data it uses after this
 * operation.  Some builders deliberately sample the original corner order.
 */
static bool
repair_orient_triangle(int triangle[3], const ON_3dVector &cross,
	const ON_3dVector &normal)
{
    if (!(cross * normal < 0.0))
	return false;
    std::swap(triangle[1], triangle[2]);
    return true;
}

template <typename Evaluate>
static bool
repair_patch_triangle_deviation(const ON_3dPoint points[3],
	const Evaluate &evaluate, double &maximum)
{
    const double weights[4][3] = {
	{1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
	{0.5, 0.5, 0.0}, {0.0, 0.5, 0.5}, {0.5, 0.0, 0.5}
    };
    maximum = 0.0;
    for (const auto &weight : weights) {
	ON_3dPoint mesh_point(0.0, 0.0, 0.0);
	for (int corner = 0; corner < 3; ++corner) {
	    mesh_point.x += weight[corner] * points[corner].x;
	    mesh_point.y += weight[corner] * points[corner].y;
	    mesh_point.z += weight[corner] * points[corner].z;
	}
	double deviation = 0.0;
	if (!evaluate(weight, mesh_point, deviation))
	    return false;
	maximum = std::max(maximum, deviation);
    }
    return true;
}

static void
repair_append_patch_triangle(repair_boundary_patch &patch,
	const int triangle[3], double deviation)
{
    patch.faces.insert(patch.faces.end(), triangle, triangle + 3);
    patch.direct_surface_deviations.push_back(deviation);
}

namespace {

class repair_strip_parameterization {
public:
    repair_strip_parameterization(const ON_BrepFace &source_face,
	    int source_index, const std::vector<repair_fast_trim_sample> &first,
	    const std::vector<repair_fast_trim_sample> &second, int direction,
	    size_t shift, int &step, double deviation, bool debug)
	: face(source_face), surface(source_face.SurfaceOf()),
	  face_index(source_index), first_samples(first), second_samples(second),
	  first_segments(first.size() - 1), second_segments(second.size() - 1),
	  closed_direction(direction), best_shift(shift), best_step(step),
	  allowed_deviation(deviation), debug_strip(debug),
	  analytic_recognition_tolerance(std::min((double)BN_TOL_DIST,
	      0.25 * deviation)),
	  closed_domain(direction >= 0 ? surface->Domain(direction) :
	      ON_Interval(0.0, 1.0)),
	  closed_period(direction >= 0 ? closed_domain.Length() : 0.0) {}

    bool prepare(bool explicit_seam_topology);
    bool is_torus() const { return kind == representation::torus; }
    bool is_cylinder() const { return kind == representation::cylinder; }
    double recognition_deviation() const
    {
	return kind == representation::native ? 0.0 : analytic_recognition_tolerance;
    }
    size_t corresponding_second(size_t point) const;
    ON_2dPoint interior_uv(double phase, double across) const;
    ON_3dPoint point(double phase, double across, const ON_2dPoint &uv) const;
    bool normal(double phase, double across, const ON_2dPoint &uv,
	    ON_3dVector &normal, const char *&failure) const;
    const char *invalid_point_message() const
    {
	return is_torus() ? "invalid analytic torus sample" :
	    (is_cylinder() ? "invalid analytic cylinder sample" :
	    "invalid interior surface sample");
    }
    std::vector<ON_2dPoint> first_uv;
    std::vector<ON_2dPoint> second_uv;

private:
    enum class representation { native, torus, cylinder };
    representation kind = representation::native;
    const ON_BrepFace &face;
    const ON_Surface *surface;
    int face_index;
    const std::vector<repair_fast_trim_sample> &first_samples;
    const std::vector<repair_fast_trim_sample> &second_samples;
    size_t first_segments;
    size_t second_segments;
    int closed_direction;
    size_t best_shift;
    int &best_step;
    double allowed_deviation;
    bool debug_strip;
    double analytic_recognition_tolerance;
    ON_Interval closed_domain;
    double closed_period;
    ON_Torus strip_torus;
    ON_Cylinder strip_cylinder;
    std::vector<double> first_phase;
    std::vector<double> second_phase;
    double first_cross = 0.0;
    double second_cross = 0.0;
    bool torus_phase_is_major = false;
    double orientation_sign = 1.0;

    bool prepare_torus();
    bool prepare_cylinder();
    void prepare_uv();
    bool reject(const char *reason) const
    {
	if (debug_strip)
	    bu_log("Face %d: periodic strip rejected: %s\n", face_index, reason);
	return false;
    }
    static double unwrap_angle(double angle, double prior)
    {
	return angle + std::round((prior - angle) / (2.0 * ON_PI)) *
	    (2.0 * ON_PI);
    }
    static ON_2dPoint interpolate_uv(const std::vector<ON_2dPoint> &samples,
	    double phase);
    static double boundary_angle(const std::vector<double> &samples,
	    double phase);
    void torus_parameters(double phase, double across,
	    double &major, double &minor) const;
    void cylinder_parameters(double phase, double across,
	    double &angle, double &height) const;
    ON_2dPoint wrapped_uv(ON_2dPoint uv) const;
};

bool
repair_strip_parameterization::prepare(bool explicit_seam_topology)
{
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
	for (const auto *samples : {&first_samples, &second_samples}) {
	    for (const repair_fast_trim_sample &sample : *samples) {
		const ON_3dPoint uv_point = surface->PointAt(sample.uv[X],
		    sample.uv[Y]);
		if (!uv_point.IsValid() || uv_point.DistanceTo(
			ON_3dPoint(sample.point)) > allowed_deviation)
		    return reject("shared boundary misses the closed surface");
	    }
	}
    }

    kind = toroidal_seam_strip ? representation::torus :
	(cylindrical_seam_strip ? representation::cylinder : representation::native);
    if ((is_torus() && !prepare_torus()) ||
	    (is_cylinder() && !prepare_cylinder()))
	return false;
    prepare_uv();
    return true;
}

bool
repair_strip_parameterization::prepare_torus()
{
    std::vector<double> first_major, first_minor, second_major, second_minor;
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
	first_phase = torus_phase_is_major ? first_major :
	    first_minor;
	second_phase = torus_phase_is_major ? second_major :
	    second_minor;
	const std::vector<double> &first_cross_angles = torus_phase_is_major ?
	    first_minor : first_major;
	const std::vector<double> &second_cross_angles = torus_phase_is_major ?
	    second_minor : second_major;
	first_cross = std::accumulate(first_cross_angles.begin(),
	    first_cross_angles.end(), 0.0) / (double)first_cross_angles.size();
	second_cross = std::accumulate(second_cross_angles.begin(),
	    second_cross_angles.end(), 0.0) / (double)second_cross_angles.size();
	second_cross = unwrap_angle(second_cross,
	    first_cross);
	for (double angle : first_cross_angles) {
	    if (std::fabs(angle - first_cross) > angular_tolerance)
		return false;
	}
	for (double angle : second_cross_angles) {
	    angle = unwrap_angle(angle, second_cross);
	    if (std::fabs(angle - second_cross) > angular_tolerance)
		return false;
	}
	second_phase.front() = unwrap_angle(
	    second_phase.front(), first_phase.front());
	for (size_t point = 1; point < second_phase.size(); ++point)
	    second_phase[point] = unwrap_angle(
		second_phase[point], second_phase[point - 1]);
	return true;
    };
    if (!assign_torus_coordinates())
	return reject("second torus boundary is not the same isocircle type");
    double first_winding = first_phase.back() -
	first_phase.front();
    double second_winding = second_phase.back() -
	second_phase.front();
    if (first_winding * second_winding <= 0.0) {
	best_step = -best_step;
	if (!sample_torus_boundaries() || !assign_torus_coordinates())
	    return reject("opposite torus boundary traversal is unusable");
	first_winding = first_phase.back() -
	    first_phase.front();
	second_winding = second_phase.back() -
	    second_phase.front();
    }
    if (debug_strip)
	bu_log("Face %d: analytic torus candidate %s cross "
	    "%.17g/%.17g, phase windings %.17g/%.17g\n", face_index,
	    torus_phase_is_major ? "minor" : "major",
	    first_cross, second_cross, first_winding,
	    second_winding);
    if (std::fabs(std::fabs(first_winding) - 2.0 * ON_PI) >
	    0.1 * ON_PI || std::fabs(std::fabs(second_winding) -
	    2.0 * ON_PI) > 0.1 * ON_PI ||
	    first_winding * second_winding <= 0.0)
	return reject("torus boundary phases do not wind together");
    const double cross_span = second_cross - first_cross;
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
    orientation_sign = torus_normal * orientation_normal > 0.0 ?
	1.0 : -1.0;
    if (debug_strip)
	bu_log("Face %d: analytic torus strip cross span %.17g, "
	    "phase windings %.17g/%.17g\n", face_index, cross_span,
	    first_winding, second_winding);
    return true;
}

bool
repair_strip_parameterization::prepare_cylinder()
{
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

    std::vector<double> first_heights;
    std::vector<double> second_heights;
    const auto sample_cylinder_boundaries = [&]() {
	return cylinder_boundary_coordinates(first_segments,
	    [&](size_t phase) -> const repair_fast_trim_sample & {
		return first_samples[phase];
	    }, first_phase, first_heights) &&
	    cylinder_boundary_coordinates(second_segments,
	    [&](size_t phase) -> const repair_fast_trim_sample & {
		return second_samples[corresponding_second(phase)];
	    }, second_phase, second_heights);
    };
    const auto assign_cylinder_heights = [&]() {
	first_cross = std::accumulate(first_heights.begin(),
	    first_heights.end(), 0.0) / (double)first_heights.size();
	second_cross = std::accumulate(second_heights.begin(),
	    second_heights.end(), 0.0) / (double)second_heights.size();
	for (double height : first_heights) {
	    if (std::fabs(height - first_cross) >
		    analytic_recognition_tolerance)
		return false;
	}
	for (double height : second_heights) {
	    if (std::fabs(height - second_cross) >
		    analytic_recognition_tolerance)
		return false;
	}
	return true;
    };
    if (!sample_cylinder_boundaries() || !assign_cylinder_heights())
	return reject("shared boundary misses one cylinder isocircle");
    double first_winding = first_phase.back() -
	first_phase.front();
    double second_winding = second_phase.back() -
	second_phase.front();
    if (first_winding * second_winding <= 0.0) {
	best_step = -best_step;
	if (!sample_cylinder_boundaries() ||
		!assign_cylinder_heights())
	    return reject("opposite cylinder boundary traversal is unusable");
	first_winding = first_phase.back() -
	    first_phase.front();
	second_winding = second_phase.back() -
	    second_phase.front();
    }
    if (std::fabs(std::fabs(first_winding) - 2.0 * ON_PI) >
	    0.1 * ON_PI || std::fabs(std::fabs(second_winding) -
	    2.0 * ON_PI) > 0.1 * ON_PI ||
	    first_winding * second_winding <= 0.0)
	return reject("cylinder boundary phases do not wind together");
    if (std::fabs(second_cross - first_cross) <=
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
    orientation_sign = cylinder_normal * orientation_normal > 0.0 ?
	1.0 : -1.0;
    if (debug_strip)
	bu_log("Face %d: analytic cylinder strip heights %.17g/%.17g, "
	    "phase windings %.17g/%.17g\n", face_index,
	    first_cross, second_cross, first_winding,
	    second_winding);
    return true;
}

void
repair_strip_parameterization::prepare_uv()
{
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
    first_uv = unwrap_boundary(first_segments,
	[&](size_t phase) -> const repair_fast_trim_sample & {
	    return first_samples[phase];
	});
    second_uv = unwrap_boundary(second_segments,
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
}

ON_2dPoint
repair_strip_parameterization::interpolate_uv(const std::vector<ON_2dPoint> &samples,
	double phase)
{
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
}

double
repair_strip_parameterization::boundary_angle(const std::vector<double> &samples,
	double phase)
{
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
}

void
repair_strip_parameterization::torus_parameters(double phase, double across,
	double &major, double &minor) const
{
    const double phase_angle = (1.0 - across) *
	boundary_angle(first_phase, phase) + across *
	boundary_angle(second_phase, phase);
    const double cross_angle = (1.0 - across) * first_cross +
	across * second_cross;
    major = torus_phase_is_major ? phase_angle : cross_angle;
    minor = torus_phase_is_major ? cross_angle : phase_angle;
}

void
repair_strip_parameterization::cylinder_parameters(double phase, double across,
	double &angle, double &height) const
{
    angle = (1.0 - across) *
	boundary_angle(first_phase, phase) + across *
	boundary_angle(second_phase, phase);
    height = (1.0 - across) * first_cross +
	across * second_cross;
}

ON_2dPoint
repair_strip_parameterization::wrapped_uv(ON_2dPoint uv) const
{
    if (closed_direction < 0)
	return uv;
    double &coordinate = closed_direction ? uv.y : uv.x;
    coordinate = closed_domain.Min() + std::fmod(coordinate -
	closed_domain.Min(), closed_period);
    if (coordinate < closed_domain.Min())
	coordinate += closed_period;
    return uv;
}

size_t
repair_strip_parameterization::corresponding_second(size_t point) const
{
    const long signed_index = (long)best_shift +
	(long)best_step * (long)(point % second_segments);
    return (size_t)((signed_index % (long)second_segments +
	(long)second_segments) % (long)second_segments);
}

ON_2dPoint
repair_strip_parameterization::interior_uv(double phase, double across) const
{
    const ON_2dPoint lower = interpolate_uv(first_uv, phase);
    const ON_2dPoint upper = interpolate_uv(second_uv, phase);
    return ON_2dPoint((1.0 - across) * lower.x + across * upper.x,
	(1.0 - across) * lower.y + across * upper.y);
}

ON_3dPoint
repair_strip_parameterization::point(double phase, double across,
	const ON_2dPoint &uv) const
{
    if (is_torus()) {
	double major = 0.0, minor = 0.0;
	torus_parameters(phase, across, major, minor);
	return strip_torus.PointAt(major, minor);
    }
    if (is_cylinder()) {
	double angle = 0.0, height = 0.0;
	cylinder_parameters(phase, across, angle, height);
	return strip_cylinder.PointAt(angle, height);
    }
    const ON_2dPoint native_uv = wrapped_uv(uv);
    return surface->PointAt(native_uv.x, native_uv.y);
}

bool
repair_strip_parameterization::normal(double phase, double across,
	const ON_2dPoint &uv, ON_3dVector &surface_normal,
	const char *&failure) const
{
    if (is_torus()) {
	double major = 0.0, minor = 0.0;
	torus_parameters(phase, across, major, minor);
	surface_normal = orientation_sign * strip_torus.NormalAt(major, minor);
    } else if (is_cylinder()) {
	double angle = 0.0, height = 0.0;
	cylinder_parameters(phase, across, angle, height);
	surface_normal = orientation_sign *
	    (std::cos(angle) * strip_cylinder.circle.plane.xaxis +
	    std::sin(angle) * strip_cylinder.circle.plane.yaxis);
    } else {
	ON_3dPoint normal_point;
	const ON_2dPoint normal_uv = wrapped_uv(uv);
	if (!surface_EvNormal(surface, normal_uv.x, normal_uv.y,
		normal_point, surface_normal) || !surface_normal.IsValid()) {
	    failure = "invalid interior surface normal";
	    return false;
	}
	if (face.m_bRev)
	    surface_normal.Reverse();
    }
    if (!surface_normal.IsValid()) {
	failure = "invalid reconstructed surface normal";
	return false;
    }
    return true;
}

} // namespace

/* A failed periodic side face may still have two authoritative closed edge
 * loops supplied by rigorous neighboring faces.  Join those loops in a
 * synthetic rectangular chart while keeping their exact shared 3-D samples.
 * If the boundary-only chords do not meet the requested fidelity, insert
 * bounded rows evaluated directly on the failed source surface.  This avoids
 * projecting a twisted 3-D patch onto one plane while retaining the source
 * face's seam topology.  General holed or partly constrained faces remain the
 * responsibility of the other bounded recovery paths. */
bool
repair_constrained_periodic_strip(struct ON_Brep_CDT_State *s_cdt,
	int face_index, const repair_fast_constraint_store &constraints,
	size_t max_points, fastf_t allowed_deviation, int64_t deadline,
	repair_boundary_patch &patch, int required_step,
	size_t required_shift)
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
    const bool separate_loops = face.LoopCount() == 2;
    /* Surface conditioning can shrink away the periodic image between two
     * essential loops.  Reconstruct on the original support in that case. */
    const ON_BrepFace &support_face = separate_loops && s_cdt->orig_brep &&
	face_index < s_cdt->orig_brep->m_F.Count() ?
	s_cdt->orig_brep->m_F[face_index] : face;
    const ON_Surface *surface = support_face.SurfaceOf();
    const char *strip_debug_setting = getenv("BRLCAD_CDT_DEBUG_STRIP");
    const bool debug_strip = strip_debug_setting && strip_debug_setting[0] &&
	!BU_STR_EQUAL(strip_debug_setting, "0");
    const auto reject = [&](const char *reason) {
	if (debug_strip)
	    bu_log("Face %d: periodic strip rejected: %s\n", face_index,
		reason);
	return false;
    };
    if (!outer || !surface || (face.LoopCount() != 1 && !separate_loops) ||
	    (!separate_loops && outer->TrimCount() < 4))
	return reject("not a bounded periodic strip");

    struct boundary_path {
	std::vector<repair_fast_trim_sample> samples;
	size_t constrained_edges = 0;
	size_t constrained_samples = 0;
    };
    std::vector<boundary_path> boundaries;
    std::vector<const ON_BrepTrim *> seam_boundaries;
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
    const auto collect_loop = [&](const ON_BrepLoop &loop) {
	for (int trim_index = 0; trim_index < loop.TrimCount(); ++trim_index) {
	    const ON_BrepTrim *trim = loop.Trim(trim_index);
	    if (!trim)
		return reject("missing boundary trim");
	    const auto constrained = constraints.trims.find(trim->m_trim_index);
	    if (constrained != constraints.trims.end()) {
		const ON_BrepEdge *edge = trim->Edge();
		if (!edge || constrained->second.size() < 2)
		    return reject("constrained boundary edge is not fully sampled");
		if (!prior_constrained)
		    boundaries.push_back(boundary_path());
		const auto *samples = &constrained->second;
		std::vector<repair_fast_trim_sample> native_samples;
		if (separate_loops) {
		    /* Shared CDT vertices may use another periodic image.  Recover
		     * each trim's native UV from its own parameter. */
		    native_samples = *samples;
		    for (repair_fast_trim_sample &sample : native_samples) {
			const ON_3dPoint uv = trim->PointAt(sample.parameter);
			if (!uv.IsValid())
			    return reject("invalid native boundary parameter");
			V2SET(sample.uv, uv.x, uv.y);
		    }
		    samples = &native_samples;
		}
		if (!append_path(boundaries.back(), *samples, 1,
			constrained->second.size()))
		    return reject("adjacent constrained boundary edges do not meet");
		prior_constrained = true;
		first_constrained = first_constrained || trim_index == 0;
		last_constrained = trim_index + 1 == loop.TrimCount();
		continue;
	    }
	    if (separate_loops || trim->m_type != ON_BrepTrim::seam)
		return reject("unconstrained trim is not a seam");
	    seam_boundaries.push_back(trim);
	    prior_constrained = false;
	    last_constrained = false;
	}
	return true;
    };
    if (!collect_loop(*outer))
	return false;
    if (separate_loops) {
	const ON_BrepLoop *inner = face.Loop(0) == outer ? face.Loop(1) :
	    face.Loop(0);
	if (!inner || inner->m_type != ON_BrepLoop::inner || boundaries.size() != 1)
	    return reject("periodic strip lacks one inner boundary");
	prior_constrained = false;
	if (!collect_loop(*inner))
	    return false;
	/* A collapsed trim can repeat an authoritative endpoint.  Keep distinct
	 * source identities, but do not turn the same endpoint into a chart edge. */
	for (boundary_path &boundary : boundaries) {
	    auto &samples = boundary.samples;
	    samples.erase(std::unique(samples.begin(), samples.end(),
		[](const repair_fast_trim_sample &first,
		    const repair_fast_trim_sample &second) {
		    return first.source_point == second.source_point &&
			    repair_point_key{first.point[X], first.point[Y], first.point[Z]} ==
			    repair_point_key{second.point[X], second.point[Y], second.point[Z]};
		}), samples.end());
	}
    }
    if (!separate_loops && first_constrained && last_constrained &&
	    boundaries.size() > 1) {
	boundary_path joined = std::move(boundaries.back());
	if (!append_path(joined, boundaries.front().samples,
		boundaries.front().constrained_edges,
		boundaries.front().constrained_samples))
	    return reject("wrapped constrained boundary edges do not meet");
	boundaries.front() = std::move(joined);
	boundaries.pop_back();
    }
    if (boundaries.size() != 2 || (!separate_loops && seam_boundaries.size() < 2))
	return reject("topology is not a two-boundary periodic strip");

    int closed_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const double period = surface->Domain(direction).Length();
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period))
	    continue;
	bool spans_period = true;
	double first_winding = 0.0;
	for (size_t boundary_index = 0; boundary_index < boundaries.size();
		++boundary_index) {
	    const std::vector<repair_fast_trim_sample> &samples =
		boundaries[boundary_index].samples;
	    double winding = samples.back().uv[direction] -
		samples.front().uv[direction];
	    if (separate_loops) {
		/* A closed ring may start and end in the same periodic copy. */
		winding = 0.0;
		for (size_t point = 1; point < samples.size(); ++point) {
		    double delta = samples[point].uv[direction] -
			samples[point - 1].uv[direction];
		    delta -= std::round(delta / period) * period;
		    winding += delta;
		}
	    }
	    if (separate_loops) {
		const double tolerance = cdt_surface_parameter_tolerance(
		    surface->Domain(direction));
		spans_period = spans_period && std::fabs(std::fabs(winding) -
		    period) <= tolerance;
		if (!boundary_index)
		    first_winding = winding;
		else
		    spans_period = spans_period && first_winding * winding < 0.0;
	    } else {
		spans_period = spans_period && std::fabs(winding) >= 0.5 * period;
	    }
	    if (debug_strip)
		bu_log("Face %d: periodic strip direction %d boundary winding "
		    "%.17g periods (%zu samples, endpoints %.17g/%.17g)\n",
		    face_index, direction, winding / period, samples.size(),
		    samples.front().uv[direction], samples.back().uv[direction]);
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
	/* Opposed native windings must traverse the strip in the same phase
	 * direction.  A distance-only match can twist the periodic interior. */
	if (separate_loops && step != -1)
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
	return reject("no compatible closed boundary correspondence");
    if (debug_strip)
	bu_log("Face %d: periodic strip correspondence shift %zu, step %d, "
	    "score %.17g, explicit seam %d\n", face_index, best_shift,
	    best_step, (double)best_score, explicit_seam_topology);

    repair_strip_parameterization parameterization(support_face, face_index,
	first_samples, second_samples, closed_direction, best_shift,
	best_step, allowed_deviation, debug_strip);
    if (!parameterization.prepare(explicit_seam_topology))
	return false;

    if (separate_loops && surface->IsClosed(1 - closed_direction)) {
	/* The outer loop's directed winding selects the band on its left.
	 * Nearest periodic copies can select the complementary region instead. */
	const double winding = parameterization.first_uv.back()[closed_direction] -
	    parameterization.first_uv.front()[closed_direction];
	const int side = (closed_direction ? -1 : 1) * (winding > 0.0 ? 1 : -1);
	const double period = surface->Domain(1 - closed_direction).Length();
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period))
	    return reject("invalid transverse period");
	const double first = parameterization.first_uv.front()[1 - closed_direction];
	const double second = parameterization.second_uv.front()[1 - closed_direction];
	double width = std::fmod(side * (second - first), period);
	if (width < 0.0)
	    width += period;
	if (!(width > ON_ZERO_TOLERANCE) || !std::isfinite(width))
	    return reject("periodic strip has no transverse width");
	const double shift = first + side * width - second;
	for (ON_2dPoint &uv : parameterization.second_uv)
	    uv[1 - closed_direction] += shift;
    }

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
	    source_uv.push_back(parameterization.first_uv[point]);
	    source_points.emplace_back(first_samples[point].point);
	    point_sources.push_back(first_samples[point].source_point);
	}
	for (size_t point = second_segments + 1; point > 0; --point) {
	    const size_t phase_index = point - 1;
	    const double phase = (double)phase_index /
		(double)second_segments;
	    chart_points.push_back({phase, 1.0});
	    source_uv.push_back(parameterization.second_uv[phase_index]);
	    source_points.emplace_back(second_samples[
		parameterization.corresponding_second(phase_index)].point);
	    point_sources.push_back(second_samples[
		parameterization.corresponding_second(phase_index)].source_point);
	}
	for (size_t row = 1; row <= interior_rows; ++row) {
	    const double y = (double)row / (double)(interior_rows + 1);
	    const size_t row_start = source_points.size();
	    for (double phase : interior_phases) {
		const ON_2dPoint uv = parameterization.interior_uv(phase, y);
		chart_points.push_back({phase, y});
		source_uv.push_back(uv);
		if (phase >= 1.0 && source_points.size() > row_start) {
		    source_points.push_back(source_points[row_start]);
		    point_sources.push_back(NULL);
		    continue;
		}
		const ON_3dPoint surface_point = parameterization.point(phase, y, uv);
		if (!surface_point.IsValid()) {
		    strip_build_failure = parameterization.invalid_point_message();
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
		ON_3dVector cross;
		if (!repair_patch_triangle_normal(points, cross)) {
		    strip_build_failure = "geometrically degenerate triangle";
		    valid = false;
		    return;
		}
		ON_3dVector surface_normal;
		if (!parameterization.normal(centroid_chart.x, centroid_chart.y,
			centroid_uv, surface_normal, strip_build_failure)) {
		    valid = false;
		    return;
		}
		repair_orient_triangle(local, cross, surface_normal);

		double triangle_deviation = 0.0;
		const auto sample_deviation = [&](const double weights[3],
			const ON_3dPoint &mesh_point, double &deviation) {
		    ON_2dPoint uv(0.0, 0.0);
		    ON_2dPoint chart_sample(0.0, 0.0);
		    for (int corner = 0; corner < 3; ++corner) {
			uv.x += weights[corner] *
			    source_uv[(size_t)chart[corner]].x;
			uv.y += weights[corner] *
			    source_uv[(size_t)chart[corner]].y;
			chart_sample.x += weights[corner] *
			    chart_points[(size_t)chart[corner]].x;
			chart_sample.y += weights[corner] *
			    chart_points[(size_t)chart[corner]].y;
		    }
		    const ON_3dPoint surface_point = parameterization.point(
			chart_sample.x, chart_sample.y, uv);
		    if (!surface_point.IsValid()) {
			strip_build_failure = "invalid surface deviation sample";
			return false;
		    }
		    deviation = mesh_point.DistanceTo(surface_point);
		    if (parameterization.is_torus() || parameterization.is_cylinder())
			deviation += parameterization.recognition_deviation();
		    return true;
		};
		if (!repair_patch_triangle_deviation(points, sample_deviation,
			triangle_deviation)) {
		    valid = false;
		    return;
		}
		maximum_deviation = std::max(maximum_deviation,
		    triangle_deviation);
		repair_append_patch_triangle(candidate, local, triangle_deviation);
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
	candidate.analytic_torus_strip = parameterization.is_torus();
	candidate.analytic_cylinder_strip = parameterization.is_cylinder();
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
	    if (debug_strip)
		bu_log("Face %d: periodic strip refinement failed with %zu "
		    "interior rows: %s\n", face_index, interior_rows,
		    strip_build_failure ? strip_build_failure : "unknown reason");
	    if (!interior_rows)
		return reject("chart triangulation failed");
	    break;
	}
	if (debug_strip)
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
	candidate.source_points.assign(points.size(), NULL);
	for (size_t point = 0; point < segments; ++point)
	    candidate.source_points[point] = boundary[point].source_point;
	const auto append_triangle = [&](int first, int second, int third) {
	    int triangle[3] = {first, second, third};
	    ON_3dPoint triangle_points[3] = {points[(size_t)first],
		points[(size_t)second], points[(size_t)third]};
	    ON_3dVector cross;
	    if (!repair_patch_triangle_normal(triangle_points, cross))
		return false;
	    const ON_3dPoint centroid = (triangle_points[0] +
		triangle_points[1] + triangle_points[2]) / 3.0;
	    const ON_3dVector expected = orientation_sign *
		(centroid - center);
	    if (!expected.IsValid() || expected.Length() <= ON_ZERO_TOLERANCE)
		return false;
	    if (repair_orient_triangle(triangle, cross, expected))
		std::swap(triangle_points[1], triangle_points[2]);
	    double triangle_deviation = 0.0;
	    repair_patch_triangle_deviation(triangle_points,
		[&](const double *, const ON_3dPoint &mesh_point,
			double &deviation) {
		    deviation = std::fabs(mesh_point.DistanceTo(center) - radius) +
			recognized_surface_deviation;
		    return true;
		}, triangle_deviation);
	    maximum_deviation = std::max(maximum_deviation,
		triangle_deviation);
	    repair_append_patch_triangle(candidate, triangle, triangle_deviation);
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
	    first_points + second_points - segments > max_points ||
	    first.source_points.size() != first_points ||
	    second.source_points.size() != second_points)
	return false;

    const size_t interior_rows = std::max(first.strip_interior_rows,
	second.strip_interior_rows);
    patch = std::move(first);
    patch.source_points.reserve(first_points + second_points - segments);
    std::vector<int> second_to_patch(second_points, -1);
    for (size_t point = 0; point < segments; ++point)
	second_to_patch[point] = (int)point;
    for (size_t point = segments; point < second_points; ++point) {
	second_to_patch[point] = (int)(patch.vertices.size() / 3);
	patch.vertices.insert(patch.vertices.end(),
	    second.vertices.begin() + (ptrdiff_t)(point * 3),
	    second.vertices.begin() + (ptrdiff_t)(point * 3 + 3));
	patch.source_points.push_back(second.source_points[point]);
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
	patch.direct_surface_deviations.size() == patch.faces.size() / 3 &&
	patch.source_points.size() == patch.vertices.size() / 3;
}

bool
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

bool
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
bool
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
		ON_3dPoint triangle_points[3] = {
		    points[(size_t)triangle[0]], points[(size_t)triangle[1]],
		    points[(size_t)triangle[2]]
		};
		ON_3dVector cross;
		if (!repair_patch_triangle_normal(triangle_points, cross))
		    return false;
		if (repair_orient_triangle(triangle, cross, surface_normal)) {
		    std::swap(triangle_uv[1][0], triangle_uv[2][0]);
		    std::swap(triangle_uv[1][1], triangle_uv[2][1]);
		    std::swap(triangle_points[1], triangle_points[2]);
		}
		double direct_deviation = 0.0;
		const auto sample_deviation = [&](const double weights[3],
			const ON_3dPoint &mesh_sample, double &deviation) {
		    double uv[2] = {0.0, 0.0};
		    for (int corner = 0; corner < 3; ++corner) {
			uv[0] += weights[corner] * triangle_uv[corner][0];
			uv[1] += weights[corner] * triangle_uv[corner][1];
		    }
		    const ON_3dPoint source_sample = surface->PointAt(uv[0], uv[1]);
		    if (!source_sample.IsValid())
			return false;
		    deviation = mesh_sample.DistanceTo(source_sample);
		    return true;
		};
		if (!repair_patch_triangle_deviation(triangle_points,
			sample_deviation, direct_deviation))
		    return false;
		if (direct_deviation > allowed_deviation)
		    return false;
		repair_append_patch_triangle(patch, triangle, direct_deviation);
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
repair_projected_disk_contract(void)
{
    /* The mean of this U-shaped boundary lies in the missing notch.  Test
     * tilted planes, both windings, and every coordinate-axis orientation. */
    const double outline[8][2] = {
	{0, 0}, {4, 0}, {4, 4}, {3, 4},
	{3, 1}, {1, 1}, {1, 4}, {0, 4}
    };
    constexpr double slope_x = 0.125;
    constexpr double slope_y = 0.25;
    const double expected_area = 10.0 *
	std::sqrt(1.0 + slope_x * slope_x + slope_y * slope_y);
    for (int axis = 0; axis < 3; ++axis) {
	for (bool reverse : {false, true}) {
	    std::vector<ON_3dPoint> boundary;
	    ON_3dPoint center(0.0, 0.0, 0.0);
	    for (const auto &point : outline) {
		ON_3dPoint transformed;
		transformed[axis] = point[0];
		transformed[(axis + 1) % 3] = point[1];
		transformed[(axis + 2) % 3] =
		    slope_x * point[0] + slope_y * point[1];
		boundary.push_back(transformed);
		center += transformed;
	    }
	    center /= (double)boundary.size();
	    if (reverse)
		std::reverse(boundary.begin(), boundary.end());
	    std::vector<int> faces;
	    if (!repair_projected_disk(boundary, center, faces) ||
		    faces.size() / 3 != boundary.size() - 2)
		return 1;
	    double area = 0.0;
	    std::map<std::pair<int, int>, size_t> edge_uses;
	    std::map<std::pair<int, int>, int> edge_directions;
	    for (size_t face = 0; face < faces.size() / 3; ++face) {
		ON_3dPoint points[3];
		for (int corner = 0; corner < 3; ++corner) {
		    const int vertex = faces[face * 3 + corner];
		    const int next = faces[face * 3 + (corner + 1) % 3];
		    if (vertex < 0 || (size_t)vertex >= boundary.size())
			return 2;
		    points[corner] = boundary[(size_t)vertex];
		    edge_uses[std::minmax(vertex, next)]++;
		    edge_directions[std::minmax(vertex, next)] +=
			vertex < next ? 1 : -1;
		}
		if (assembled_mesh_triangle_degenerate(points))
		    return 3;
		area += 0.5 * ON_CrossProduct(points[1] - points[0],
		    points[2] - points[0]).Length();
	    }
	    constexpr double area_roundoff_factor = 64.0;
	    if (std::fabs(area - expected_area) > area_roundoff_factor *
		    std::numeric_limits<double>::epsilon() * expected_area)
		return 4;
	    for (size_t vertex = 0; vertex < boundary.size(); ++vertex) {
		const std::pair<int, int> edge = std::minmax((int)vertex,
		    (int)((vertex + 1) % boundary.size()));
		const int direction = vertex + 1 < boundary.size() ? 1 : -1;
		if (edge_uses[edge] != 1 || edge_directions[edge] != direction)
		    return 5;
		edge_uses.erase(edge);
		edge_directions.erase(edge);
	    }
	    for (const auto &edge : edge_uses) {
		if (edge.second != 2 || edge_directions[edge.first] != 0)
		    return 6;
	    }
	}
    }
    const std::vector<ON_3dPoint> square = {
	ON_3dPoint(0, 0, 0), ON_3dPoint(1, 0, 0),
	ON_3dPoint(1, 1, 0), ON_3dPoint(0, 1, 0)
    };
    std::vector<int> unchanged = {0, 1, 2};
    if (repair_projected_disk(square, ON_3dPoint(0.5, 0.5, 0.0),
	    unchanged) || unchanged != std::vector<int>({0, 1, 2}))
	return 7;
    std::vector<ON_3dPoint> crossed = square;
    std::swap(crossed[1], crossed[2]);
    if (repair_projected_disk(crossed, ON_3dPoint(0.5, 0.5, 0.0),
	    unchanged) || unchanged != std::vector<int>({0, 1, 2}))
	return 8;
    /* The retained triangle already uses the square's new diagonal.  Its
     * endpoint indices must not become a four-face edge after assembly. */
    std::vector<fastf_t> vertices = {
	0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1
    };
    const std::vector<int> retained = {0, 2, 4};
    std::vector<int> patch = {0, 1, 2, 0, 2, 3};
    if (!repair_split_conflicting_patch_edges(patch, vertices, retained) ||
	    vertices.size() / 3 != 6 || patch.size() / 3 != 4 ||
	    !NEAR_EQUAL(vertices[15], 0.5, ON_ZERO_TOLERANCE) ||
	    !NEAR_EQUAL(vertices[16], 0.5, ON_ZERO_TOLERANCE) ||
	    !NEAR_ZERO(vertices[17], ON_ZERO_TOLERANCE))
	return 9;
    std::set<std::pair<int, int>> patch_boundary;
    const std::set<std::pair<int, int>> expected = {
	{0, 1}, {1, 2}, {2, 3}, {0, 3}
    };
    if (!repair_patch_indexed_boundary(patch.data(), (int)(patch.size() / 3),
	    vertices.size() / 3, patch_boundary) || patch_boundary != expected)
	return 10;
    for (size_t face = 0; face < patch.size() / 3; ++face) {
	for (int corner = 0; corner < 3; ++corner) {
	    const std::pair<int, int> edge = std::minmax(
		patch[face * 3 + corner], patch[face * 3 + (corner + 1) % 3]);
	    if (edge == std::make_pair(0, 2))
		return 11;
	}
    }
    return 0;
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
    const int projected_contract = repair_projected_disk_contract();
    if (projected_contract)
	return 30 + projected_contract;
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
    std::vector<ON_3dPoint> cap_source_points(cap_boundary.size());
    const double cap_plane = -0.2;
    const double cap_radius = std::sqrt(1.0 - cap_plane * cap_plane);
    for (size_t point = 0; point < cap_boundary.size(); ++point) {
	const double angle = 2.0 * ON_PI * (double)point /
	    (double)cap_boundary.size();
	VSET(cap_boundary[point].point, cap_radius * std::cos(angle),
	    cap_plane, cap_radius * std::sin(angle));
	cap_source_points[point] = ON_3dPoint(cap_boundary[point].point);
	cap_boundary[point].source_point = &cap_source_points[point];
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
	cap_patch.faces.size() / 3 && cap_patch.source_points.size() ==
	cap_patch.vertices.size() / 3;
    /* Assembly must reuse the exact rigorous rim, while generated interior
     * vertices must not claim a source identity. */
    for (size_t point = 0; valid && point < cap_patch.source_points.size(); ++point) {
	const ON_3dPoint *expected = point < cap_source_points.size() ?
	    &cap_source_points[point] : NULL;
	valid = cap_patch.source_points[point] == expected;
	if (expected)
	    valid = ON_3dPoint(&cap_patch.vertices[point * 3]).DistanceTo(
		*expected) <= ON_ZERO_TOLERANCE;
    }
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
	full_sphere_patch.source_points.size() ==
	(size_t)full_sphere_points &&
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
    /* Adding a vertex can relocate m_V, invalidating earlier references. */
    const int low_vertex_index = closed_sphere_brep->NewVertex(
	closed_sphere_surface->PointAt(low_uv.x, low_uv.y), 1.0e-8).m_vertex_index;
    const int high_vertex_index = closed_sphere_brep->NewVertex(
	closed_sphere_surface->PointAt(high_uv.x, high_uv.y), 1.0e-8).m_vertex_index;
    ON_BrepVertex &low_vertex = closed_sphere_brep->m_V[low_vertex_index];
    ON_BrepVertex &high_vertex = closed_sphere_brep->m_V[high_vertex_index];
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
    const auto test_torus_strip = [&](ON_Brep &strip_brep,
	    bool analytic, double expected_area) {
	const int boundary_segments = 32;
	const size_t point_limit = 8192;
	const fastf_t deviation_limit = 0.06;
	const double area_relative_tolerance = 0.03;
	struct ON_Brep_CDT_State *strip_state = ON_Brep_CDT_Create(
	    &strip_brep, "torus strip repair contract");
	if (strip_state)
	    strip_state->brep = new ON_Brep(strip_brep);
	if (!strip_state || !strip_state->brep ||
		strip_state->brep->m_F.Count() != 1) {
	    ON_Brep_CDT_Destroy(strip_state);
	    return false;
	}
	/* Match the conditioning used before rigorous triangulation. */
	if (!analytic)
	    strip_state->brep->ShrinkSurface(strip_state->brep->m_F[0]);
	const ON_BrepFace &strip_face = strip_state->brep->m_F[0];
	repair_fast_constraint_store strip_constraints;
	strip_constraints.face_index = 0;
	for (int trim_index = 0; trim_index < strip_state->brep->m_T.Count(); ++trim_index) {
	    const ON_BrepTrim *trim = &strip_state->brep->m_T[trim_index];
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (!trim || !edge || !edge->IsClosed() ||
		    trim->m_type == ON_BrepTrim::seam)
		continue;
	    std::vector<repair_fast_trim_sample> &samples =
		strip_constraints.trims[trim->m_trim_index];
	    const ON_Interval domain = trim->Domain();
	    for (int sample_index = 0; sample_index <= boundary_segments; ++sample_index) {
		repair_fast_trim_sample sample;
		sample.parameter = domain.ParameterAt(
		    (double)sample_index / boundary_segments);
		const ON_2dPoint uv = trim->PointAt(sample.parameter);
		const ON_3dPoint point = strip_face.SurfaceOf()->PointAt(
		    uv.x, uv.y);
		V2SET(sample.uv, uv.x, uv.y);
		VSET(sample.point, point.x, point.y, point.z);
		/* Require source trim parameters to recover periodic winding. */
		if (!analytic)
		    V2SET(sample.uv, 0.0, 0.0);
		if (sample_index == boundary_segments)
		    VMOVE(sample.point, samples.front().point);
		samples.push_back(sample);
	    }
	    strip_constraints.constrained_edges++;
	    strip_constraints.constrained_samples += samples.size();
	}
	repair_boundary_patch strip_patch;
	const bool strip_reconstructed = repair_constrained_periodic_strip(
	    strip_state, 0, strip_constraints, point_limit, deviation_limit, 0,
	    strip_patch);
	bool strip_valid = strip_reconstructed &&
	    strip_patch.analytic_torus_strip == analytic &&
	    strip_patch.constrained_edges == 2 &&
	    strip_patch.constrained_samples == 2 * (size_t)(boundary_segments + 1) &&
	    strip_patch.direct_surface_deviations.size() ==
	    strip_patch.faces.size() / 3;
	for (double deviation : strip_patch.direct_surface_deviations)
	    strip_valid = strip_valid && std::isfinite(deviation) &&
		deviation <= deviation_limit;
	if (expected_area > 0.0) {
	    const double area = bg_trimesh_area(strip_patch.faces.data(),
		strip_patch.faces.size() / 3, (const point_t *)strip_patch.vertices.data(),
		strip_patch.vertices.size() / 3);
	    strip_valid = strip_valid && std::fabs(area - expected_area) <=
		area_relative_tolerance * expected_area;
	}
	ON_Brep_CDT_Destroy(strip_state);
	return strip_valid;
    };
    const auto test_explicit_torus_strip = [&](ON_RevSurface *source_surface) {
	std::unique_ptr<ON_Brep> strip_brep(ON_BrepRevSurface(source_surface,
	    false, false, NULL));
	if (!strip_brep) {
	    delete source_surface;
	    return false;
	}
	return test_torus_strip(*strip_brep, true, 0.0);
    };
    ON_RevSurface *minor_circle_strip = new ON_RevSurface();
    minor_circle_strip->m_angle.Set(0.0, 0.5 * ON_PI);
    minor_circle_strip->m_t = minor_circle_strip->m_angle;
    minor_circle_strip->m_curve = new ON_ArcCurve(minor);
    minor_circle_strip->m_axis.from = torus.plane.origin;
    minor_circle_strip->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    minor_circle_strip->m_bTransposed = false;
    if (!test_explicit_torus_strip(minor_circle_strip))
	return 11;

    ON_RevSurface *major_circle_strip = new ON_RevSurface();
    major_circle_strip->m_angle.Set(0.0, 2.0 * ON_PI);
    major_circle_strip->m_t = major_circle_strip->m_angle;
    major_circle_strip->m_curve = new ON_ArcCurve(ON_Arc(minor,
	ON_Interval(-0.4, 0.7)));
    major_circle_strip->m_axis.from = torus.plane.origin;
    major_circle_strip->m_axis.to = torus.plane.origin + torus.plane.zaxis;
    major_circle_strip->m_bTransposed = false;
    if (!test_explicit_torus_strip(major_circle_strip))
	return 12;

    for (bool reverse : {false, true}) {
	for (bool transpose : {false, true}) {
	    ON_Brep strip_brep;
	    ON_RevSurface *strip_surface = new ON_RevSurface();
	    strip_surface->m_angle.Set(0.0, 2.0 * ON_PI);
	    strip_surface->m_t = strip_surface->m_angle;
	    strip_surface->m_curve = new ON_ArcCurve(minor);
	    strip_surface->m_axis.from = torus.plane.origin;
	    strip_surface->m_axis.to = torus.plane.origin + torus.plane.zaxis;
	    strip_surface->m_bTransposed = transpose;
	    ON_BrepFace &strip_face = strip_brep.NewFace(
		strip_brep.AddSurface(strip_surface));
	    const int winding_direction = transpose ? 0 : 1;
	    const ON_Interval cross_domain = strip_surface->Domain(1 - winding_direction);
	    const ON_Interval winding_domain = strip_surface->Domain(winding_direction);
	    const auto uv_point = [transpose](double u, double v) {
		return transpose ? ON_2dPoint(v, u) : ON_2dPoint(u, v);
	    };
	    for (int boundary = 0; boundary < 2; ++boundary) {
		ON_BrepLoop &loop = strip_brep.NewLoop(boundary ?
		    ON_BrepLoop::inner : ON_BrepLoop::outer, strip_face);
		const double u = cross_domain.ParameterAt(boundary ? 0.75 : 0.0);
		const bool decreasing = ((boundary != 0) != reverse) != transpose;
		const ON_2dPoint start = uv_point(u, decreasing ?
		    winding_domain.Max() : winding_domain.Min());
		const ON_2dPoint end = uv_point(u, decreasing ?
		    winding_domain.Min() : winding_domain.Max());
		const int vertex = strip_brep.NewVertex(strip_surface->PointAt(
		    start.x, start.y)).m_vertex_index;
		ON_Curve *curve = strip_surface->IsoCurve(winding_direction, u);
		if (!curve)
		    return 16;
		if (decreasing)
		    curve->Reverse();
		ON_BrepEdge &edge = strip_brep.NewEdge(strip_brep.m_V[vertex],
		    strip_brep.m_V[vertex], strip_brep.AddEdgeCurve(curve));
		ON_BrepTrim &trim = strip_brep.NewTrim(edge, false, loop,
		    strip_brep.AddTrimCurve(new ON_LineCurve(start, end)));
		trim.m_type = ON_BrepTrim::boundary;
	    }
	    const double fraction = reverse ? 0.75 : 0.25;
	    const double expected_area = fraction * 4.0 * ON_PI * ON_PI *
		torus.MajorRadius() * torus.MinorRadius();
	    if (!test_torus_strip(strip_brep, false, expected_area))
		return 17;
	}
    }

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

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
