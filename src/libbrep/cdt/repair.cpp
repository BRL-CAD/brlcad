/*                    R E P A I R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */
/** @file cdt/repair.cpp
 *
 * Bounded B-Rep mesh recovery, fidelity validation, and provenance.
 */

#include "common.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "bu/str.h"
#include "bg/tri_pt.h"
#include "brep/surfacetree.h"
#include "cdt.h"
#include "assembly.h"
#include "repair_boundary.h"
#include "heal.h"
#include "surface.h"

#define MAX_BEST_EFFORT_FOLD_DIVISOR 20

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
    for (const chart_triangle_t &native : mesh->tris_2d) {
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
	const cdt_face_chart *chart = mesh->triangle_chart(native);
	ON_2dPoint sample = ON_2dPoint::UnsetPoint;
	if (!chart || !chart->triangle_interior_sample(
		native.chart_vertices.data(), sample))
	    continue;
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
	    const int chart_edge[2] = {
		native.chart_vertices[edge], native.chart_vertices[(edge + 1) % 3]};
	    ON_2dPoint edge_sample = ON_2dPoint::UnsetPoint;
	    ON_2dPoint chart_sample = ON_2dPoint::UnsetPoint;
	    if (!chart->edge_midpoint_sample(chart_edge, edge_sample, chart_sample))
		continue;
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
    if (!cdt_rebuild_repair_face(s_cdt, mesh)) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_CERTIFICATION_FAILED,
	    BREP_CDT_STAGE_FACE_TRIANGULATION, face_index, 0, 1,
	    "locally reconstructed face did not pass mesh validation");
	return false;
    }
    size_t inserted_points = 0;
    int refinement_rounds = 0;
    double maximum_deviation = 0.0;
    while (refinement_rounds < CDT_MAX_CHART_REFINEMENT_ATTEMPTS &&
	    inserted_points < CDT_MAX_CHART_REFINEMENT_POINTS) {
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
	    CDT_MAX_CHART_REFINEMENT_POINTS - inserted_points);
	if (!inserted)
	    inserted = mesh->refine_problem_triangles(problematic,
		CDT_MAX_CHART_REFINEMENT_POINTS - inserted_points);
	if (dump_refinement)
	    bu_log("Face %d: local surface refinement round %d inserted %zu "
		"points\n", face_index, refinement_rounds + 1, inserted);
	if (!inserted)
	    break;
	inserted_points += inserted;
	refinement_rounds++;
	if (!cdt_rebuild_repair_face(s_cdt, mesh))
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

/* Fidelity samples on adjacent triangles share edge midpoints.  Keep their
 * successful surface projections for this validation attempt, while the
 * source surface is immutable.  Include the search limit in the key so a
 * diagnostic query cannot reuse a result from a different tolerance. */
class repair_surface_context {
public:
    explicit repair_surface_context(const ON_Surface *source) : surface(source) {}

    bool project(const ON_3dPoint &point, double allowed, ON_2dPoint &uv,
	ON_3dPoint &projected, double &distance)
    {
	const query_key key = {point.x, point.y, point.z, allowed};
	const auto found = projections.find(key);
	if (found != projections.end()) {
	    brlcad::PullbackWorkProgress();
	    if (!brlcad::PullbackWorkCancelled()) {
		uv = found->second.uv;
		projected = found->second.point;
		distance = found->second.distance;
		return true;
	    }
	}
	const bool matched = context.SurfaceClosestPoint(surface, point, uv,
	    projected, distance, 0, BREP_SAME_POINT_TOLERANCE, allowed);
	if (matched)
	    projections.emplace(key, projection{uv, projected, distance});
	return matched;
    }

private:
    typedef std::array<double, 4> query_key;
    struct query_compare {
	bool operator()(const query_key &first, const query_key &second) const
	{
	    /* Preserve exact inputs, including signed zero at surface seams. */
	    return std::memcmp(first.data(), second.data(),
		sizeof(double) * first.size()) < 0;
	}
    };
    struct projection {
	ON_2dPoint uv;
	ON_3dPoint point;
	double distance;
    };

    const ON_Surface *surface;
    brlcad::PullbackContext context;
    std::map<query_key, projection, query_compare> projections;
};

static bool
repair_surface_distance(const ON_Brep *brep,
	const std::vector<ON_BoundingBox> &face_bounds,
	std::vector<std::unique_ptr<brlcad::SurfaceTree>> &face_trees,
	std::vector<std::unique_ptr<repair_surface_context>> &face_contexts,
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
		    std::unique_ptr<repair_surface_context>(
			new repair_surface_context(face.SurfaceOf()));
	    ON_2dPoint uv;
	    ON_3dPoint projected = ON_3dPoint::UnsetPoint;
	    double candidate_distance =
		std::numeric_limits<double>::infinity();
	    const bool projected_point = face_contexts[(size_t)face_index]->
		project(point, allowed, uv, projected, candidate_distance);
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
	    brlcad::get_closest_point(uv, face, point, tree) && uv.IsValid();
	if (projected_point) {
	    projected = face.SurfaceOf()->PointAt(uv.x, uv.y);
	    projected_point = projected.IsValid();
	    if (projected_point)
		candidate_distance = projected.DistanceTo(point);
	}
	const auto outside_face_trim = [&](const ON_2dPoint &test_uv) {
	    const brlcad::BRNode *closest_trim = NULL;
	    double trim_distance = -1.0;
	    if (!tree->Valid())
		return true;
	    const bool trimmed = tree->getRootNode()->isTrimmed(test_uv,
		&closest_trim, trim_distance, BREP_EDGE_MISS_TOLERANCE);
	    /* An oriented trim may classify its boundary as outside.  A point
	     * within the trim tolerance is nevertheless part of the face. */
	    return trimmed && (!closest_trim || trim_distance < 0.0 ||
		trim_distance > BREP_EDGE_MISS_TOLERANCE);
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
		    std::unique_ptr<repair_surface_context>(
			new repair_surface_context(face.SurfaceOf()));
	    ON_2dPoint fallback_uv;
	    ON_3dPoint fallback_projection = ON_3dPoint::UnsetPoint;
	    double fallback_distance =
		std::numeric_limits<double>::infinity();
	    const bool fallback_point = face_contexts[(size_t)face_index]->
		project(point, allowed, fallback_uv, fallback_projection,
		    fallback_distance) && fallback_uv.IsValid() &&
		fallback_projection.IsValid();
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

static void
repair_index_triangles(RTree<size_t, double, 3> &index,
	const fastf_t *vertices, const int *faces, int face_count)
{
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
	for (int corner = 0; corner < 3; ++corner) {
	    const int vertex = faces[(size_t)face * 3 + corner];
	    for (int axis = 0; axis < 3; ++axis) {
		const double coordinate = vertices[(size_t)vertex * 3 + axis];
		minimum[axis] = std::min(minimum[axis], coordinate);
		maximum[axis] = std::max(maximum[axis], coordinate);
	    }
	}
	index.Insert(minimum, maximum, (size_t)face);
    }
}

static std::array<ON_3dPoint, 4>
repair_triangle_samples(const ON_3dPoint &a, const ON_3dPoint &b,
	const ON_3dPoint &c)
{
    return {{(a + b + c) / 3.0, (a + b) / 2.0,
	(b + c) / 2.0, (c + a) / 2.0}};
}

static double
repair_diagnostic_distance_limit(double tolerance)
{
    const double diagnostic_factor = 4.0;
    return tolerance <= std::numeric_limits<double>::max() /
	diagnostic_factor ? diagnostic_factor * tolerance : tolerance;
}

static bool
repair_input_mesh_distance(RTree<size_t, double, 3> &triangle_index,
	const fastf_t *vertices, const int *faces, const ON_3dPoint &point,
	double allowed, double *distance, size_t *closest_face = NULL)
{
    if (!vertices || !faces || !distance || !point.IsValid() ||
	!(allowed > 0.0))
	return false;
    point_t test_point;
    VSET(test_point, point.x, point.y, point.z);
    /* Once a query finds a triangle within its radius, every equally near
     * triangle must also overlap that cube.  Repeating the original traversal
     * order preserves source attribution on ties.  Most fidelity samples are
     * much closer than the maximum permitted repair distance. */
    constexpr double initial_radius_subdivisions = 64.0;
    constexpr long double distance_roundoff_factor = 64.0L;
    double radius = allowed / initial_radius_subdivisions;
    for (;;) {
	double minimum[3], maximum[3];
	for (int axis = 0; axis < 3; ++axis) {
	    minimum[axis] = std::nextafter(test_point[axis] - radius,
		-std::numeric_limits<double>::infinity());
	    maximum[axis] = std::nextafter(test_point[axis] + radius,
		std::numeric_limits<double>::infinity());
	}
	double closest = std::numeric_limits<double>::infinity();
	size_t closest_triangle = std::numeric_limits<size_t>::max();
	triangle_index.Search(minimum, maximum, [&](size_t candidate, void *) {
	    point_t triangle[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = faces[candidate * 3 + (size_t)corner];
		VSET(triangle[corner], vertices[(size_t)vertex * 3],
		    vertices[(size_t)vertex * 3 + 1],
		    vertices[(size_t)vertex * 3 + 2]);
	    }
	    long double lower_squared = 0.0L;
	    double magnitude = 0.0;
	    for (int axis = 0; axis < 3; ++axis) {
		const double low = std::min(triangle[0][axis],
		    std::min(triangle[1][axis], triangle[2][axis]));
		const double high = std::max(triangle[0][axis],
		    std::max(triangle[1][axis], triangle[2][axis]));
		const long double separation = std::max(0.0L,
		    std::max((long double)low - test_point[axis],
		    (long double)test_point[axis] - high));
		lower_squared += separation * separation;
		magnitude = std::max(magnitude, std::max(std::fabs(low),
		    std::max(std::fabs(high), std::fabs(test_point[axis]))));
	    }
	    /* Only prune a strictly farther box.  Allow floating-point distance
	     * roundoff here; this guard does not change the acceptance tolerance. */
	    const long double guarded_closest = (long double)closest +
		distance_roundoff_factor * DBL_EPSILON * magnitude;
	    if (lower_squared > guarded_closest * guarded_closest)
		return true;
	    const double candidate_distance = bg_tri_closest_pt(NULL,
		test_point, triangle[0], triangle[1], triangle[2]);
	    if (candidate_distance < closest) {
		closest = candidate_distance;
		closest_triangle = candidate;
	    }
	    return true;
	}, NULL);
	if (std::isfinite(closest) && closest <= radius) {
	    *distance = closest;
	    if (closest_face)
		*closest_face = closest_triangle;
	    return true;
	}
	if (radius >= allowed)
	    return false;
	radius = radius > 0.0 ? std::min(allowed, 2.0 * radius) : allowed;
    }
}

extern "C" int
cdt_test_repair_nearest_triangle(void)
{
    constexpr int side = 8;
    const double scales[] = {0.0001, 1.0, 10000.0};
    const double offsets[] = {0.0, 10000000.0};
    const double allowances[] = {0.00001, 0.125, 1.0, 64.0};
    for (double scale : scales) {
	for (double offset : offsets) {
	    std::vector<fastf_t> vertices;
	    for (int y = 0; y <= side; ++y) {
		for (int x = 0; x <= side; ++x) {
		    vertices.push_back((offset + x) * scale);
		    vertices.push_back((offset + y) * scale);
		    vertices.push_back(offset * scale);
		}
	    }
	    std::vector<int> faces;
	    for (int y = 0; y < side; ++y) {
		for (int x = 0; x < side; ++x) {
		    const int a = y * (side + 1) + x;
		    const int cell[6] = {a, a + 1, a + side + 2,
			a, a + side + 2, a + side + 1};
		    faces.insert(faces.end(), cell, cell + 6);
		}
	    }
	    /* Include collinear and duplicate triangles to exercise degenerate
	     * distance evaluation and exact source-attribution ties. */
	    const int extra[6] = {0, 1, 2, 0, 1, side + 2};
	    faces.insert(faces.end(), extra, extra + 6);
	    RTree<size_t, double, 3> tree;
	    for (size_t triangle = 0; triangle < faces.size() / 3; ++triangle) {
		double minimum[3], maximum[3];
		for (int axis = 0; axis < 3; ++axis) {
		    minimum[axis] = DBL_MAX;
		    maximum[axis] = -DBL_MAX;
		    for (int corner = 0; corner < 3; ++corner) {
			const double coordinate = vertices[
			    (size_t)faces[triangle * 3 + corner] * 3 + axis];
			minimum[axis] = std::min(minimum[axis], coordinate);
			maximum[axis] = std::max(maximum[axis], coordinate);
		    }
		}
		tree.Insert(minimum, maximum, triangle);
	    }
	    for (int sample = 0; sample < (side + 2) * (side + 2); ++sample) {
		point_t point;
		VSET(point, (offset + sample % (side + 2) - 0.25) * scale,
		    (offset + sample / (side + 2) - 0.25) * scale,
		    (offset + (sample % 3) * 0.0625) * scale);
		for (double allowance : allowances) {
		    const double allowed = allowance * scale;
		    double minimum[3], maximum[3];
		    for (int axis = 0; axis < 3; ++axis) {
			minimum[axis] = point[axis] - allowed;
			maximum[axis] = point[axis] + allowed;
		    }
		    double exhaustive = std::numeric_limits<double>::infinity();
		    size_t exhaustive_face = SIZE_MAX;
		    tree.Search(minimum, maximum, [&](size_t candidate, void *) {
			const double distance = bg_tri_closest_pt(NULL, point,
			    &vertices[(size_t)faces[candidate * 3] * 3],
			    &vertices[(size_t)faces[candidate * 3 + 1] * 3],
			    &vertices[(size_t)faces[candidate * 3 + 2] * 3]);
			if (distance < exhaustive) {
			    exhaustive = distance;
			    exhaustive_face = candidate;
			}
			return true;
		    }, NULL);
		    double nearest = -1.0;
		    size_t nearest_face = SIZE_MAX;
		    const bool found = repair_input_mesh_distance(tree,
			vertices.data(), faces.data(), ON_3dPoint(point), allowed,
			&nearest, &nearest_face);
		    if (found != (std::isfinite(exhaustive) && exhaustive <= allowed))
			return 1;
		    if (found && (nearest < exhaustive || nearest > exhaustive ||
			nearest_face != exhaustive_face))
			return 2;
		}
	    }
	}
    }
    return 0;
}

struct repair_source_sample {
    ON_3dPoint point;
    int face;
};

static size_t
repair_sample_position(size_t sample, size_t count, size_t available)
{
    return (size_t)(((long double)sample + 0.5L) *
	(long double)available / (long double)count);
}

static std::vector<repair_source_sample>
repair_source_boundary_samples(const ON_Brep &brep, size_t sample_limit)
{
    size_t referenced_edges = 0;
    for (int ei = 0; ei < brep.m_E.Count(); ++ei)
	if (brep.m_E[ei].TrimCount() > 0)
	    referenced_edges++;
    const size_t selected_edges = std::min(referenced_edges, sample_limit);
    std::vector<repair_source_sample> samples;
    if (!selected_edges)
	return samples;

    /* Include endpoints and interior curve samples, including seam edges.
     * The source may have many more edges than the validation budget.  Spread
     * that budget over the complete edge list rather than its first faces. */
    const size_t maximum_samples_per_edge = 17;
    const size_t samples_per_edge = std::min(maximum_samples_per_edge,
	sample_limit / selected_edges);
    samples.reserve(selected_edges * samples_per_edge);
    size_t selected = 0;
    size_t ordinal = 0;
    for (int ei = 0; ei < brep.m_E.Count() && selected < selected_edges; ++ei) {
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.TrimCount() == 0)
	    continue;
	const size_t target = repair_sample_position(selected, selected_edges,
	    referenced_edges);
	if (ordinal++ != target)
	    continue;
	selected++;
	const ON_BrepTrim *trim = edge.Trim(0);
	const int source_face = trim && trim->Face() ?
	    trim->Face()->m_face_index : -1;
	const ON_Interval domain = edge.Domain();
	for (size_t sample = 0; sample < samples_per_edge; ++sample) {
	    const double fraction = samples_per_edge == 1 ? 0.5 :
		(double)sample / (double)(samples_per_edge - 1);
	    const ON_3dPoint point = domain.IsIncreasing() ?
		edge.PointAt(domain.ParameterAt(fraction)) : ON_3dPoint::UnsetPoint;
	    samples.push_back({point, source_face});
	}
    }
    return samples;
}

static bool
repair_face_covers_surface(const ON_Brep &brep, int face_index)
{
    if (!brep.FaceIsSurface(face_index))
	return false;
    const ON_BrepFace &face = brep.m_F[face_index];
    const ON_Surface *surface = face.SurfaceOf();
    const ON_BrepLoop *loop = face.OuterLoop();
    if (!surface || !loop)
	return false;
    unsigned int sides = 0;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	int side = -1;
	switch (trim->m_iso) {
	    case ON_Surface::W_iso: side = 0; break;
	    case ON_Surface::S_iso: side = 1; break;
	    case ON_Surface::E_iso: side = 2; break;
	    case ON_Surface::N_iso: side = 3; break;
	    default: return false;
	}
	ON_BoundingBox bounds;
	if (!trim->GetBoundingBox(bounds) || !bounds.IsValid())
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    if (!domain.IsIncreasing())
		return false;
	    const double coordinate_scale = std::max(domain.Length(),
		std::max(std::fabs(domain.Min()), std::fabs(domain.Max())));
	    const double roundoff = 256.0 *
		std::numeric_limits<double>::epsilon() * coordinate_scale;
	    if (bounds.m_min[direction] < domain.Min() - roundoff ||
		bounds.m_max[direction] > domain.Max() + roundoff)
		return false;
	    if (direction != side % 2)
		continue;
	    const double boundary = side < 2 ? domain.Min() : domain.Max();
	    if (std::fabs(bounds.m_min[direction] - boundary) > roundoff ||
		std::fabs(bounds.m_max[direction] - boundary) > roundoff)
		return false;
	}
	sides |= 1u << side;
    }
    /* Iso flags alone can be stale on imported faces.  Require geometric
     * agreement with all four sides before sampling the untrimmed interior. */
    return sides == 0xfu;
}

static std::vector<repair_source_sample>
repair_source_interior_samples(const ON_Brep &brep, size_t sample_limit)
{
    std::vector<repair_source_sample> samples;
    const size_t selected_faces = std::min(sample_limit,
	(size_t)brep.m_F.Count());
    if (!selected_faces)
	return samples;
    /* A 3 by 3 interior grid includes the middle of a bulging face whose
     * complete boundary could still match a flattened output mesh.  General
     * trimmed faces need a separate region proof; do not sample their holes
     * or the unused parts of their support surfaces. */
    const size_t grid_width = 3;
    const size_t grid_size = grid_width * grid_width;
    const size_t per_face = std::min(grid_size, sample_limit / selected_faces);
    samples.reserve(selected_faces * per_face);
    for (size_t selected = 0; selected < selected_faces; ++selected) {
	const int face_index = (int)repair_sample_position(selected,
	    selected_faces, (size_t)brep.m_F.Count());
	if (!repair_face_covers_surface(brep, face_index))
	    continue;
	const ON_Surface *surface = brep.m_F[face_index].SurfaceOf();
	for (size_t sample = 0; sample < per_face; ++sample) {
	    const size_t cell = repair_sample_position(sample, per_face, grid_size);
	    const double u = (double)(1 + cell % grid_width) / (grid_width + 1);
	    const double v = (double)(1 + cell / grid_width) / (grid_width + 1);
	    samples.push_back({surface->PointAt(surface->Domain(0).ParameterAt(u),
		surface->Domain(1).ParameterAt(v)), face_index});
	}
    }
    return samples;
}

int
cdt_test_repair_source_coverage(void)
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0, 0, 0), ON_3dPoint(1, 0, 0),
	ON_3dPoint(1, 1, 0), ON_3dPoint(0, 1, 0),
	ON_3dPoint(0, 0, 1), ON_3dPoint(1, 0, 1),
	ON_3dPoint(1, 1, 1), ON_3dPoint(0, 1, 1)
    };
    std::unique_ptr<ON_Brep> box(ON_BrepBox(corners));
    if (!box)
	return 1;
    const size_t sample_budget = 4096;
    const auto samples = repair_source_boundary_samples(*box, sample_budget);
    if (samples.empty() || samples.size() > sample_budget)
	return 2;
    const fastf_t vertices[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
    const int faces[] = {0, 1, 2, 0, 2, 3};
    RTree<size_t, double, 3> triangle_index;
    const double minimum[3] = {0, 0, 0};
    const double maximum[3] = {1, 1, 0};
    triangle_index.Insert(minimum, maximum, 0);
    triangle_index.Insert(minimum, maximum, 1);
    size_t covered = 0;
    size_t omitted = 0;
    for (const auto &sample : samples) {
	if (!sample.point.IsValid() || sample.face < 0 ||
	    !box->BoundingBox().IsPointIn(sample.point))
	    return 3;
	double distance;
	if (repair_input_mesh_distance(triangle_index, vertices, faces,
	    sample.point, BN_TOL_DIST, &distance))
	    ++covered;
	else
	    ++omitted;
    }
    if (!covered || !omitted)
	return 4;

    /* An unrelated construction curve must not become a required surface. */
    const int start = box->NewVertex(ON_3dPoint(10, 10, 10)).m_vertex_index;
    const int end = box->NewVertex(ON_3dPoint(11, 10, 10)).m_vertex_index;
    box->NewEdge(box->m_V[start], box->m_V[end], box->AddEdgeCurve(
	new ON_LineCurve(box->m_V[start].Point(), box->m_V[end].Point())));
    const auto with_wire = repair_source_boundary_samples(*box, sample_budget);
    if (with_wire.size() != samples.size())
	return 5;
    for (size_t i = 0; i < samples.size(); ++i)
	if (samples[i].point != with_wire[i].point ||
	    samples[i].face != with_wire[i].face)
	    return 6;
    if (repair_source_boundary_samples(*box, 1).size() != 1)
	return 7;

    ON_NurbsSurface dome(3, false, 3, 3, 3, 3);
    dome.MakeClampedUniformKnotVector(0, 1.0);
    dome.MakeClampedUniformKnotVector(1, 1.0);
    for (int u = 0; u < 3; ++u) {
	for (int v = 0; v < 3; ++v)
	    dome.SetCV(u, v, ON_3dPoint(0.5 * u, 0.5 * v,
		u == 1 && v == 1 ? 1.0 : 0.0));
    }
    ON_Brep curved;
    if (!curved.NewFace(dome) || !curved.IsValid())
	return 8;
    for (const auto &sample : repair_source_boundary_samples(curved,
	    sample_budget)) {
	double distance;
	if (!repair_input_mesh_distance(triangle_index, vertices, faces,
		sample.point, BN_TOL_DIST, &distance))
	    return 9;
    }
    const auto interior = repair_source_interior_samples(curved, 1);
    double distance;
    if (interior.size() != 1 || interior[0].face != 0 ||
	!interior[0].point.IsValid() || repair_input_mesh_distance(triangle_index,
	    vertices, faces, interior[0].point, BN_TOL_DIST, &distance))
	return 10;
    /* A hole-only or contradictory boundary does not authorize samples from
     * the entire underlying patch. */
    curved.m_L[0].m_type = ON_BrepLoop::inner;
    if (!repair_source_interior_samples(curved, sample_budget).empty())
	return 11;
    curved.m_L[0].m_type = ON_BrepLoop::outer;
    curved.m_C2[curved.m_T[0].m_c2i]->Transform(
	ON_Xform::TranslationTransformation(0.0, 0.1, 0.0));
    if (!repair_source_interior_samples(curved, sample_budget).empty())
	return 12;

    /* Repeated fidelity samples must retain all projection outputs without
     * borrowing a wider search's answer for a narrower query or another
     * source surface. */
    ON_PlaneSurface plane(ON_xy_plane);
    repair_surface_context queries(&plane);
    const ON_3dPoint point(0.25, 0.75, 0.5);
    const double projection_tolerance = 1.0e-10;
    ON_2dPoint uv;
    ON_3dPoint projected;
    if (!queries.project(point, 1.0, uv, projected, distance) ||
	!NEAR_EQUAL(distance, 0.5, projection_tolerance) ||
	projected.DistanceTo(ON_3dPoint(point.x, point.y, 0.0)) >
	    projection_tolerance)
	return 13;
    const ON_2dPoint initial_uv = uv;
    brlcad::PullbackContext reference;
    ON_2dPoint reference_uv;
    ON_3dPoint reference_point;
    double reference_distance = DBL_MAX;
    const bool narrow_reference = reference.SurfaceClosestPoint(&plane,
	point, reference_uv, reference_point, reference_distance, 0,
	BREP_SAME_POINT_TOLERANCE, 0.25);
    if (queries.project(point, 0.25, uv, projected, distance) !=
	    narrow_reference || (narrow_reference &&
	    (!NEAR_EQUAL(distance, reference_distance, projection_tolerance) ||
	    projected.DistanceTo(reference_point) > projection_tolerance)))
	return 14;
    uv = ON_2dPoint::UnsetPoint;
    projected = ON_3dPoint::UnsetPoint;
    distance = DBL_MAX;
    if (!queries.project(point, 1.0, uv, projected, distance) ||
	uv.DistanceTo(initial_uv) > projection_tolerance ||
	!NEAR_EQUAL(distance, 0.5, projection_tolerance) ||
	projected.DistanceTo(ON_3dPoint(point.x, point.y, 0.0)) >
	    projection_tolerance)
	return 15;
    ON_PlaneSurface elevated(ON_Plane(ON_3dPoint(0.0, 0.0, 2.0),
	ON_3dVector::ZAxis));
    repair_surface_context elevated_queries(&elevated);
    if (!elevated_queries.project(point, 2.0, uv, projected, distance) ||
	!NEAR_EQUAL(distance, 1.5, projection_tolerance) ||
	projected.DistanceTo(ON_3dPoint(point.x, point.y, 2.0)) >
	    projection_tolerance)
	return 16;
    return 0;
}

struct repair_changed_face {
    int index;
    double area;
    int source_brep_face = -1;
    bool local_surface_approximation = false;
    bool direct_surface_samples = false;
    double direct_surface_deviation = 0.0;
};

static std::vector<repair_changed_face>
repair_sample_faces(const std::vector<repair_changed_face> &faces,
	size_t sample_limit)
{
    std::vector<repair_changed_face> sampled;
    if (faces.size() <= sample_limit) {
	sampled = faces;
    } else {
	/* Cover the full deterministic output order rather than testing only
	 * the largest triangles.  Always include the largest triangle,
	 * then distribute the remaining centroid samples across the mesh. */
	sampled.reserve(sample_limit);
	for (size_t sample = 0; sample < sample_limit; ++sample) {
	    const long double position_fraction =
		((long double)sample + 0.5L) /
		(long double)sample_limit;
	    const size_t position = std::min(faces.size() - 1,
		(size_t)(position_fraction *
		(long double)faces.size()));
	    sampled.push_back(faces[position]);
	}
	const repair_changed_face largest = *std::max_element(
	    faces.begin(), faces.end(),
	    [](const repair_changed_face &first,
		    const repair_changed_face &second) {
		return first.area < second.area;
	    });
	bool sampled_largest = false;
	for (const repair_changed_face &selected : sampled)
	    sampled_largest = sampled_largest || selected.index == largest.index;
	if (!sampled_largest)
	    sampled[0] = largest;
    }
    return sampled;
}

struct repair_fast_face_range {
    size_t first_face = 0;
    size_t face_count = 0;
    size_t first_point = 0;
    size_t point_count = 0;
    bool present = false;
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

static bool
repair_preserve_approximate_boundary(
	const struct bg_trimesh_solid_errors &errors,
	size_t degenerate_faces, size_t invalid_vertex_links, size_t max_edges)
{
    /* Winding can be synchronized without changing a boundary.  Excess edge
     * uses instead describe branching, which bounded hole repair cannot fix. */
    if (degenerate_faces && invalid_vertex_links)
	return false;
    return !errors.unmatched.count || (!errors.excess.count &&
	(size_t)errors.unmatched.count <= max_edges);
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
    if (repair_missing_patch_bounded(excessive_valence, 100.0,
	    settings, NULL))
	return 4;

    struct bg_trimesh_solid_errors errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    /* These profiles reduce the pistol's bounded crack and the Mirage's
     * closed degenerate fan to deterministic policy checks. */
    const int boundary_limit = 64;
    const int bounded_open_edges = 4;
    const int orientation_errors = 64;
    const size_t degenerate_faces = 10;
    const size_t invalid_vertex_links = 11;
    errors.unmatched.count = bounded_open_edges;
    errors.misoriented.count = orientation_errors;
    if (!repair_preserve_approximate_boundary(errors, 0,
	    bounded_open_edges, boundary_limit))
	return 5;
    errors.excess.count = 1;
    if (repair_preserve_approximate_boundary(errors, 0,
	    bounded_open_edges, boundary_limit))
	return 6;
    errors.excess.count = 0;
    errors.unmatched.count = boundary_limit + 1;
    if (repair_preserve_approximate_boundary(errors, 0,
	    bounded_open_edges, boundary_limit))
	return 7;
    errors.unmatched.count = 0;
    if (repair_preserve_approximate_boundary(errors, degenerate_faces,
	    invalid_vertex_links, boundary_limit))
	return 8;
    return repair_preserve_approximate_boundary(errors, 0, 0,
	boundary_limit) ? 0 : 9;
}

static unsigned int
repair_fast_resource_limits(const struct brep_cdt_fast_report &report)
{
    return (report.hit_time_limit ? BREP_CDT_REPAIR_LIMIT_TIME : 0u) |
	(report.hit_memory_limit ? BREP_CDT_REPAIR_LIMIT_MEMORY : 0u) |
	(report.hit_point_limit ? BREP_CDT_REPAIR_LIMIT_POINTS : 0u);
}

static brep_cdt_fast_options
repair_fast_options(size_t max_points, size_t max_bytes, long max_time_ms)
{
    brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    options.max_workers = 1;
    options.adaptive_quality = 0;
    options.max_points = max_points;
    options.max_result_bytes = max_bytes;
    options.max_time_ms = max_time_ms;
    options.allow_partial = 1;
    return options;
}

struct repair_fast_mesh {
    cdt_owned_array<int> faces;
    int face_count = 0;
    cdt_owned_array<vect_t> normals;
    cdt_owned_array<point_t> points;
    int point_count = 0;
    brep_cdt_fast_report report = {};
    int result;
    unsigned int resource_limits;

    repair_fast_mesh(ON_Brep_CDT_State *state, int face,
	    const brep_cdt_fast_options &options)
    {
	bn_tol model_tolerance = BN_TOL_INIT_TOL;
	result = brep_cdt_fast_ex(cdt_array_address(faces), &face_count,
	    cdt_array_address(normals), cdt_array_address(points),
	    &point_count, state->orig_brep, face, &state->tol,
	    &model_tolerance, &options, &report);
	resource_limits = repair_fast_resource_limits(report);
    }

    bool valid_geometry() const
    {
	if ((result != BREP_CDT_FAST_OK && result != BREP_CDT_FAST_PARTIAL) ||
		!faces || face_count <= 0 || !points || point_count <= 0)
	    return false;
	for (size_t corner = 0; corner < (size_t)face_count * 3; ++corner) {
	    if (faces[corner] < 0 || faces[corner] >= point_count)
		return false;
	}
	for (int point = 0; point < point_count; ++point) {
	    if (!std::isfinite(points[point][X]) ||
		    !std::isfinite(points[point][Y]) ||
		    !std::isfinite(points[point][Z]))
		return false;
	}
	return true;
    }

    bool valid_normals() const
    {
	if (!normals)
	    return false;
	for (size_t corner = 0; corner < (size_t)face_count * 3; ++corner) {
	    if (!std::isfinite(normals[corner][X]) ||
		    !std::isfinite(normals[corner][Y]) ||
		    !std::isfinite(normals[corner][Z]))
		return false;
	}
	return true;
    }
};

enum repair_closed_boundary_action {
    REPAIR_PRESERVE_CLOSED_BOUNDARY,
    REPAIR_DEFERRED_CLOSED_BOUNDARY,
    REPAIR_RECONSTRUCT_CLOSED_BOUNDARY
};

struct repair_patch_mapping {
    std::vector<int> indices;
    std::vector<fastf_t> vertices;
    std::unordered_map<ON_3dPoint *, int> new_sources;
    size_t source_vertices = 0;
    size_t matched_source_vertices = 0;
};

/* Source identity joins authoritative edge samples across faces.  Coordinate
 * equality only joins generated points within this patch: using it across
 * patches would weld distinct topology.  Keep the input map unchanged so an
 * orientation trial uses exactly the same mapping as an accepted patch. */
static bool
repair_map_patch_vertices(repair_patch_mapping &mapping,
	const fastf_t *vertices, int vertex_count,
	const std::vector<ON_3dPoint *> *sources,
	const std::unordered_map<ON_3dPoint *, int> &input_sources,
	int input_vertex_count)
{
    if (!vertices || vertex_count <= 0 || input_vertex_count < 0 ||
	vertex_count > INT_MAX - input_vertex_count ||
	(sources && sources->size() != (size_t)vertex_count))
	return false;

    mapping = repair_patch_mapping();
    mapping.indices.resize((size_t)vertex_count);
    mapping.vertices.reserve((size_t)vertex_count * 3);
    std::map<repair_point_key, int> generated_points;
    for (int point = 0; point < vertex_count; ++point) {
	ON_3dPoint *source = sources ? (*sources)[(size_t)point] : NULL;
	const fastf_t *coordinates = &vertices[(size_t)point * 3];
	const repair_point_key key = {
	    coordinates[0], coordinates[1], coordinates[2]};
	if (source) {
	    mapping.source_vertices++;
	    const auto existing = input_sources.find(source);
	    const auto added = mapping.new_sources.find(source);
	    if (existing != input_sources.end() ||
		added != mapping.new_sources.end()) {
		mapping.indices[(size_t)point] = existing != input_sources.end() ?
		    existing->second : added->second;
		mapping.matched_source_vertices++;
		continue;
	    }
	} else {
	    const auto existing = generated_points.find(key);
	    if (existing != generated_points.end()) {
		mapping.indices[(size_t)point] = existing->second;
		continue;
	    }
	}
	const int new_index = input_vertex_count +
	    (int)(mapping.vertices.size() / 3);
	if (source)
	    mapping.new_sources[source] = new_index;
	else
	    generated_points[key] = new_index;
	mapping.indices[(size_t)point] = new_index;
	mapping.vertices.insert(mapping.vertices.end(), coordinates,
	    coordinates + 3);
    }
    return true;
}

int
cdt_test_repair_patch_mapping(void)
{
    ON_3dPoint shared(2.0, 0.0, 0.0);
    ON_3dPoint coincident(2.0, 0.0, 0.0);
    ON_3dPoint other(3.0, 0.0, 0.0);
    const fastf_t vertices[] = {
	2.0, 0.0, 0.0, 2.0, 0.0, 0.0,
	2.0, 0.0, 0.0, 2.0, 0.0, 0.0,
	2.0, 0.0, 0.0, 3.0, 0.0, 0.0,
	3.0, 0.0, 0.0, 2.0, 0.0, 0.0
    };
    const int vertex_count = sizeof(vertices) / (3 * sizeof(fastf_t));
    std::vector<ON_3dPoint *> sources = {
	&shared, &coincident, &coincident, NULL, NULL,
	&other, &other, &shared};
    std::unordered_map<ON_3dPoint *, int> input_sources = {{&shared, 1}};

    /* Distinct source vertices and generated samples stay distinct even at
     * identical coordinates.  Repeated source identities still join. */
    repair_patch_mapping first;
    const std::vector<int> expected = {1, 2, 2, 3, 3, 4, 4, 1};
    if (!repair_map_patch_vertices(first, vertices, vertex_count, &sources,
	    input_sources, 2) || first.indices != expected ||
	first.vertices.size() != 9 || first.new_sources.size() != 2 ||
	first.source_vertices != 6 || first.matched_source_vertices != 4 ||
	input_sources.size() != 1)
	return 1;

    repair_patch_mapping repeated;
    if (!repair_map_patch_vertices(repeated, vertices, vertex_count, &sources,
	    input_sources, 2) || repeated.indices != first.indices ||
	repeated.new_sources != first.new_sources ||
	repeated.vertices.size() != first.vertices.size() ||
	memcmp(repeated.vertices.data(), first.vertices.data(),
	    first.vertices.size() * sizeof(fastf_t)))
	return 2;

    /* A later patch reuses installed source identities but owns its generated
     * vertices, even when their coordinates match an earlier patch. */
    input_sources.insert(first.new_sources.begin(), first.new_sources.end());
    repair_patch_mapping later;
    const std::vector<int> later_expected = {1, 2, 2, 5, 5, 4, 4, 1};
    if (!repair_map_patch_vertices(later, vertices, vertex_count, &sources,
	    input_sources, 5) || later.indices != later_expected ||
	later.vertices.size() != 3 || !later.new_sources.empty())
	return 3;

    repair_patch_mapping generated;
    const std::vector<int> generated_expected = {2, 2, 2, 2, 2, 3, 3, 2};
    if (!repair_map_patch_vertices(generated, vertices, vertex_count, NULL,
	    input_sources, 2) || generated.indices != generated_expected ||
	generated.vertices.size() != 6 || !generated.new_sources.empty())
	return 4;

    if (repair_map_patch_vertices(generated, vertices, vertex_count, &sources,
	    input_sources, INT_MAX) ||
	repair_map_patch_vertices(generated, NULL, vertex_count, &sources,
	    input_sources, 0))
	return 5;
    sources.pop_back();
    if (repair_map_patch_vertices(generated, vertices, vertex_count, &sources,
	    input_sources, 0))
	return 6;
    return 0;
}

static const char *
repair_settings_error(const struct brep_cdt_repair_settings *settings)
{
    if (!std::isfinite(settings->max_surface_deviation) ||
	    settings->max_surface_deviation < 0.0 ||
	    !std::isfinite(settings->max_area_change_percent) ||
	    settings->max_area_change_percent < 0.0 ||
	    !std::isfinite(settings->max_planar_cap_area_percent) ||
	    settings->max_planar_cap_area_percent < 0.0 ||
	    (settings->max_planar_cap_area_percent > 0.0 &&
	     !settings->try_invalid_brep) ||
	    !std::isfinite(settings->relaxed_fidelity_factor) ||
	    settings->relaxed_fidelity_factor < 0.0 ||
	    (settings->relaxed_fidelity_factor > 0.0 &&
	    (settings->relaxed_fidelity_factor < 1.0 ||
	    settings->relaxed_fidelity_factor > 4.0)) ||
	    (settings->max_adaptive_hole_edges > 0 &&
	    settings->max_adaptive_hole_edges <
	    settings->mesh.max_hole_edges) ||
	    !std::isfinite(settings->max_adaptive_hole_area_percent) ||
	    settings->max_adaptive_hole_area_percent < 0.0 ||
	    (settings->max_adaptive_hole_area_percent > 0.0 &&
	    settings->max_adaptive_hole_area_percent <
	    settings->mesh.max_hole_area_percent) ||
	    ((settings->use_fast_face_fallback ||
	    settings->use_full_fast_fallback ||
	    settings->use_full_fast_fallback_if_needed) &&
	    (!settings->max_fast_points || !settings->max_fast_result_bytes ||
	    settings->max_fast_time_ms <= 0)))
	return "invalid mesh repair limits";
    if (settings->mesh.fill_holes &&
	    (settings->mesh.max_hole_edges < 3 ||
	    (!(settings->mesh.max_hole_area > 0.0) &&
	    !(settings->mesh.max_hole_area_percent > 0.0))))
	return "B-Rep hole repair requires explicit edge and area limits";
    if (settings->mesh.remove_small_components &&
	    !(settings->mesh.max_component_area > 0.0) &&
	    !(settings->mesh.max_component_area_percent > 0.0))
	return "B-Rep component removal requires an explicit area limit";

    return NULL;
}

namespace {

struct repair_mesh_arrays {
    cdt_owned_array<int> faces;
    int face_count = 0;
    cdt_owned_array<fastf_t> vertices;
    int vertex_count = 0;
};

struct repair_triangle_provenance {
    std::map<repair_triangle_key, std::set<int>> local_chart;
    std::map<repair_triangle_key, std::set<int>> fast;
    std::map<repair_triangle_key, double> direct_deviations;
    std::map<repair_triangle_key,
	std::vector<std::pair<int, triangle_t>>> surface_triangles;

    bool source(const repair_triangle_key &key, int &face) const
    {
	bool attributed = false;
	face = -1;
	const auto local = local_chart.find(key);
	if (local != local_chart.end() && local->second.size() == 1) {
	    face = *local->second.begin();
	    attributed = true;
	}
	const auto display = fast.find(key);
	if (face < 0 && display != fast.end() && display->second.size() == 1) {
	    face = *display->second.begin();
	    attributed = true;
	}
	return attributed;
    }

};

enum class repair_face_origin { chart, display };

/* Assembly keeps source identities across patches, but generated-coordinate
 * welding remains local to each patch.  Coordinate keys retain all possible
 * source faces rather than silently resolving ambiguous provenance.
 */
class repair_assembly {
public:
    repair_mesh_arrays mesh;
    int rigorous_face_count = 0;
    std::vector<int> rigorous_sources;
    std::vector<int> face_sources;
    repair_triangle_provenance provenance;
    std::set<int> reconstructed_faces;
    std::set<int> approximation_faces;

    void record_rigorous_input(ON_Brep_CDT_State *state);
    int append(int face_index, repair_face_origin origin,
	    const int *faces, int face_count, const fastf_t *vertices,
	    int vertex_count,
	    const std::vector<ON_3dPoint *> *source_points = NULL);
    bool orientable(const int *faces, int face_count, const fastf_t *vertices,
	    int vertex_count, const std::vector<ON_3dPoint *> *source_points) const;
    int append_face(ON_Brep_CDT_State *state, int face_index,
	    const repair_boundary_patch *reference_boundary = NULL);
    int source_vertex(ON_3dPoint *point) const
    {
	const auto found = approximate_source_points.find(point);
	return found == approximate_source_points.end() ? -1 : found->second;
    }
    void release_source_index()
    {
	std::unordered_map<ON_3dPoint *, int>().swap(approximate_source_points);
	std::vector<int>().swap(face_sources);
    }

private:
    std::unordered_map<ON_3dPoint *, int> approximate_source_points;
};

int
repair_assembly::append(int face_index, repair_face_origin origin,
	const int *local_faces, int local_face_count,
	const fastf_t *local_vertices, int local_vertex_count,
	const std::vector<ON_3dPoint *> *local_source_points)
{
    auto &triangle_sources = origin == repair_face_origin::chart ?
	provenance.local_chart : provenance.fast;
    if (!local_faces || local_face_count <= 0 ||
	    local_face_count > INT_MAX - mesh.face_count)
	return 0;
    for (size_t corner = 0; corner < (size_t)local_face_count * 3; ++corner) {
	if (local_faces[corner] < 0 ||
		local_faces[corner] >= local_vertex_count)
	    return 0;
    }
    repair_patch_mapping mapping;
    if (!repair_map_patch_vertices(mapping, local_vertices,
	    local_vertex_count, local_source_points,
	    approximate_source_points, mesh.vertex_count))
	return 0;
    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	bu_log("Face %d: matched %zu/%zu authoritative repair vertices "
	    "by CDT point identity\n", face_index,
	    mapping.matched_source_vertices, mapping.source_vertices);
    const size_t new_vertex_count = (size_t)mesh.vertex_count +
	mapping.vertices.size() / 3;
    const size_t new_face_count = (size_t)mesh.face_count +
	(size_t)local_face_count;
    mesh.vertices.reset((fastf_t *)bu_realloc(mesh.vertices.release(),
	new_vertex_count * 3 * sizeof(fastf_t),
	"repair input vertices with approximate face"));
    mesh.faces.reset((int *)bu_realloc(mesh.faces.release(),
	new_face_count * 3 * sizeof(int),
	"repair input faces with approximate face"));
    if (!mapping.vertices.empty())
	memcpy(&mesh.vertices[(size_t)mesh.vertex_count * 3],
	    mapping.vertices.data(), mapping.vertices.size() * sizeof(fastf_t));
    for (size_t corner = 0; corner < (size_t)local_face_count * 3; ++corner)
	mesh.faces[(size_t)mesh.face_count * 3 + corner] =
	    mapping.indices[(size_t)local_faces[corner]];
    for (int face = 0; face < local_face_count; ++face) {
	const repair_triangle_key key = repair_triangle_coordinates(
	    mesh.vertices.get(),
	    &mesh.faces[((size_t)mesh.face_count + face) * 3]);
	triangle_sources[key].insert(face_index);
    }
    approximate_source_points.insert(mapping.new_sources.begin(),
	mapping.new_sources.end());
    mesh.vertex_count = (int)new_vertex_count;
    mesh.face_count = (int)new_face_count;
    face_sources.insert(face_sources.end(),
	(size_t)local_face_count, face_index);
    return local_face_count;
}

bool
repair_assembly::orientable(const int *local_faces, int local_face_count,
	const fastf_t *local_vertices, int local_vertex_count,
	const std::vector<ON_3dPoint *> *local_source_points) const
{
    if (!local_faces || local_face_count <= 0 ||
	    local_face_count > INT_MAX - mesh.face_count)
	return false;
    repair_patch_mapping mapping;
    if (!repair_map_patch_vertices(mapping, local_vertices,
	    local_vertex_count, local_source_points,
	    approximate_source_points, mesh.vertex_count))
	return false;
    std::vector<int> combined;
    combined.reserve(((size_t)mesh.face_count + local_face_count) * 3);
    if (mesh.face_count)
	combined.insert(combined.end(), mesh.faces.get(),
	    mesh.faces.get() + (size_t)mesh.face_count * 3);
    for (size_t corner = 0; corner < (size_t)local_face_count * 3; ++corner) {
	const int local = local_faces[corner];
	if (local < 0 || local >= local_vertex_count)
	    return false;
	combined.push_back(mapping.indices[(size_t)local]);
    }
    std::vector<int> synchronized(combined.size());
    return bg_trimesh_sync(synchronized.data(), combined.data(),
	mesh.face_count + local_face_count) >= 0;
}

int
repair_assembly::append_face(ON_Brep_CDT_State *s_cdt, int face_index,
	const repair_boundary_patch *reference_boundary)
{
    cdt_owned_array<int> owned_local_faces;
    int local_face_count = 0;
    cdt_owned_array<fastf_t> owned_local_vertices;
    int local_vertex_count = 0;
    std::vector<ON_3dPoint *> local_source_points;
    int one_face = face_index;
    bool assembled = ON_Brep_CDT_Mesh(cdt_array_address(owned_local_faces),
	&local_face_count, cdt_array_address(owned_local_vertices),
	&local_vertex_count,
	NULL, NULL, NULL, NULL, s_cdt, 1, &one_face) == 0;
    if (assembled && s_cdt->bot_pnt_to_on_pnt &&
	    s_cdt->bot_pnt_to_on_pnt->size() ==
	    (size_t)local_vertex_count)
	local_source_points = *s_cdt->bot_pnt_to_on_pnt;
    if (assembled && reference_boundary &&
	    !repair_patch_matches_source_boundary(owned_local_faces.get(),
		local_face_count, local_source_points,
		*reference_boundary)) {
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	    bu_log("Face %d: rejected a cleaned chart whose open edges "
		"do not match its authoritative boundary\n", face_index);
	assembled = false;
    }
    const int appended = assembled ? append(face_index,
	repair_face_origin::chart,
	owned_local_faces.get(), local_face_count, owned_local_vertices.get(),
	local_vertex_count, local_source_points.empty() ? NULL :
	&local_source_points) : 0;
    return appended;
}

void
repair_assembly::record_rigorous_input(ON_Brep_CDT_State *s_cdt)
{
    rigorous_face_count = mesh.face_count;
    rigorous_sources =
	s_cdt->bot_face_to_brep_face;
    face_sources = rigorous_sources;
    if (face_sources.size() != (size_t)mesh.face_count)
	face_sources.assign((size_t)mesh.face_count, -1);

    if (s_cdt->bot_pnt_to_on_pnt &&
	    s_cdt->bot_pnt_to_on_pnt->size() ==
	    (size_t)mesh.vertex_count) {
	for (int point = 0; point < mesh.vertex_count; ++point) {
	    ON_3dPoint *source = (*s_cdt->bot_pnt_to_on_pnt)[
		(size_t)point];
	    if (source)
		approximate_source_points[source] = point;
	}
    }
}

struct repair_candidate_mesh {
    cdt_owned_array<int> faces;
    int face_count = 0;
    cdt_owned_array<point_t> points;
    int vertex_count = 0;
    bool preserve_input = false;
    fastf_t *vertices() const { return (fastf_t *)points.get(); }
};

struct repair_neighborhood_result {
    repair_degenerate_neighborhood_stats stats;
    bool applied = false;
    double reference_area = 0.0;
};

struct repair_fidelity_limits {
    bool relaxed_enabled = false;
    bool relaxed_applied = false;
    double surface_deviation = 0.0;
    double area_change_limit = 0.0;
};

struct repair_surface_queries {
    std::vector<ON_BoundingBox> bounds;
    std::vector<std::unique_ptr<brlcad::SurfaceTree>> trees;
    std::vector<std::unique_ptr<repair_surface_context>> contexts;
};

struct repair_preservation_result {
    std::map<repair_triangle_key, size_t> rigorous_counts;
    std::vector<repair_triangle_key> unresolved;
    std::map<repair_triangle_key, std::set<int>> missing_sources;
    bool bounded_replacement = false;
};

class repair_attempt {
public:
    repair_attempt(ON_Brep_CDT_State *state,
	    const brep_cdt_repair_settings *options, brep_cdt_repair_report *result,
	    repair_closed_boundary_action &boundary_action, bool preserve_samples)
	: s_cdt(state), settings(options), report(result),
	  closed_boundary_action(boundary_action),
	  preserve_pullback_samples(preserve_samples),
	  failed_faces(state->failed_face_indices.begin(),
	      state->failed_face_indices.end()) {}
    int run();

private:
    ON_Brep_CDT_State *s_cdt;
    const brep_cdt_repair_settings *settings;
    brep_cdt_repair_report *report;
    repair_closed_boundary_action &closed_boundary_action;
    bool preserve_pullback_samples;
    std::set<int> failed_faces;
    std::set<int> best_effort_faces;
    repair_assembly assembly;
    repair_neighborhood_result neighborhood;
    repair_candidate_mesh candidate;
    repair_fidelity_limits fidelity;
    repair_surface_queries surface_queries;
    repair_preservation_result preservation;

    size_t sample_limit() const
    {
	const size_t default_samples = 4096;
	return settings->max_deviation_samples ?
	    settings->max_deviation_samples : default_samples;
    }
    int prepare_input();
    void retain_adaptive_faces();
    void recover_clean_charts();
    void recover_surface_patches();
    void recover_topological_disks();
    int recover_full_display();
    void recover_display_faces();
    int prepare_boundary_repair();
    void trace_input();
    int repair_boundaries();
    int repair_mesh();
    int prepare_validation();
    int validate_mesh();
    int preserve_rigorous_triangles();
    int validate_area();
    int validate_surface();
    int validate_coverage();
    int publish();
};

int
repair_attempt::prepare_input()
{
    assembly.approximation_faces.insert(failed_faces.begin(), failed_faces.end());
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
    assembly.approximation_faces.insert(bounded_edge_faces.begin(),
	bounded_edge_faces.end());
    report->bounded_edge_approximation_faces = (int)std::min(
	bounded_edge_faces.size(), (size_t)INT_MAX);
    for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face) {
	if (s_cdt->fmeshes.find(face) == s_cdt->fmeshes.end())
	    assembly.approximation_faces.insert(face);
    }
    std::vector<int> usable_faces;
    for (const auto &face : s_cdt->fmeshes) {
	if (failed_faces.find(face.first) != failed_faces.end())
	    continue;
	if (face.second.tris_tree.Count())
	    usable_faces.push_back(face.first);
	else
	    assembly.approximation_faces.insert(face.first);
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
    /* Whole-B-Rep fallback replaces every rigorous face mesh.  Do not spend
     * another bounded refinement cycle assembling those faces after a stage
     * 11 failure only to discard the result below. */
    if (!settings->use_full_fast_fallback && !usable_faces.empty()) {
	if (ON_Brep_CDT_Mesh(cdt_array_address(assembly.mesh.faces), &assembly.mesh.face_count,
	    cdt_array_address(assembly.mesh.vertices),
	    &assembly.mesh.vertex_count, NULL, NULL, NULL, NULL, s_cdt,
	    usable_selection_count, usable_selection) < 0 ||
	    assembly.mesh.face_count <= 0 || assembly.mesh.vertex_count <= 0) {
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		report->source_diagnostic.completed_faces,
		report->source_failed_faces,
		"failed to assemble the usable face meshes for repair");
	    return -1;
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
	while (assembly.mesh.face_count > 0 && assembly.mesh.vertex_count > 0 &&
	    partial_refinement_attempts <
	    CDT_MAX_ASSEMBLED_REFINEMENT_ATTEMPTS &&
	    partial_refinement_points < CDT_MAX_ASSEMBLED_REFINEMENT_POINTS) {
	    assembled_mesh_validation partial_validation;
	    assembled_mesh_validate(assembly.mesh.vertex_count, assembly.mesh.face_count,
		assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &partial_validation,
		!settings->mesh.allow_self_intersections);
	    if (!partial_validation.intersecting_triangle_pairs)
		break;
	    const size_t inserted = refine_assembled_intersection(s_cdt,
		partial_validation, CDT_MAX_ASSEMBLED_REFINEMENT_POINTS -
		partial_refinement_points);
	    if (!inserted)
		break;
	    partial_refinement_points += inserted;
	    partial_refinement_attempts++;
	    assembly.mesh.faces.reset();
	    assembly.mesh.vertices.reset();
	    if (ON_Brep_CDT_Mesh(cdt_array_address(assembly.mesh.faces), &assembly.mesh.face_count,
		cdt_array_address(assembly.mesh.vertices), &assembly.mesh.vertex_count,
		NULL, NULL, NULL,
		NULL, s_cdt, usable_selection_count,
		usable_selection) < 0 ||
		assembly.mesh.face_count <= 0 || assembly.mesh.vertex_count <= 0) {
		cdt_diagnostic_set(s_cdt,
		    BREP_CDT_RESULT_REPAIR_FAILED,
		    BREP_CDT_STAGE_MESH_REPAIR, -1,
		    report->source_diagnostic.completed_faces,
		    report->source_failed_faces,
		    "failed to reassemble cross-face refined repair input");
		return -1;
	    }
	}
    }
    assembly.record_rigorous_input(s_cdt);
    return 0;
}

void
repair_attempt::retain_adaptive_faces()
{
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
	    const int appended = assembly.append_face(s_cdt, failed_face);
	    if (!appended)
		continue;
	    RTree<size_t, double, 3>::Iterator active;
	    mesh->second.tris_tree.GetFirst(active);
	    while (!active.IsNull()) {
		const size_t triangle_index = *active;
		if (triangle_index < mesh->second.tris_vect.size()) {
		    const triangle_t &triangle =
			mesh->second.tris_vect[triangle_index];
		    assembly.provenance.surface_triangles[
			repair_triangle_coordinates(mesh->second, triangle)]
			.push_back(std::make_pair(failed_face, triangle));
		}
		++active;
	    }
	    assembly.reconstructed_faces.insert(failed_face);
	    best_effort_faces.insert(failed_face);
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
}

void
repair_attempt::recover_clean_charts()
{
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
	    if (assembly.reconstructed_faces.find(failed_face) !=
		    assembly.reconstructed_faces.end())
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
		assembly.append_face(s_cdt, failed_face, has_reference_boundary ?
		    &reference_boundary : NULL);
	    if (!local_face_count)
		continue;
	    assembly.reconstructed_faces.insert(failed_face);
	    bu_log("Face %d: retained a locally cleaned surface chart for "
		"repair (%d triangles)\n", failed_face, local_face_count);
	}
    }
}

void
repair_attempt::recover_surface_patches()
{
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
	    if (assembly.reconstructed_faces.find(failed_face) !=
		    assembly.reconstructed_faces.end())
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
	    if (boundary_strip && !assembly.orientable(
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
			assembly.orientable(alternate.faces.data(),
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
	    const int appended = assembly.append(failed_face,
		repair_face_origin::chart,
		patch.faces.data(), (int)patch_faces, patch.vertices.data(),
		(int)patch_points, patch.source_points.empty() ? NULL :
		&patch.source_points);
	    if (!appended)
		continue;
	    if (patch.direct_surface_deviations.size() == patch_faces) {
		for (size_t triangle = 0; triangle < patch_faces; ++triangle) {
		    const repair_triangle_key key = repair_triangle_coordinates(
			patch.vertices.data(), &patch.faces[triangle * 3]);
		    assembly.provenance.direct_deviations[key] =
			patch.direct_surface_deviations[triangle];
		}
	    }
	    assembly.reconstructed_faces.insert(failed_face);
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
}

void
repair_attempt::recover_topological_disks()
{
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
	    if (assembly.reconstructed_faces.find(failed_face) !=
		    assembly.reconstructed_faces.end())
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
	    const int appended = assembly.append(failed_face,
		repair_face_origin::chart,
		patch.faces.data(), (int)patch_faces, patch.vertices.data(),
		(int)patch_points, patch.source_points.empty() ? NULL :
		&patch.source_points);
	    if (!appended)
		continue;
	    assembly.reconstructed_faces.insert(failed_face);
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
}

int
repair_attempt::recover_full_display()
{
    if (settings->use_full_fast_fallback) {
	report->fast_fallback_attempted_faces =
	    s_cdt->orig_brep->m_F.Count();
	brep_cdt_fast_options fast_options = repair_fast_options(
	    settings->max_fast_points, settings->max_fast_result_bytes,
	    settings->max_fast_time_ms);
	fast_options.preserve_pullback_samples = preserve_pullback_samples ? 1 : 0;
	std::vector<repair_fast_face_range> fast_face_ranges(
	    (size_t)s_cdt->orig_brep->m_F.Count());
	fast_options.face_output = repair_fast_face_output;
	fast_options.face_output_data = &fast_face_ranges;
	repair_fast_mesh fast(s_cdt, -1, fast_options);
	report->resource_limits |= fast.resource_limits;
	const bool valid_fast_mesh = fast.face_count <= INT_MAX / 3 &&
	    (size_t)fast.point_count <= settings->max_fast_points &&
	    fast.report.result_bytes <= settings->max_fast_result_bytes &&
	    fast.valid_geometry() && fast.valid_normals();
	if (!valid_fast_mesh) {
	    report->fast_fallback_failed_faces =
		std::max(1, fast.report.failed_faces);
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		report->source_diagnostic.completed_faces,
		report->source_failed_faces,
		fast.resource_limits ?
		"whole-B-Rep fast fallback exhausted a configured resource limit" :
		"whole-B-Rep fast fallback did not produce usable geometry");
	    return -1;
	}
	fast.normals.reset();
	assembly.mesh.faces = std::move(fast.faces);
	assembly.mesh.face_count = fast.face_count;
	assembly.mesh.vertices.reset((fastf_t *)fast.points.release());
	assembly.mesh.vertex_count = fast.point_count;
	assembly.rigorous_face_count = 0;
	assembly.face_sources.assign((size_t)fast.face_count, -1);
	for (size_t brep_face = 0; brep_face < fast_face_ranges.size();
		++brep_face) {
	    const repair_fast_face_range &range =
		fast_face_ranges[brep_face];
	    if (!range.present || range.first_face >
		    (size_t)fast.face_count || range.face_count >
		    (size_t)fast.face_count - range.first_face)
		continue;
	    for (size_t face = range.first_face;
		    face < range.first_face + range.face_count; ++face)
		assembly.face_sources[face] = (int)brep_face;
	}
	report->fast_fallback_used_faces = fast.report.completed_faces;
	report->fast_fallback_failed_faces = fast.report.failed_faces;
	if (!report->fast_fallback_triangles)
	    report->fast_fallback_triangles = fast.face_count;
	report->full_fast_fallback_used = 1;
    }
    return 0;
}

void
repair_attempt::recover_display_faces()
{
    if (!settings->use_full_fast_fallback &&
	    settings->use_fast_face_fallback && !failed_faces.empty()) {
	const int64_t fast_start = bu_gettime();
	size_t fast_points_used = 0;
	size_t fast_bytes_used = 0;
	for (int failed_face : failed_faces) {
	    if (assembly.reconstructed_faces.find(failed_face) !=
		    assembly.reconstructed_faces.end())
		continue;
	    const int64_t elapsed_ms = (bu_gettime() - fast_start) / 1000;
	    if (elapsed_ms >= settings->max_fast_time_ms ||
		    fast_points_used >= settings->max_fast_points ||
		    fast_bytes_used >= settings->max_fast_result_bytes)
		break;
	    report->fast_fallback_attempted_faces++;
	    brep_cdt_fast_options fast_options = repair_fast_options(
		settings->max_fast_points - fast_points_used,
		settings->max_fast_result_bytes - fast_bytes_used,
		settings->max_fast_time_ms - (long)elapsed_ms);
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
	    repair_fast_mesh fast(s_cdt, failed_face, fast_options);
	    report->resource_limits |= fast.resource_limits;
	    const bool within_limits = fast.valid_geometry() &&
		(size_t)fast.point_count <=
		    settings->max_fast_points - fast_points_used &&
		    fast.report.result_bytes <=
		    settings->max_fast_result_bytes - fast_bytes_used &&
		(fast.point_count <= INT_MAX - assembly.mesh.vertex_count &&
		fast.face_count <= INT_MAX - assembly.mesh.face_count);
	    if (within_limits && !fast_point_sources.empty())
		fast_point_sources.resize((size_t)fast.point_count, NULL);
	    const int appended = within_limits ? assembly.append(failed_face,
		repair_face_origin::display, fast.faces.get(), fast.face_count,
		(const fastf_t *)fast.points.get(), fast.point_count,
		fast_point_sources.empty() ? NULL : &fast_point_sources) : 0;
	    if (appended) {
		report->fast_fallback_used_faces++;
		report->fast_fallback_triangles += fast.face_count;
		report->fast_fallback_constrained_edges +=
		    constraints.constrained_edges;
		report->fast_fallback_constrained_samples +=
		    constraints.constrained_samples;
		fast_points_used += (size_t)fast.point_count;
		fast_bytes_used += fast.report.result_bytes;
	    } else {
		report->fast_fallback_failed_faces++;
	    }
	}
    }
}

int
repair_attempt::prepare_boundary_repair()
{
    std::set<int> duplicate_sources;
    size_t duplicate_count = 0;
    std::map<int, std::pair<size_t, size_t>> duplicate_source_counts;
    const size_t quarantined_triangles =
	repair_quarantine_duplicate_source_faces(assembly.mesh.faces.get(),
	    assembly.mesh.face_count, assembly.mesh.vertices.get(), &assembly.rigorous_face_count,
	    assembly.face_sources, duplicate_sources, &duplicate_count,
	    &duplicate_source_counts);
    if (quarantined_triangles) {
	assembly.rigorous_sources.assign(assembly.face_sources.begin(),
	    assembly.face_sources.begin() + assembly.rigorous_face_count);
	assembly.approximation_faces.insert(duplicate_sources.begin(),
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
    if (assembly.mesh.face_count <= 0 || assembly.mesh.vertex_count <= 0 || !assembly.mesh.faces.get() ||
	    !assembly.mesh.vertices.get()) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "mesh repair did not obtain usable B-Rep face geometry");
	return -1;
    }

    if (settings->use_full_fast_fallback) {
	struct bg_trimesh_solid_errors edge_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(assembly.mesh.vertex_count, assembly.mesh.face_count,
	    assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &edge_errors);
	const bool edge_closed = !edge_errors.unmatched.count &&
	    !edge_errors.excess.count && !edge_errors.misoriented.count;
	bg_free_trimesh_solid_errors(&edge_errors);
	assembled_mesh_validation pre_weld_validation;
	(void)assembled_mesh_validate(assembly.mesh.vertex_count, assembly.mesh.face_count,
	    assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &pre_weld_validation, false);
	if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
	    bu_log("Whole-display pre-weld: edge closed %d, degenerate %zu, "
		"invalid links %zu, triangles %d\n", (int)edge_closed,
		pre_weld_validation.degenerate_faces,
		pre_weld_validation.invalid_vertex_links, assembly.mesh.face_count);
	std::vector<int> weld_trial;
	size_t welded = 0;
	if (edge_closed && pre_weld_validation.degenerate_faces &&
		pre_weld_validation.invalid_vertex_links) {
	    weld_trial.assign(assembly.mesh.faces.get(),
		assembly.mesh.faces.get() + (size_t)assembly.mesh.face_count * 3);
	    welded = repair_weld_roundoff_vertices(weld_trial.data(),
		assembly.mesh.face_count, assembly.mesh.vertices.get(), assembly.mesh.vertex_count);
	    assembled_mesh_validation post_weld_validation;
	    (void)assembled_mesh_validate(assembly.mesh.vertex_count, assembly.mesh.face_count,
		assembly.mesh.vertices.get(), weld_trial.data(), &post_weld_validation, false);
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
		memcpy(assembly.mesh.faces.get(), weld_trial.data(),
		    weld_trial.size() * sizeof(int));
	    }
	}
	if (welded)
	    bu_log("Normalized %zu whole-display vertices separated only by "
		"coordinate roundoff before local repair\n", welded);
    }
    return 0;
}

void
repair_attempt::trace_input()
{
    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY")) {
	bu_log("Repair input mesh: %d vertices, %d triangles (%d rigorous)\n",
	    assembly.mesh.vertex_count, assembly.mesh.face_count, assembly.rigorous_face_count);
	assembled_mesh_validation input_validation;
	const bool input_geometric = assembled_mesh_validate(assembly.mesh.vertex_count,
	    assembly.mesh.face_count, assembly.mesh.vertices.get(), assembly.mesh.faces.get(),
		&input_validation,
	    false);
	struct bg_trimesh_solid_errors input_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(assembly.mesh.vertex_count, assembly.mesh.face_count,
	    assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &input_errors);
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
	for (int face = 0; face < assembly.mesh.face_count; ++face) {
	    ON_3dPoint points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const int vertex = assembly.mesh.faces[(size_t)face * 3 + corner];
		points[corner] = ON_3dPoint(
		    &assembly.mesh.vertices[(size_t)vertex * 3]);
	    }
	    if (!assembled_mesh_triangle_degenerate(points))
		continue;
	    const int local = (int)degenerate_faces.size();
	    degenerate_faces.push_back(face);
	    for (int corner = 0; corner < 3; ++corner) {
		int first = assembly.mesh.faces[(size_t)face * 3 + corner];
		int second = assembly.mesh.faces[(size_t)face * 3 +
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
	    if ((size_t)input_face < assembly.face_sources.size() &&
		    assembly.face_sources[(size_t)input_face] >= 0) {
		component.source_faces.insert(
		    assembly.face_sources[(size_t)input_face]);
		continue;
	    }
	    if (input_face < assembly.rigorous_face_count &&
		    (size_t)input_face < assembly.rigorous_sources.size()) {
		component.source_faces.insert(
		    assembly.rigorous_sources[(size_t)input_face]);
		continue;
	    }
	    const repair_triangle_key key = repair_triangle_coordinates(
		assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)input_face * 3]);
	    const auto local_source = assembly.provenance.local_chart.find(key);
	    if (local_source != assembly.provenance.local_chart.end())
		component.source_faces.insert(local_source->second.begin(),
		    local_source->second.end());
	    const auto fast_source = assembly.provenance.fast.find(key);
	    if (fast_source != assembly.provenance.fast.end())
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
}

int
repair_attempt::repair_boundaries()
{
    /* A locally reconstructed face is oriented from its damaged source
     * surface, whose normals can disagree with the otherwise authoritative
     * closed shell.  Once exact shared-edge identity proves the assembled
     * candidate closed, synchronize winding transactionally before invoking
     * a geometric repair that could split the valid topology. */
    if (!assembly.reconstructed_faces.empty()) {
	const int synchronized = closed_mesh_orientation_sync(assembly.mesh.faces.get(),
	    assembly.mesh.face_count, assembly.mesh.vertices.get(), assembly.mesh.vertex_count,
		NULL);
	if (synchronized > 0)
	    bu_log("Synchronized %d locally reconstructed triangle "
		"orientations after complete closed-mesh validation\n",
	    synchronized);
    }

    neighborhood.reference_area = repair_mesh_area(
	assembly.mesh.vertices.get(), assembly.mesh.faces.get(), assembly.mesh.face_count);
    bool rigorous_boundary_repair = false;
    if (settings->use_fast_face_fallback ||
	    settings->use_full_fast_fallback) {
	std::vector<int> input_source_faces = assembly.face_sources;
	if (input_source_faces.size() != (size_t)assembly.mesh.face_count)
	    input_source_faces.assign((size_t)assembly.mesh.face_count, -1);
	for (int face = assembly.rigorous_face_count; face < assembly.mesh.face_count;
		++face) {
	    if (input_source_faces[(size_t)face] >= 0)
		continue;
	    const repair_triangle_key key = repair_triangle_coordinates(
		assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)face * 3]);
	    assembly.provenance.source(key, input_source_faces[(size_t)face]);
	}
	const double neighborhood_deviation =
	    settings->max_surface_deviation > 0.0 ?
	    settings->max_surface_deviation : s_cdt->absmax;
	const size_t neighborhood_max_edges =
	    settings->mesh.max_hole_edges > 0 ?
	    (size_t)settings->mesh.max_hole_edges : (size_t)256;
	bool reconstruct_approximate_boundary = false;
	if (!settings->use_full_fast_fallback) {
	    assembled_mesh_validation approximation_validation;
	    (void)assembled_mesh_validate(assembly.mesh.vertex_count, assembly.mesh.face_count,
		assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &approximation_validation,
		    false);
	    struct bg_trimesh_solid_errors boundary_errors =
		BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	    (void)bg_trimesh_solid2(assembly.mesh.vertex_count, assembly.mesh.face_count,
		assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &boundary_errors);
	    const bool preserve_approximate_boundary =
		repair_preserve_approximate_boundary(boundary_errors,
		    approximation_validation.degenerate_faces,
		    approximation_validation.invalid_vertex_links,
		    neighborhood_max_edges);
	    reconstruct_approximate_boundary =
		!preserve_approximate_boundary ||
		closed_boundary_action == REPAIR_RECONSTRUCT_CLOSED_BOUNDARY;
	    bg_free_trimesh_solid_errors(&boundary_errors);
	    if (!reconstruct_approximate_boundary && assembly.rigorous_face_count > 0 &&
		    assembly.mesh.face_count > assembly.rigorous_face_count)
		closed_boundary_action = REPAIR_DEFERRED_CLOSED_BOUNDARY;
	}
	/* First preserve an edge-closed approximation or one with only a bounded,
	 * non-branching open residue.  Reconstructing its perimeter can
	 * flatten a curved cap or select another periodic band.  If local and
	 * generic mesh repair cannot certify it, the caller retries with
	 * rigorous-boundary reconstruction enabled.  A closed candidate whose
	 * flat triangles participate in invalid vertex links goes directly to
	 * reconstruction; generic repair can expand that local defect across the
	 * mesh before it reaches the retry.  Every attempt still requires Manifold,
	 * source coverage, and fidelity validation. */
	if (reconstruct_approximate_boundary) {
	    rigorous_boundary_repair =
		repair_failed_face_from_rigorous_boundary(s_cdt,
		    cdt_array_address(assembly.mesh.faces),
		    &assembly.mesh.face_count, cdt_array_address(assembly.mesh.vertices),
			&assembly.mesh.vertex_count,
		    assembly.rigorous_face_count, input_source_faces,
		    neighborhood_deviation, std::max(neighborhood_max_edges,
			(size_t)1024),
		    &neighborhood.stats);
	}
	/* Failed faces expose independent rigorous rings.  Reconstruct each
	 * source transactionally when every one of its exact shared-edge segments
	 * is present in the rigorous prefix.  Intermediate candidates may remain
	 * open only at the other proved source rings; the final source must close
	 * and validate the complete mesh.  For one failed source, this is also the
	 * authoritative requested-boundary path. */
	if (reconstruct_approximate_boundary && !rigorous_boundary_repair) {
	    std::set<int> approximate_sources;
	    bool attributed = true;
	    for (int face = assembly.rigorous_face_count;
		    face < assembly.mesh.face_count; ++face) {
		const int source = input_source_faces[(size_t)face];
		if (source < 0) {
		    if (getenv("BRLCAD_CDT_DEBUG_REPAIR_TOPOLOGY"))
			bu_log("Source-wise rigorous-boundary repair lacks "
			    "provenance for approximate triangle %d of %d\n",
			    face - assembly.rigorous_face_count,
			    assembly.mesh.face_count - assembly.rigorous_face_count);
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
	    for (int point = 0; point < assembly.mesh.vertex_count; ++point) {
		const repair_point_key key = {
		    assembly.mesh.vertices[(size_t)point * 3],
		    assembly.mesh.vertices[(size_t)point * 3 + 1],
		    assembly.mesh.vertices[(size_t)point * 3 + 2]};
		const auto existing = exact_input_points.find(key);
		if (existing == exact_input_points.end())
		    exact_input_points.emplace(key, point);
		else if (existing->second != point)
		    existing->second = -1;
	    }
	    const auto source_index = [&](ON_3dPoint *point) {
		if (!point)
		    return -1;
		int indexed = assembly.source_vertex(point);
		if (indexed >= 0)
		    return indexed;
		const auto canonical = s_cdt->collapsed_edge_pnts.find(point);
		if (canonical != s_cdt->collapsed_edge_pnts.end()) {
		    indexed = assembly.source_vertex(canonical->second);
		    if (indexed >= 0)
			return indexed;
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
		int trial_face_count = assembly.mesh.face_count;
		int trial_vertex_count = assembly.mesh.vertex_count;
		cdt_owned_array<int> trial_faces((int *)bu_malloc(
		    (size_t)trial_face_count * 3 * sizeof(int),
		    "multi-source rigorous-boundary trial faces"));
		cdt_owned_array<fastf_t> trial_vertices((fastf_t *)bu_malloc(
		    (size_t)trial_vertex_count * 3 * sizeof(fastf_t),
		    "multi-source rigorous-boundary trial vertices"));
		memcpy(trial_faces.get(), assembly.mesh.faces.get(),
		    (size_t)trial_face_count * 3 * sizeof(int));
		memcpy(trial_vertices.get(), assembly.mesh.vertices.get(),
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
				cdt_array_address(trial_faces), &trial_face_count,
				cdt_array_address(trial_vertices),
				&trial_vertex_count, assembly.rigorous_face_count,
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
		    assembly.mesh.faces = std::move(trial_faces);
		    assembly.mesh.face_count = trial_face_count;
		    assembly.mesh.vertices = std::move(trial_vertices);
		    assembly.mesh.vertex_count = trial_vertex_count;
		    input_source_faces.swap(trial_sources);
		    neighborhood.stats = combined_stats;
		    rigorous_boundary_repair = true;
		    bu_log("Reconstructed %zu failed B-Rep face%s from "
			"independent rigorous boundary rings\n",
			approximate_sources.size(),
			approximate_sources.size() == 1 ? "" : "s");
		}
	    }
	}
	neighborhood.applied = rigorous_boundary_repair;
	if (!neighborhood.applied) {
	    size_t paired_boundary_vertices = 0;
	    if (!settings->use_full_fast_fallback &&
		    repair_pair_near_boundary_cracks(assembly.mesh.faces.get(),
			assembly.mesh.face_count,
		    assembly.mesh.vertices.get(), assembly.mesh.vertex_count,
		    assembly.rigorous_face_count, neighborhood_deviation,
		    &paired_boundary_vertices))
		bu_log("Paired %zu near-coincident failed-face boundary "
		    "vertices before local neighborhood repair\n",
		    paired_boundary_vertices);
	    neighborhood.applied =
		repair_degenerate_neighborhoods(cdt_array_address(assembly.mesh.faces),
		    &assembly.mesh.face_count, cdt_array_address(assembly.mesh.vertices),
			&assembly.mesh.vertex_count,
		    assembly.rigorous_face_count, input_source_faces,
		    neighborhood_deviation, std::max(neighborhood_max_edges,
			(size_t)1024),
		    &neighborhood.stats);
	}
	if (neighborhood.applied) {
	    assembly.reconstructed_faces.insert(
		neighborhood.stats.source_faces.begin(),
		neighborhood.stats.source_faces.end());
	    assembly.approximation_faces.insert(
		neighborhood.stats.source_faces.begin(),
		neighborhood.stats.source_faces.end());
	    /* Retained rigorous triangles are recognized by their coordinate keys
	     * before local provenance is consulted.  Recording those keys here
	     * duplicates a large part of the mesh without helping attribution. */
	    for (int face = assembly.rigorous_face_count; face < assembly.mesh.face_count; ++face) {
		const int source = input_source_faces[(size_t)face];
		if (source < 0)
		    continue;
		const repair_triangle_key key = repair_triangle_coordinates(
		    assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)face * 3]);
		assembly.provenance.local_chart[key].insert(source);
	    }
	    if (rigorous_boundary_repair) {
		bu_log("Replaced %d failed-face triangles with %d triangles "
		    "spanning a %d-edge boundary supplied entirely by retained "
		    "rigorous faces\n",
		    neighborhood.stats.removed_faces,
		    neighborhood.stats.added_faces,
		    neighborhood.stats.boundary_edges);
	    } else {
		bu_log("Replaced %d degenerate triangles in %d local "
		    "neighborhoods with %d fixed-boundary triangles "
		    "(maximum interior offset %.17g)\n",
		    neighborhood.stats.removed_faces,
		    neighborhood.stats.components,
		    neighborhood.stats.added_faces,
		    neighborhood.stats.max_center_offset);
	    }
	}
    }

    /* Removing flat triangles from an edge-closed or nearly closed hybrid can
     * turn point contacts between many independent vertex fans into a large
     * set of artificial holes.  Give the topology-preserving, transactional
     * local reconstruction above the first opportunity to fix those defects.
     * If it cannot, and the caller explicitly authorized a whole-display
     * retry, avoid an unbounded sequence of generic hole triangulations when
     * invalid links are disproportionate, flat triangles dominate, or a very
     * large hybrid combines many flat triangles with a bounded edge crack. */
    if (settings->use_full_fast_fallback_if_needed &&
	    !settings->use_full_fast_fallback &&
	    !neighborhood.applied) {
	assembled_mesh_validation hybrid_validation;
	(void)assembled_mesh_validate(assembly.mesh.vertex_count, assembly.mesh.face_count,
	    assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &hybrid_validation, false);
	struct bg_trimesh_solid_errors hybrid_errors =
	    BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
	(void)bg_trimesh_solid2(assembly.mesh.vertex_count, assembly.mesh.face_count,
	    assembly.mesh.vertices.get(), assembly.mesh.faces.get(), &hybrid_errors);
	const size_t unmatched_edges = hybrid_errors.unmatched.count;
	const bool bounded_edge_defects = !hybrid_errors.excess.count &&
	    !hybrid_errors.misoriented.count &&
	    unmatched_edges <= settings->mesh.max_hole_edges;
	bg_free_trimesh_solid_errors(&hybrid_errors);
	const bool hybrid_storage_valid = !hybrid_validation.invalid_indices &&
	    !hybrid_validation.nonfinite_vertices;
	if (hybrid_storage_valid && bounded_edge_defects &&
		repair_hybrid_fallback_preflight(
		    hybrid_validation.degenerate_faces,
		    hybrid_validation.invalid_vertex_links,
		    (size_t)assembly.mesh.face_count,
		    unmatched_edges,
		    settings->mesh.max_hole_edges)) {
	    const size_t degenerate_faces =
		hybrid_validation.degenerate_faces;
	    const size_t invalid_links =
		hybrid_validation.invalid_vertex_links;
	    std::string message = "rigorous hybrid preflight declined " +
		std::to_string(assembly.mesh.face_count) + " triangles: " +
		std::to_string(degenerate_faces) + " flat triangles and " +
		std::to_string(invalid_links) + " invalid vertex links exceed " +
		"the bounded generic repair envelope";
	    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
		BREP_CDT_STAGE_MESH_REPAIR, -1,
		report->source_diagnostic.completed_faces,
		report->source_failed_faces, message.c_str());
	    return -1;
	}
    }

    /* Assembly point identities and its source array are no longer needed.
     * Release their storage before Manifold allocates its topology indices. */
    assembly.release_source_index();
    return 0;
}

int
repair_attempt::repair_mesh()
{
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
    /* A reconstructed mesh may already satisfy every topology and geometry
     * condition.  Preserve it exactly: tolerance welding intended for an
     * open mixed-source mesh can remove valid, very small facets and create a
     * new hole.  The independent fidelity gates still run below. */
    if (closed_boundary_action == REPAIR_DEFERRED_CLOSED_BOUNDARY &&
	    !settings->mesh.union_components) {
	assembled_mesh_validation input_validation;
	const bool input_geometric = assembled_mesh_validate(
	    assembly.mesh.vertex_count, assembly.mesh.face_count, assembly.mesh.vertices.get(),
	    assembly.mesh.faces.get(), &input_validation,
	    !settings->mesh.allow_self_intersections);
	candidate.preserve_input = input_geometric && !bg_trimesh_solid2(
	    assembly.mesh.vertex_count, assembly.mesh.face_count, assembly.mesh.vertices.get(),
	    assembly.mesh.faces.get(), NULL);
    }
    const auto run_mesh_repair = [&]() {
	const int result = bg_trimesh_repair_ex(cdt_array_address(candidate.faces),
	    &candidate.face_count, cdt_array_address(candidate.points), &candidate.vertex_count,
	    assembly.mesh.faces.get(), assembly.mesh.face_count,
		(const point_t *)assembly.mesh.vertices.get(),
	    assembly.mesh.vertex_count, &mesh_settings, &report->mesh);
	if (report->mesh.allocation_failed)
	    report->resource_limits |= BREP_CDT_REPAIR_LIMIT_MEMORY;
	return result;
    };
    int repair_result = 1;
    size_t adaptive_hole_edges = 0;
    if (candidate.preserve_input) {
	report->mesh.input_vertices = assembly.mesh.vertex_count;
	report->mesh.input_faces = assembly.mesh.face_count;
	report->mesh.input_area = repair_mesh_area(assembly.mesh.vertices.get(),
	    assembly.mesh.faces.get(), assembly.mesh.face_count);
	report->mesh.solid = 1;
	report->mesh.manifold_accepted = 1;
    } else {
	repair_result = run_mesh_repair();
	adaptive_hole_edges = repair_result < 0 ?
	    repair_adaptive_hole_edge_budget(settings, &report->mesh) : 0;
	if (adaptive_hole_edges) {
	    report->adaptive_hole_retry_attempted = 1;
	    report->adaptive_hole_edges = adaptive_hole_edges;
	    mesh_settings.max_hole_edges = adaptive_hole_edges;
	    bu_log("Retrying final mesh repair with a bounded %zu-edge hole "
		"ceiling\n", adaptive_hole_edges);
	    repair_result = run_mesh_repair();
	}
	const fastf_t adaptive_hole_area = repair_result < 0 ?
	    repair_adaptive_hole_area_budget(settings, &mesh_settings,
		&report->mesh) : 0.0;
	if (adaptive_hole_area > 0.0) {
	    report->adaptive_hole_area_retry_attempted = 1;
	    report->adaptive_hole_area_percent = adaptive_hole_area;
	    mesh_settings.max_hole_area_percent = adaptive_hole_area;
	    bu_log("Retrying final mesh repair with a bounded %.6g%% hole "
		"area ceiling\n", adaptive_hole_area);
	    repair_result = run_mesh_repair();
	}
    }
    if (neighborhood.applied) {
	report->mesh.removed_faces +=
	    neighborhood.stats.removed_faces;
	report->mesh.added_faces += neighborhood.stats.added_faces;
    }
    if (repair_result == 1) {
	candidate.face_count = assembly.mesh.face_count;
	candidate.vertex_count = assembly.mesh.vertex_count;
	candidate.faces.reset((int *)bu_malloc((size_t)candidate.face_count * 3 *
	    sizeof(int), "certified repaired faces"));
	candidate.points.reset((point_t *)bu_malloc(
	    (size_t)candidate.vertex_count * sizeof(point_t),
	    "certified repaired vertices"));
	memcpy(candidate.faces.get(), assembly.mesh.faces.get(),
	    (size_t)candidate.face_count * 3 * sizeof(int));
	memcpy(candidate.points.get(), assembly.mesh.vertices.get(),
	    (size_t)candidate.vertex_count * sizeof(point_t));
    } else if (repair_result < 0) {
	std::string message =
	    "bounded triangle mesh repair did not produce a solid: " +
	    std::to_string(report->mesh.unmatched_edges) +
	    " unmatched, " + std::to_string(report->mesh.excess_edges) +
	    " excess, " + std::to_string(report->mesh.misoriented_edges) +
	    " misoriented edges, " +
	    std::to_string(report->mesh.invalid_vertex_links) +
	    " invalid vertex links, " +
	    std::to_string(report->mesh.rejected_hole_faces) +
	    " candidate cap faces rejected";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    return 0;
}

int
repair_attempt::prepare_validation()
{
    report->allowed_surface_deviation =
	settings->max_surface_deviation > 0.0 ?
	settings->max_surface_deviation : s_cdt->absmax;
    if (!(report->allowed_surface_deviation > 0.0) ||
	    !std::isfinite(report->allowed_surface_deviation))
	report->allowed_surface_deviation = BN_TOL_DIST;
    fidelity.relaxed_enabled =
	settings->relaxed_fidelity_factor >= 1.0;
    const double relaxed_fidelity_factor = fidelity.relaxed_enabled ?
	settings->relaxed_fidelity_factor : 1.0;
    fidelity.surface_deviation =
	report->allowed_surface_deviation * relaxed_fidelity_factor;
    fidelity.area_change_limit =
	settings->max_area_change_percent * relaxed_fidelity_factor;
    if (!(fidelity.surface_deviation > 0.0) ||
	    !std::isfinite(fidelity.surface_deviation) ||
	    (settings->max_area_change_percent > 0.0 &&
	    !std::isfinite(fidelity.area_change_limit))) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces,
	    "relaxed repair fidelity limit is not finite");
	return -1;
    }
    report->relaxed_fidelity_factor = fidelity.relaxed_enabled ?
	settings->relaxed_fidelity_factor : 0.0;
    report->relaxed_surface_deviation_limit = fidelity.relaxed_enabled ?
	fidelity.surface_deviation : 0.0;
    report->relaxed_area_change_percent_limit =
	fidelity.relaxed_enabled && settings->max_area_change_percent > 0.0 ?
	fidelity.area_change_limit : 0.0;

    surface_queries.bounds.resize((size_t)s_cdt->orig_brep->m_F.Count());
    surface_queries.trees.resize((size_t)s_cdt->orig_brep->m_F.Count());
    surface_queries.contexts.resize((size_t)s_cdt->orig_brep->m_F.Count());
    for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face)
	surface_queries.bounds[(size_t)face] =
	    s_cdt->orig_brep->m_F[face].BoundingBox();

    for (int face = 0; face < assembly.rigorous_face_count; ++face) {
	const repair_triangle_key key = repair_triangle_coordinates(
	    assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)face * 3]);
	preservation.rigorous_counts[key]++;
    }
    return 0;
}

int
repair_attempt::validate_mesh()
{
    report->mesh.output_vertices = candidate.vertex_count;
    report->mesh.output_faces = candidate.face_count;
    report->mesh.output_area = repair_mesh_area(
	(const fastf_t *)candidate.points.get(), candidate.faces.get(),
	candidate.face_count);

    assembled_mesh_validation mesh_validation;
    struct bg_trimesh_solid_errors solid_errors =
	BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    bool geometric_valid = assembled_mesh_validate(
	candidate.vertex_count, candidate.face_count, candidate.vertices(),
	candidate.faces.get(), &mesh_validation,
	!settings->mesh.allow_self_intersections);
    int not_solid = bg_trimesh_solid2(candidate.vertex_count,
	candidate.face_count, (fastf_t *)candidate.vertices(), candidate.faces.get(),
	&solid_errors);
    bg_free_trimesh_solid_errors(&solid_errors);
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
	    for (int face = assembly.rigorous_face_count;
		    face < assembly.mesh.face_count; ++face) {
		const repair_triangle_key key = repair_triangle_coordinates(
		    assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)face * 3]);
		fast_triangles.insert(key);
	    }
	    const auto provenance = [&](int face) {
		const repair_triangle_key key = repair_triangle_coordinates(
		    candidate.vertices(), &candidate.faces[(size_t)face * 3]);
		if (preservation.rigorous_counts.find(key) != preservation.rigorous_counts.end()) {
		    for (int input_face = 0;
			    input_face < assembly.rigorous_face_count; ++input_face) {
			if (key != repair_triangle_coordinates(assembly.mesh.vertices.get(),
				&assembly.mesh.faces[(size_t)input_face * 3]))
			    continue;
			if ((size_t)input_face <
				assembly.rigorous_sources.size())
			    return std::string("rigorous B-Rep face ") +
				std::to_string(assembly.rigorous_sources[
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
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }

    report->mesh.output_volume = bg_trimesh_volume(candidate.faces.get(),
	(size_t)candidate.face_count, candidate.points.get(),
	(size_t)candidate.vertex_count);
    repair_added_patch_stats(report, assembly.mesh.vertices.get(), assembly.mesh.faces.get(),
	assembly.mesh.face_count, candidate.vertices(), candidate.faces.get(),
	candidate.face_count);
    return 0;
}

int
repair_attempt::preserve_rigorous_triangles()
{
    /* A local fallback is allowed to add geometry inside failed-face
     * neighborhoods, but it may not silently simplify unrelated certified
     * faces.  Compare triangle coordinates rather than indices so harmless
     * reindexing, reorientation, and vertex duplication remain acceptable.
     * A bounded edge split is also preservation: mesh repair uses it to mate
     * a rigorous triangle to an already certified hanging boundary sample. */
    std::vector<repair_triangle_key> missing_rigorous_keys;
    std::set<repair_triangle_edge_key> missing_rigorous_edges;
    {
	std::map<repair_triangle_key, size_t> output_triangle_counts;
	for (int face = 0; face < candidate.face_count; ++face) {
	    const repair_triangle_key key = repair_triangle_coordinates(
		candidate.vertices(), &candidate.faces[(size_t)face * 3]);
	    output_triangle_counts[key]++;
	}
	for (const auto &required : preservation.rigorous_counts) {
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
    }
    std::map<repair_triangle_edge_key, std::vector<int>>
	output_faces_by_missing_edge;
    std::vector<repair_triangle_key> output_triangle_keys;
    if (!missing_rigorous_edges.empty()) {
	output_triangle_keys.resize((size_t)candidate.face_count);
	for (int face = 0; face < candidate.face_count; ++face) {
	    output_triangle_keys[(size_t)face] =
		repair_triangle_coordinates(candidate.vertices(),
		&candidate.faces[(size_t)face * 3]);
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
    if (!missing_rigorous_keys.empty()) {
	const std::set<repair_triangle_key> missing_unique(
	    missing_rigorous_keys.begin(), missing_rigorous_keys.end());
	for (int face = 0; face < assembly.rigorous_face_count; ++face) {
	    const repair_triangle_key key = repair_triangle_coordinates(
		assembly.mesh.vertices.get(), &assembly.mesh.faces[(size_t)face * 3]);
	    if (missing_unique.find(key) == missing_unique.end() ||
		    (size_t)face >= assembly.rigorous_sources.size())
		continue;
	    preservation.missing_sources[key].insert(
		assembly.rigorous_sources[(size_t)face]);
	}
    }
    const auto collect_source_faces = [&](const repair_triangle_key &key,
	    std::set<int> &faces) {
	const auto source = preservation.missing_sources.find(key);
	if (source == preservation.missing_sources.end())
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
			    preservation.rigorous_counts.find(output_triangle_keys[
			    (size_t)first_face]) != preservation.rigorous_counts.end())
			continue;
		    for (int second_face : second_candidates->second) {
			if (first_face == second_face ||
				subdivision_faces_used.find(second_face) !=
				subdivision_faces_used.end() ||
				preservation.rigorous_counts.find(output_triangle_keys[
				(size_t)second_face]) !=
				preservation.rigorous_counts.end())
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
			assembly.provenance.local_chart[
			    output_triangle_keys[(size_t)first_face]].insert(
			    source_faces.begin(), source_faces.end());
			assembly.provenance.local_chart[
			    output_triangle_keys[(size_t)second_face]].insert(
			    source_faces.begin(), source_faces.end());
			subdivided = true;
			break;
		    }
		}
	    }
	}
	if (!subdivided)
	    preservation.unresolved.push_back(required);
    }
    assembly.approximation_faces.insert(reconciled_rigorous_faces.begin(),
	reconciled_rigorous_faces.end());
    const size_t missing_rigorous_triangles =
	preservation.unresolved.size();
    double missing_rigorous_area = 0.0;
    std::set<int> missing_rigorous_brep_faces;
    for (const repair_triangle_key &missing : preservation.unresolved) {
	missing_rigorous_area += repair_triangle_key_area(missing);
	collect_source_faces(missing, missing_rigorous_brep_faces);
    }
    report->missing_rigorous_triangles = (int)std::min(
	missing_rigorous_triangles, (size_t)INT_MAX);
    report->retained_rigorous_triangles = assembly.rigorous_face_count -
	report->missing_rigorous_triangles;
    repair_missing_patch_report missing_patch_report;
    if (missing_rigorous_triangles &&
	    !settings->use_full_fast_fallback &&
	    !settings->mesh.union_components) {
	preservation.bounded_replacement = repair_missing_patch_bounded(
	    preservation.unresolved, report->mesh.input_area,
	    settings->mesh, &missing_patch_report);
    }
    if (missing_rigorous_triangles &&
	    !settings->use_full_fast_fallback &&
	    !settings->mesh.union_components &&
	    !preservation.bounded_replacement) {
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
    if (preservation.bounded_replacement) {
	report->replaced_rigorous_components =
	    missing_patch_report.components;
	report->largest_replaced_rigorous_triangles =
	    missing_patch_report.largest_faces;
	report->largest_replaced_boundary_edges =
	    missing_patch_report.largest_boundary_edges;
	report->replaced_rigorous_area = missing_patch_report.total_area;
	report->largest_replaced_rigorous_area =
	    missing_patch_report.largest_area;
	assembly.approximation_faces.insert(missing_rigorous_brep_faces.begin(),
	    missing_rigorous_brep_faces.end());
    }
    return 0;
}

int
repair_attempt::validate_area()
{
    report->reference_area = neighborhood.applied ?
	neighborhood.reference_area : report->mesh.input_area;
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
    const bool use_reference_area =
	neighborhood.stats.inferred_topology;
    const fastf_t guarded_area_change = use_reference_area ?
	report->reference_area_change_percent : report->area_change_percent;
    if (settings->max_area_change_percent > 0.0 &&
	    (!(guarded_area_change <= fidelity.area_change_limit))) {
	std::string message = use_reference_area ?
	    "repaired mesh area differs from display reference by " :
	    "repaired mesh area changed by ";
	message += std::to_string(guarded_area_change) +
	    "% (strict limit " +
	    std::to_string(settings->max_area_change_percent);
	if (fidelity.relaxed_enabled)
	    message += "%, relaxed limit " +
		std::to_string(fidelity.area_change_limit);
	message += "%)";
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, message.c_str());
	return -1;
    }
    if (settings->max_area_change_percent > 0.0 &&
	    guarded_area_change > settings->max_area_change_percent)
	fidelity.relaxed_applied = true;
    return 0;
}

int
repair_attempt::validate_surface()
{
    std::vector<repair_changed_face> changed_faces;
    for (int face = 0; face < candidate.face_count; ++face) {
	const int *triangle = &candidate.faces[(size_t)face * 3];
	const repair_triangle_key key = repair_triangle_coordinates(
	    candidate.vertices(), triangle);
	if (preservation.rigorous_counts.find(key) != preservation.rigorous_counts.end())
	    continue;
	int source_brep_face = -1;
	/* Prefer the unambiguous source face before a global surface query. */
	const bool local_surface_approximation =
	    assembly.provenance.source(key, source_brep_face);
	const ON_3dPoint a(&candidate.vertices()[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&candidate.vertices()[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&candidate.vertices()[(size_t)triangle[2] * 3]);
	const auto direct_surface = assembly.provenance.direct_deviations.find(key);
	changed_faces.push_back({face,
	    0.5 * ON_CrossProduct(b - a, c - a).Length(),
	    source_brep_face, local_surface_approximation,
	    direct_surface != assembly.provenance.direct_deviations.end(),
	    direct_surface != assembly.provenance.direct_deviations.end() ?
	    direct_surface->second : 0.0});
    }
    report->changed_faces = (int)changed_faces.size();

    std::vector<repair_changed_face> sampled_changed_faces =
	repair_sample_faces(changed_faces, sample_limit());

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
	    candidate.vertices(),
	    &candidate.faces[(size_t)changed.index * 3]);
	const auto candidates = assembly.provenance.surface_triangles.find(key);
	if (candidates == assembly.provenance.surface_triangles.end() ||
		candidates->second.empty())
	    continue;
	bool proved = true;
	double maximum = 0.0;
	for (const auto &witness : candidates->second) {
	    const auto mesh = s_cdt->fmeshes.find(witness.first);
	    double deviation = 0.0;
	    if (mesh == s_cdt->fmeshes.end() ||
		    !mesh->second.surface_triangle_deviation(witness.second,
			&deviation)) {
		proved = false;
		break;
	    }
	    maximum = std::max(maximum, deviation);
	}
	if (proved && maximum <= fidelity.surface_deviation) {
	    changed.direct_surface_samples = true;
	    changed.direct_surface_deviation = maximum;
	}
    }

    RTree<size_t, double, 3> input_triangle_index;
    /* Every nonrigorous face present here entered through an explicitly
     * enabled, provenance-tagged local fallback.  After source-surface
     * projection fails, let a small topology repair justify its changed
     * samples against that bounded local interpretation.  The reverse
     * coverage and area gates below still prevent accepting a missing or
     * wholesale replacement region. */
    const int surface_reference_face_count = assembly.mesh.face_count;
    repair_index_triangles(input_triangle_index, assembly.mesh.vertices.get(),
	assembly.mesh.faces.get(), surface_reference_face_count);

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
	sample_limit() - sampled_changed_faces.size();
    double squared_distance_sum = 0.0;
    int worst_deviation_brep_face = -1;
    const std::set<int> local_surface_faces(
	assembly.reconstructed_faces.begin(), assembly.reconstructed_faces.end());
    const bool allow_untrimmed_fidelity =
	settings->allow_untrimmed_surface_match != 0 ||
	settings->use_full_fast_fallback;
    for (const repair_changed_face &changed : sampled_changed_faces) {
	const int *triangle = &candidate.faces[(size_t)changed.index * 3];
	const ON_3dPoint a(&candidate.vertices()[(size_t)triangle[0] * 3]);
	const ON_3dPoint b(&candidate.vertices()[(size_t)triangle[1] * 3]);
	const ON_3dPoint c(&candidate.vertices()[(size_t)triangle[2] * 3]);
	const auto samples = repair_triangle_samples(a, b, c);
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
	    if (candidate.preserve_input) {
		/* The accepted output is an exact copy of this mixed-source
		 * input.  Reverse coverage below still proves that the B-Rep is
		 * represented. */
		matched_local_surface = true;
		report->input_mesh_surface_samples++;
	    } else if (changed.direct_surface_samples) {
		distance = changed.direct_surface_deviation;
		used_untrimmed = true;
		matched_local_surface = distance <=
		    fidelity.surface_deviation;
		if (changed.source_brep_face >= 0 &&
			best_effort_faces.find(changed.source_brep_face) !=
			best_effort_faces.end()) {
		    report->best_effort_reference_samples++;
		    report->max_best_effort_surface_deviation = std::max(
			report->max_best_effort_surface_deviation,
			(fastf_t)distance);
		}
	    } else if (!local_filter.empty()) {
		matched_local_surface = repair_surface_distance(
		    s_cdt->orig_brep, surface_queries.bounds,
		    surface_queries.trees, surface_queries.contexts, samples[sample], fidelity.surface_deviation, true,
		    &distance, &used_untrimmed, NULL, NULL, &local_filter);
	    }
	    bool matched_global_surface = false;
	    if (!matched_local_surface &&
		    !settings->use_full_fast_fallback)
		matched_global_surface = repair_surface_distance(
		    s_cdt->orig_brep, surface_queries.bounds,
		    surface_queries.trees, surface_queries.contexts, samples[sample], fidelity.surface_deviation,
		    allow_untrimmed_fidelity, &distance, &used_untrimmed);
	    if (!matched_local_surface && !matched_global_surface) {
		if (!repair_input_mesh_distance(input_triangle_index,
			assembly.mesh.vertices.get(), assembly.mesh.faces.get(),
			samples[sample], fidelity.surface_deviation,
			&distance)) {
		    const double diagnostic_limit =
			repair_diagnostic_distance_limit(fidelity.surface_deviation);
		    double surface_distance =
			std::numeric_limits<double>::infinity();
		    double mesh_distance =
			std::numeric_limits<double>::infinity();
		    bool diagnostic_untrimmed = false;
		    int surface_face = -1;
		    const bool have_surface_distance =
			(!settings->use_full_fast_fallback ||
			 !local_filter.empty()) && repair_surface_distance(
			    s_cdt->orig_brep, surface_queries.bounds, surface_queries.trees,
			    surface_queries.contexts, samples[sample], diagnostic_limit,
			    allow_untrimmed_fidelity, &surface_distance,
			    &diagnostic_untrimmed, NULL, &surface_face,
			    settings->use_full_fast_fallback ? &local_filter :
			    NULL);
		    const bool have_mesh_distance = repair_input_mesh_distance(
			input_triangle_index, assembly.mesh.vertices.get(),
			assembly.mesh.faces.get(), samples[sample], diagnostic_limit,
			&mesh_distance);
		    if (have_surface_distance || have_mesh_distance) {
			int diagnostic_face = -1;
			if (have_surface_distance) {
			    distance = surface_distance;
			    diagnostic_face = surface_face;
			} else {
			    distance = mesh_distance;
			}
			if (have_surface_distance && have_mesh_distance &&
				mesh_distance < distance) {
			    distance = mesh_distance;
			    diagnostic_face = -1;
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
			best_effort_faces.find(changed.source_brep_face) !=
			best_effort_faces.end()) {
		    /* The approximation is accepted against the retained surface
		     * mesh, but still quantify how far its chord samples lie from
		     * the source face.  A miss means no face projection was found
		     * within four requested tolerances; report it without confusing
		     * this explicitly tagged interpretation with an exact match. */
		    const double support_limit =
			repair_diagnostic_distance_limit(report->allowed_surface_deviation);
		    std::set<int> support_filter = {
			changed.source_brep_face
		    };
		    double support_distance =
			std::numeric_limits<double>::infinity();
		    bool support_untrimmed = false;
		    if (!repair_surface_distance(s_cdt->orig_brep,
			    surface_queries.bounds, surface_queries.trees, surface_queries.contexts,
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
	    fidelity.surface_deviation) {
	std::string message = "repaired surface failed B-Rep deviation check: " +
	    std::to_string(report->deviation_projection_failures) +
	    " projection failures, maximum " +
	    std::to_string(report->max_surface_deviation) +
	    " (strict limit " +
	    std::to_string(report->allowed_surface_deviation);
	if (fidelity.relaxed_enabled)
	    message += ", relaxed limit " +
		std::to_string(fidelity.surface_deviation);
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
	fidelity.relaxed_applied = true;
    return 0;
}

int
repair_attempt::validate_coverage()
{
    /* Output-to-source deviation alone can accept a small reconstructed
     * subset of a much larger model, particularly when matching underlying
     * untrimmed surfaces is explicitly allowed.  Sample the assembled
     * input in the reverse direction so missing target regions are
     * also bounded by the same fidelity limit. */
    RTree<size_t, double, 3> repaired_triangle_index;
    repair_index_triangles(repaired_triangle_index, candidate.vertices(),
	candidate.faces.get(), candidate.face_count);
    std::vector<repair_changed_face> reference_faces;
    const int coverage_reference_face_count = assembly.mesh.face_count;
    reference_faces.reserve((size_t)coverage_reference_face_count);
    for (int face = 0; face < coverage_reference_face_count; ++face) {
	const int *triangle = &assembly.mesh.faces[(size_t)face * 3];
	const ON_3dPoint a(&assembly.mesh.vertices[
	    (size_t)triangle[0] * 3]);
	const ON_3dPoint b(&assembly.mesh.vertices[
	    (size_t)triangle[1] * 3]);
	const ON_3dPoint c(&assembly.mesh.vertices[
	    (size_t)triangle[2] * 3]);
	reference_faces.push_back({face,
	    0.5 * ON_CrossProduct(b - a, c - a).Length()});
    }
    const std::vector<repair_changed_face> sampled_reference_faces =
	repair_sample_faces(reference_faces, sample_limit());
    double coverage_squared_distance_sum = 0.0;
    int worst_coverage_brep_face = -1;
    const auto sample_output_coverage = [&](const ON_3dPoint &sample,
	    int source_brep_face) {
	double distance = 0.0;
	report->coverage_samples++;
	if (!repair_input_mesh_distance(repaired_triangle_index,
		candidate.vertices(), candidate.faces.get(), sample,
		fidelity.surface_deviation, &distance)) {
	    report->coverage_failures++;
	    const double diagnostic_limit =
		repair_diagnostic_distance_limit(fidelity.surface_deviation);
	    if (repair_input_mesh_distance(repaired_triangle_index,
		    candidate.vertices(), candidate.faces.get(), sample,
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

    /* A faulty tessellation can omit a source face before either mesh
     * reference is constructed.  Independently require coverage of referenced
     * 3-D boundary curves; unused wire edges carry no surface obligation. */
    for (const auto &sample : repair_source_boundary_samples(
	    *s_cdt->orig_brep, sample_limit()))
	sample_output_coverage(sample.point, sample.face);
    for (const auto &sample : repair_source_interior_samples(
	    *s_cdt->orig_brep, sample_limit()))
	sample_output_coverage(sample.point, sample.face);

    /* The ordinary deterministic coverage budget may omit a few triangles
     * from a large mesh.  A locally replaced neighborhood is exceptional and
     * small by construction, so explicitly test its centroid and edge
     * midpoints against the final mesh before accepting the interpretation. */
    if (preservation.bounded_replacement) {
	for (const repair_triangle_key &missing : preservation.unresolved) {
	    const ON_3dPoint a(missing[0].data());
	    const ON_3dPoint b(missing[1].data());
	    const ON_3dPoint c(missing[2].data());
	    int source_brep_face = -1;
	    const auto source = preservation.missing_sources.find(missing);
	    if (source != preservation.missing_sources.end() &&
		    source->second.size() == 1)
		source_brep_face = *source->second.begin();
	    for (const ON_3dPoint &sample : repair_triangle_samples(a, b, c))
		sample_output_coverage(sample, source_brep_face);
	}
    }
    size_t remaining_coverage_extra =
	sample_limit() - sampled_reference_faces.size();
    for (const repair_changed_face &reference : sampled_reference_faces) {
	const int *triangle =
	    &assembly.mesh.faces[(size_t)reference.index * 3];
	const ON_3dPoint a(&assembly.mesh.vertices[
	    (size_t)triangle[0] * 3]);
	const ON_3dPoint b(&assembly.mesh.vertices[
	    (size_t)triangle[1] * 3]);
	const ON_3dPoint c(&assembly.mesh.vertices[
	    (size_t)triangle[2] * 3]);
	const auto samples = repair_triangle_samples(a, b, c);
	const size_t face_sample_count = 1 +
	    std::min((size_t)3, remaining_coverage_extra);
	remaining_coverage_extra -= face_sample_count - 1;
	int source_brep_face = -1;
	if (reference.index >= 0 &&
		(size_t)reference.index < assembly.rigorous_sources.size())
	    source_brep_face = assembly.rigorous_sources[
		(size_t)reference.index];
	for (size_t sample = 0; sample < face_sample_count; ++sample) {
	    if (candidate.preserve_input) {
		report->coverage_samples++;
		continue;
	    }
	    sample_output_coverage(samples[sample], source_brep_face);
	}
    }
    const size_t covered_samples = report->coverage_samples -
	report->coverage_failures;
    if (covered_samples)
	report->rms_coverage_deviation = std::sqrt(
	    coverage_squared_distance_sum / (double)covered_samples);
    if (report->coverage_failures || report->max_coverage_deviation >
	    fidelity.surface_deviation) {
	std::string message = "repaired surface failed input coverage check: " +
	    std::to_string(report->coverage_failures) +
	    " coverage failures, maximum " +
	    std::to_string(report->max_coverage_deviation) +
	    " (strict limit " +
	    std::to_string(report->allowed_surface_deviation);
	if (fidelity.relaxed_enabled)
	    message += ", relaxed limit " +
		std::to_string(fidelity.surface_deviation);
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
	fidelity.relaxed_applied = true;
    report->relaxed_fidelity_applied = fidelity.relaxed_applied ? 1 : 0;
    return 0;
}

int
repair_attempt::publish()
{
    assembly.mesh.faces.reset();
    assembly.mesh.vertices.reset();
    s_cdt->certified.clear();
    s_cdt->certified.faces = std::move(candidate.faces);
    s_cdt->certified.face_count = candidate.face_count;
    s_cdt->certified.vertices.reset((fastf_t *)candidate.points.release());
    s_cdt->certified.vertex_count = candidate.vertex_count;
    s_cdt->certified.repaired = true;
    s_cdt->status = BREP_CDT_SOLID;
    if (report->relaxed_fidelity_applied)
	report->approximation_tier =
	    BREP_CDT_REPAIR_APPROX_RELAXED_FIDELITY;
    else if (report->full_fast_fallback_used)
	report->approximation_tier = BREP_CDT_REPAIR_APPROX_FULL_FAST;
    else if (report->mesh.added_faces > 0 ||
	    !assembly.reconstructed_faces.empty() ||
	    preservation.bounded_replacement ||
	    !s_cdt->approximated_edges.empty())
	report->approximation_tier = BREP_CDT_REPAIR_APPROX_LOCAL_MESH;
    else if (report->fast_fallback_used_faces > 0)
	report->approximation_tier =
	    BREP_CDT_REPAIR_APPROX_CONSTRAINED_FACE;
    if (report->full_fast_fallback_used ||
	    (report->relaxed_fidelity_applied &&
	    assembly.approximation_faces.empty())) {
	assembly.approximation_faces.clear();
	for (int face = 0; face < s_cdt->orig_brep->m_F.Count(); ++face)
	    assembly.approximation_faces.insert(face);
    }
    std::set<int> approximation_edges;
    for (int face_index : assembly.approximation_faces) {
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
	assembly.approximation_faces.size(), (size_t)INT_MAX);
    report->approximation_edges = (int)std::min(
	approximation_edges.size(), (size_t)INT_MAX);
    if (settings->provenance && report->approximation_tier !=
	    BREP_CDT_REPAIR_APPROX_NONE) {
	const std::vector<int> face_indices(assembly.approximation_faces.begin(),
	    assembly.approximation_faces.end());
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
    if (report->adaptive_hole_area_retry_attempted)
	message += "; adaptive hole area ceiling " +
	    std::to_string(report->adaptive_hole_area_percent) + "%";
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIRED,
	BREP_CDT_STAGE_MESH_REPAIR, -1,
	report->source_diagnostic.completed_faces,
	report->source_failed_faces, message.c_str());
    return 0;
}

int
repair_attempt::run()
{
    if (prepare_input() < 0)
	return -1;
    retain_adaptive_faces();
    recover_clean_charts();
    recover_surface_patches();
    recover_topological_disks();
    if (recover_full_display() < 0)
	return -1;
    recover_display_faces();
    if (prepare_boundary_repair() < 0)
	return -1;
    trace_input();
    if (repair_boundaries() < 0 || repair_mesh() < 0 ||
	    prepare_validation() < 0 || validate_mesh() < 0 ||
	    preserve_rigorous_triangles() < 0 || validate_area() < 0 ||
	    validate_surface() < 0 || validate_coverage() < 0)
	return -1;
    return publish();
}

} // namespace

static int
brep_cdt_repair_attempt(struct ON_Brep_CDT_State *s_cdt,
	const struct brep_cdt_repair_settings *settings,
	struct brep_cdt_repair_report *report,
	repair_closed_boundary_action &closed_boundary_action,
	bool preserve_pullback_samples = false)
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
    if (s_cdt->diagnostic.result == BREP_CDT_RESULT_REFINEMENT_LIMIT ||
	    report->source_diagnostic.result == BREP_CDT_RESULT_REFINEMENT_LIMIT)
	report->resource_limits |= BREP_CDT_REPAIR_LIMIT_REFINEMENT;
    for (const auto &failure : s_cdt->failed_face_diagnostics) {
	if (failure.second.result == BREP_CDT_RESULT_REFINEMENT_LIMIT)
	    report->resource_limits |= BREP_CDT_REPAIR_LIMIT_REFINEMENT;
    }
    report->source_failed_faces = (int)s_cdt->failed_face_indices.size();
    report->bounded_edge_approximation_edges = (int)std::min(
	s_cdt->approximated_edges.size(), (size_t)INT_MAX);
    for (const auto &edge : s_cdt->approximated_edges)
	report->max_bounded_edge_deviation = std::max(
	    report->max_bounded_edge_deviation, edge.second);
    if (s_cdt->status == BREP_CDT_SOLID && s_cdt->certified.faces &&
	    s_cdt->certified.vertices &&
	    !settings->mesh.require_manifold &&
	    !settings->mesh.union_components &&
	    s_cdt->approximated_edges.empty()) {
	report->mesh.input_vertices = s_cdt->certified.vertex_count;
	report->mesh.input_faces = s_cdt->certified.face_count;
	report->mesh.output_vertices = s_cdt->certified.vertex_count;
	report->mesh.output_faces = s_cdt->certified.face_count;
	report->mesh.solid = 1;
	return 1;
    }
    const char *settings_error = repair_settings_error(settings);
    if (settings_error) {
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    report->source_diagnostic.completed_faces,
	    report->source_failed_faces, settings_error);
	return -1;
    }

    repair_attempt attempt(s_cdt, settings, report, closed_boundary_action,
	preserve_pullback_samples);
    return attempt.run();
}

static int
brep_cdt_repair(struct ON_Brep_CDT_State *s_cdt,
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
    unsigned int resource_limits = 0;
    const auto run_repair_attempt = [&](const brep_cdt_repair_settings *opts) {
	repair_closed_boundary_action closed_boundary_action =
	    REPAIR_PRESERVE_CLOSED_BOUNDARY;
	int result = brep_cdt_repair_attempt(s_cdt, opts, active_report,
	    closed_boundary_action);
	resource_limits |= active_report->resource_limits;
	/* Edge closure alone does not prove a manifold: flat facets can join
	 * unrelated vertex fans.  Keep the existing rigorous-boundary repair
	 * available after the less invasive candidate fails certification. */
	if (result < 0 && closed_boundary_action == REPAIR_DEFERRED_CLOSED_BOUNDARY &&
		!(active_report->resource_limits & BREP_CDT_REPAIR_LIMIT_MEMORY)) {
	    closed_boundary_action = REPAIR_RECONSTRUCT_CLOSED_BOUNDARY;
	    bu_log("Retrying failed closed approximation with rigorous-boundary "
		"reconstruction\n");
	    result = brep_cdt_repair_attempt(s_cdt, opts, active_report,
		closed_boundary_action);
	    resource_limits |= active_report->resource_limits;
	}
	/* Simplifying a repaired pcurve is suitable for display, but can leave
	 * a long edge opposite a finely sampled neighboring boundary.  Retry a
	 * small open residue on a closed source before giving up on assembly. */
	if (result < 0 && opts && opts->use_full_fast_fallback &&
	    active_report->full_fast_fallback_used &&
	    active_report->mesh.unmatched_edges > 0 &&
	    (size_t)active_report->mesh.unmatched_edges <=
		opts->mesh.max_hole_edges &&
	    !active_report->mesh.excess_edges &&
	    !active_report->mesh.misoriented_edges &&
	    s_cdt && s_cdt->orig_brep && s_cdt->orig_brep->IsValid() &&
	    s_cdt->orig_brep->IsSolid() &&
	    cdt_topology_references_safe(s_cdt->orig_brep, NULL)) {
	    result = brep_cdt_repair_attempt(s_cdt, opts, active_report,
		closed_boundary_action, true);
	    active_report->pullback_retry_attempted = 1;
	    active_report->pullback_retry_applied = result >= 0 ? 1 : 0;
	}
	resource_limits |= active_report->resource_limits;
	active_report->resource_limits = resource_limits;
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
    const bool automatic_full_fast = settings &&
	settings->use_full_fast_fallback_if_needed &&
	!settings->use_full_fast_fallback;
    int result = run_repair_attempt(settings);
    if (!automatic_full_fast)
	return result;
    const int rigorous_result = result;
    const struct brep_cdt_repair_report rigorous_report = *active_report;
    if (result < 0) {
	struct brep_cdt_repair_settings fallback_settings = *settings;
	fallback_settings.use_full_fast_fallback = 1;
	fallback_settings.use_full_fast_fallback_if_needed = 0;
	fallback_settings.try_invalid_brep = 0;
	/* The outer healing transaction owns cap construction and its area
	 * ceiling.  A mesh-only retry must disable both topology options. */
	fallback_settings.max_planar_cap_area_percent = 0.0;
	result = run_repair_attempt(&fallback_settings);
    }
    active_report->rigorous_first_attempted = 1;
    active_report->rigorous_first_result = rigorous_result;
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
    return result;
}

int
ON_Brep_CDT_Repair(struct ON_Brep_CDT_State *s_cdt,
	const struct brep_cdt_repair_settings *settings,
	struct brep_cdt_repair_report *report)
{
    struct brep_cdt_repair_report local_report = BREP_CDT_REPAIR_REPORT_INIT;
    struct brep_cdt_repair_report *active = report ? report : &local_report;
    const int original_result = brep_cdt_repair(s_cdt, settings, active);
    if (original_result >= 0 || !s_cdt || !s_cdt->orig_brep || !settings ||
	!settings->try_invalid_brep || settings->use_full_fast_fallback ||
	active->source_diagnostic.result != BREP_CDT_RESULT_INVALID_BREP)
	return original_result;

    active->healing.attempted = 1;
    double tolerance = BN_TOL_DIST;
    if (settings->max_surface_deviation > 0.0)
	tolerance = std::min(tolerance, settings->max_surface_deviation);
    cdt_healing healing;
    const bool have_candidate = cdt_heal_topology(*s_cdt->orig_brep,
	tolerance, settings->max_fast_points, settings->max_fast_result_bytes,
	settings->max_fast_time_ms, healing,
	settings->max_planar_cap_area_percent > 0.0);
    active->healing.limited = healing.limited ? 1 : 0;
    if (healing.limited)
	active->resource_limits |= BREP_CDT_REPAIR_LIMIT_REFINEMENT;
    active->healing.restored_outer_loops = healing.outer_loops;
    active->healing.corrected_loop_roles = healing.loop_roles;
    active->healing.reoriented_faces = healing.orientations;
    active->healing.removed_unused_edges = healing.unused_edges;
    active->healing.max_edge_deviation = healing.max_deviation;
    active->healing.capped_loops = healing.capped_loops;
    active->healing.cap_area_bound = healing.cap_area_bound;
    if (!have_candidate)
	return original_result;

    std::unique_ptr<ON_Brep_CDT_State, decltype(&ON_Brep_CDT_Destroy)> candidate(
	ON_Brep_CDT_Create(healing.brep.get(), s_cdt->name), ON_Brep_CDT_Destroy);
    ON_Brep_CDT_Tol_Set(candidate.get(), &s_cdt->tol);
    ON_Brep_CDT_Face_Time_Limit_Set(candidate.get(), s_cdt->max_face_time_ms);
    ON_Brep_CDT_Tessellate(candidate.get(), 0, NULL);

    struct provenance_capture {
	cdt_healing *healing;
	bool valid = true;
    } capture = {&healing};
    struct brep_cdt_repair_settings candidate_settings = *settings;
    if (healing.capped_loops)
	candidate_settings.mesh.require_manifold = 1;
    candidate_settings.provenance = [](int, const int *faces, size_t face_count,
	    const int *edges, size_t edge_count, void *data) {
	auto *context = static_cast<provenance_capture *>(data);
	for (size_t i = 0; i < face_count; ++i) {
	    if (faces[i] < 0 || faces[i] >= context->healing->brep->m_F.Count())
		context->valid = false;
	    else
		context->healing->faces.insert(faces[i]);
	}
	for (size_t i = 0; i < edge_count; ++i) {
	    if (edges[i] < 0 || (size_t)edges[i] >= context->healing->original_edges.size()) {
		context->valid = false;
		continue;
	    }
	    context->healing->edges.insert(context->healing->original_edges[(size_t)edges[i]]);
	}
    };
    candidate_settings.provenance_data = &capture;
    struct brep_cdt_repair_report candidate_report = BREP_CDT_REPAIR_REPORT_INIT;

    const auto reject_candidate = [&](const std::string &reason) {
	/* The diagnostic describes the healed candidate, so its validation
	 * measurements must accompany it.  Keep the original topology failure
	 * and healing history identifiable without reporting stale tolerances
	 * or edge counts from the preceding repair attempt. */
	candidate_report.resource_limits |= active->resource_limits;
	candidate_report.healing = active->healing;
	candidate_report.source_diagnostic = active->source_diagnostic;
	candidate_report.source_failed_faces = active->source_failed_faces;
	*active = candidate_report;
	const std::string message = "topology healing candidate rejected: " + reason;
	cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIR_FAILED,
	    BREP_CDT_STAGE_MESH_REPAIR, -1,
	    active->source_diagnostic.completed_faces,
	    active->source_failed_faces, message.c_str());
	return original_result;
    };
    if (brep_cdt_repair(candidate.get(), &candidate_settings, &candidate_report) < 0) {
	struct brep_cdt_diagnostic diagnostic;
	if (ON_Brep_CDT_Diagnostic(&diagnostic, candidate.get()) == 0)
	    return reject_candidate(diagnostic.message);
	return reject_candidate("mesh certification failed");
    }
    if (!capture.valid || !candidate_report.mesh.solid ||
	(settings->mesh.require_manifold && !candidate_report.mesh.manifold_accepted) ||
	!candidate->certified.faces || !candidate->certified.vertices)
	return reject_candidate("mesh certification or provenance is incomplete");

    if (healing.capped_loops) {
	const double output_area = repair_mesh_area(candidate->certified.vertices.get(),
	    candidate->certified.faces.get(), candidate->certified.face_count);
	const double remaining_area = output_area - healing.cap_area_bound;
	active->healing.cap_area_percent = remaining_area > 0.0 ?
	    100.0 * healing.cap_area_bound / remaining_area :
	    std::numeric_limits<double>::infinity();
	if (!candidate_report.mesh.manifold_accepted ||
	    !std::isfinite(active->healing.cap_area_percent) ||
	    active->healing.cap_area_percent > settings->max_planar_cap_area_percent)
	    return reject_candidate("planar cap exceeds its area ceiling or lacks Manifold acceptance");
	/* Added faces have no original face identity.  Their boundary edges and
	 * adjacent source faces carry provenance, with the new area reported
	 * separately. */
	healing.faces.erase(healing.faces.lower_bound(s_cdt->orig_brep->m_F.Count()),
	    healing.faces.end());
    }

    for (int fi : healing.faces) {
	if (fi < 0 || fi >= s_cdt->orig_brep->m_F.Count())
	    return original_result;
	const ON_BrepFace &face = s_cdt->orig_brep->m_F[fi];
	for (int li = 0; li < face.LoopCount(); ++li) {
	    const ON_BrepLoop &loop = *face.Loop(li);
	    for (int ti = 0; ti < loop.TrimCount(); ++ti)
		if (loop.Trim(ti)->m_ei >= 0) healing.edges.insert(loop.Trim(ti)->m_ei);
	}
    }
    candidate_report.resource_limits |= active->resource_limits;
    candidate_report.healing = active->healing;
    candidate_report.healing.applied = 1;
    candidate_report.source_diagnostic = active->source_diagnostic;
    candidate_report.source_failed_faces = active->source_failed_faces;
    candidate_report.approximation_faces = (int)healing.faces.size();
    candidate_report.approximation_edges = (int)healing.edges.size();
    if (candidate_report.approximation_tier == BREP_CDT_REPAIR_APPROX_NONE)
	candidate_report.approximation_tier = BREP_CDT_REPAIR_APPROX_TOPOLOGY;

    /* Only the certified export survives the temporary healed B-Rep.  No
     * cached mesh may retain pointers into its curves, faces, or vertices. */
    auto source_failed_faces = std::move(s_cdt->failed_face_indices);
    auto source_face_diagnostics = std::move(s_cdt->failed_face_diagnostics);
    cdt_state_reset(s_cdt);
    s_cdt->failed_face_indices = std::move(source_failed_faces);
    s_cdt->failed_face_diagnostics = std::move(source_face_diagnostics);
    s_cdt->certified = std::move(candidate->certified);
    s_cdt->certified.clear_normals();
    s_cdt->certified.repaired = true;
    s_cdt->status = BREP_CDT_SOLID;
    s_cdt->repair_source_valid = true;
    s_cdt->repair_source_diagnostic = candidate_report.source_diagnostic;
    *active = candidate_report;
    const std::string message = "certified mesh after topology healing: " +
	std::to_string(healing.outer_loops) + " outer loops restored, " +
	std::to_string(healing.loop_roles) + " loop roles corrected, " +
	std::to_string(healing.orientations) + " face orientations corrected, " +
	std::to_string(healing.unused_edges) + " unused edges ignored" +
	(healing.capped_loops ? ", planar caps added: " +
	    std::to_string(healing.capped_loops) : "");
    cdt_diagnostic_set(s_cdt, BREP_CDT_RESULT_REPAIRED,
	BREP_CDT_STAGE_MESH_REPAIR, -1, s_cdt->orig_brep->m_F.Count(), 0,
	message.c_str());
    if (settings->provenance) {
	const std::vector<int> faces(healing.faces.begin(), healing.faces.end());
	const std::vector<int> edges(healing.edges.begin(), healing.edges.end());
	settings->provenance(active->approximation_tier,
	    faces.empty() ? NULL : faces.data(), faces.size(),
	    edges.empty() ? NULL : edges.data(), edges.size(), settings->provenance_data);
    }
    return 0;
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
