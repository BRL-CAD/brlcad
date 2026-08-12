/*           B R E P _ C D T _ C Y L I N D E R _ C H A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "brep/cdt.h"
#include "../cdt/chart.h"

static bool
add_iso_trim(ON_Brep &brep, ON_BrepLoop &loop, const ON_Surface &surface,
	int start_vertex, int end_vertex, const ON_2dPoint &start,
	const ON_2dPoint &end)
{
    const bool vary_u = std::fabs(end.x - start.x) >
	std::fabs(end.y - start.y);
    const int direction = vary_u ? 0 : 1;
    ON_Curve *edge_curve = surface.IsoCurve(direction,
	vary_u ? start.y : start.x);
    if (!edge_curve)
	return false;
    const double first = start[direction];
    const double second = end[direction];
    if (!edge_curve->Trim(ON_Interval(std::min(first, second),
	    std::max(first, second)))) {
	delete edge_curve;
	return false;
    }
    if (second < first)
	edge_curve->Reverse();
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[start_vertex],
	brep.m_V[end_vertex], brep.AddEdgeCurve(edge_curve));
    edge.m_tolerance = 1.0e-8;
    ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
    trim_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	brep.AddTrimCurve(trim_curve));
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = surface.IsIsoparametric(*trim_curve);
    trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-8;
    return true;
}

static bool
exercise_cylinder(const ON_3dPoint &origin, ON_3dVector axis,
	double radius, double height)
{
    if (!axis.Unitize())
	return false;
    const ON_Plane plane(origin, axis);
    const ON_Cylinder cylinder(ON_Circle(plane, radius), height);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (!cylinder.IsValid() || 2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const ON_Interval full_angular = surface->Domain(0);
    const ON_Interval angular(full_angular.ParameterAt(0.17),
	full_angular.ParameterAt(0.83));
    if (!surface->Trim(0, angular)) {
	delete surface;
	return false;
    }

    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    ON_BrepLoop &loop = brep->NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(udom.Min(), vdom.Min()),
	ON_2dPoint(udom.Max(), vdom.Min()),
	ON_2dPoint(udom.Max(), vdom.Max()),
	ON_2dPoint(udom.Min(), vdom.Max())
    };
    int vertex_indices[4];
    std::vector<ON_3dPoint> vertex_points(4);
    for (int i = 0; i < 4; ++i) {
	vertex_points[(size_t)i] = surface->PointAt(corners[i].x,
	    corners[i].y);
	vertex_indices[i] = brep->NewVertex(
	    vertex_points[(size_t)i]).m_vertex_index;
    }
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	if (!add_iso_trim(*brep, loop, *surface, vertex_indices[i],
		vertex_indices[next], corners[i], corners[next]))
	    return false;
    }

    if (!cdt_face_uses_cylinder_chart(face) ||
	    !cdt_face_uses_topology_chart(face)) {
	std::cerr << "trimmed cylinder was not classified" << std::endl;
	return false;
    }
    ON_BrepTrim *first_trim = loop.Trim(0);
    if (!first_trim)
	return false;
    const ON_BrepTrim::TYPE original_type = first_trim->m_type;
    first_trim->m_type = ON_BrepTrim::seam;
    const bool seam_rejected = !cdt_face_uses_cylinder_chart(face);
    first_trim->m_type = original_type;
    if (!seam_rejected) {
	std::cerr << "periodic seam was accepted without chart copies"
	    << std::endl;
	return false;
    }

    std::vector<std::pair<double, double>> native_points;
    std::vector<int> outer;
    std::vector<int> steiner;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices;
    for (int i = 0; i < 4; ++i) {
	outer.push_back((int)native_points.size());
	native_points.push_back(std::make_pair(corners[i].x,
	    corners[i].y));
	points_3d.push_back(&vertex_points[(size_t)i]);
	topology_vertices.push_back(vertex_indices[i]);
    }
    outer.push_back(outer[0]);
    for (int u = 1; u <= 3; ++u) {
	for (int v = 1; v <= 3; ++v) {
	    steiner.push_back((int)native_points.size());
	    native_points.push_back(std::make_pair(
		udom.ParameterAt(0.2 * u), vdom.ParameterAt(0.25 * v)));
	    points_3d.push_back(NULL);
	    topology_vertices.push_back(CDT_TOPOLOGY_ID_NONE);
	}
    }

    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), steiner, std::vector<int>(),
	    points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CYLINDER ||
	    chart.outer.size() != 4 || chart.steiner.size() != 9) {
	std::cerr << "cylinder chart build failed: " << chart.failure()
	    << std::endl;
	return false;
    }
    for (size_t i = 0; i < chart.vertices.size(); ++i) {
	if (chart.vertices[i].native_point != (long)i ||
		chart.vertices[i].topo_vertex != topology_vertices[i]) {
	    std::cerr << "cylinder chart lost source identity" << std::endl;
	    return false;
	}
    }
    const double scale = std::max(radius, std::fabs(height));
    for (const std::pair<double, double> &point : native_points) {
	const ON_2dPoint native(point.first, point.second);
	ON_2dPoint chart_uv;
	ON_2dPoint round_trip;
	const bool chart_mapped = chart.native_to_chart(native, chart_uv);
	const bool native_mapped = chart_mapped &&
	    chart.chart_to_native(chart_uv, round_trip);
	if (!native_mapped) {
	    std::cerr << "cylinder chart mapping failed at (" << native.x
		<< ", " << native.y << "), chart status "
		<< chart_mapped << std::endl;
	    return false;
	}
	const ON_3dPoint expected = surface->PointAt(native.x, native.y);
	const ON_3dPoint actual = surface->PointAt(round_trip.x,
	    round_trip.y);
	if (actual.DistanceTo(expected) > 1.0e-9 * scale) {
	    std::cerr << "cylinder chart round trip exceeded tolerance"
		<< std::endl;
	    return false;
	}
    }
    return true;
}

static bool
exercise_full_cylinder(const ON_3dPoint &origin, ON_3dVector axis,
	double radius, double height)
{
    if (!axis.Unitize())
	return false;
    const ON_Plane plane(origin, axis);
    const ON_Cylinder cylinder(ON_Circle(plane, radius), height);
    std::unique_ptr<ON_Brep> brep(ON_BrepCylinder(cylinder, true, true));
    if (!brep || !brep->IsValid() || !brep->IsSolid()) {
	std::cerr << "invalid full cylinder B-Rep" << std::endl;
	return false;
    }
    const ON_BrepFace *side = NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep->m_F[face_index];
	if (face.SurfaceOf() && face.SurfaceOf()->IsCylinder(NULL, 0.05)) {
	    side = &face;
	    break;
	}
    }
    if (!side || !cdt_face_has_seam(*side) ||
	    !cdt_face_uses_cylinder_chart(*side)) {
	std::cerr << "full cylinder seam was not classified"
	    << (side ? std::string(": closed=") +
		(side->SurfaceOf()->IsClosed(0) ? "u" : "") +
		(side->SurfaceOf()->IsClosed(1) ? "v" : "") +
		", seam=" + (cdt_face_has_seam(*side) ? "yes" : "no") :
		": no cylinder face") << std::endl;
	return false;
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(brep.get(),
	"full cylinder chart fixture");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    if (ON_Brep_CDT_Tessellate(state, 0, NULL) != 0) {
	struct brep_cdt_diagnostic diagnostic;
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	std::cerr << "full cylinder tessellation failed at stage "
	    << diagnostic.stage << ": " << diagnostic.message << std::endl;
	ON_Brep_CDT_Destroy(state);
	return false;
    }
    int *mesh_faces = NULL;
    fastf_t *mesh_vertices = NULL;
    int mesh_face_count = 0;
    int mesh_vertex_count = 0;
    const int mesh_status = ON_Brep_CDT_Mesh(&mesh_faces,
	&mesh_face_count, &mesh_vertices, &mesh_vertex_count, NULL, NULL,
	NULL, NULL, state, 0, NULL);
    const bool solid = mesh_status >= 0 && mesh_face_count > 0 &&
	mesh_vertex_count > 0 && bg_trimesh_solid2(mesh_vertex_count,
	    mesh_face_count, mesh_vertices, mesh_faces, NULL) == 0;
    bu_free(mesh_faces, "full cylinder chart fixture faces");
    bu_free(mesh_vertices, "full cylinder chart fixture vertices");
    ON_Brep_CDT_Destroy(state);
    if (!solid) {
	std::cerr << "full cylinder chart output was not solid" << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_offset_full_cylinder_seam()
{
    const double radius = 2.0;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, radius), 5.0);
    std::unique_ptr<ON_Brep> brep(ON_BrepCylinder(cylinder, true, true));
    if (!brep || !brep->IsValid() || !brep->IsSolid())
	return false;
    const ON_BrepFace *side = NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep->m_F[face_index];
	if (face.SurfaceOf() && face.SurfaceOf()->IsCylinder(NULL, 0.05)) {
	    side = &face;
	    break;
	}
    }
    if (!side || !cdt_face_has_seam(*side))
	return false;
    const ON_Surface *surface = side->SurfaceOf();
    const int closed_direction = surface->IsClosed(0) ? 0 : 1;
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval open_domain = surface->Domain(open_direction);
    const double low = open_domain.ParameterAt(0.25);
    const double high = open_domain.ParameterAt(0.75);
    const double closed_parameters[10] = {
	closed_domain.Min(), closed_domain.ParameterAt(0.25),
	closed_domain.ParameterAt(0.5), closed_domain.ParameterAt(0.75),
	/* Model a STEP p-curve whose closed-edge endpoint and following seam
	 * trim both retain the same equivalent native parameter bound. */
	closed_domain.Min(), closed_domain.Min(),
	closed_domain.ParameterAt(0.75), closed_domain.ParameterAt(0.5),
	closed_domain.ParameterAt(0.25), closed_domain.Min()
    };
    const double open_parameters[10] = {
	low, low, low, low, low, high, high, high, high, high
    };
    std::vector<std::pair<double, double>> native_points;
    std::vector<ON_3dPoint> point_storage;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices(10,
	CDT_TOPOLOGY_ID_NONE);
    std::vector<int> outer;
    native_points.reserve(10);
    point_storage.reserve(10);
    points_3d.reserve(10);
    const double offset = closed_domain.ParameterAt(1.0e-5);
    for (int i = 0; i < 10; ++i) {
	ON_2dPoint native;
	native[closed_direction] = closed_parameters[i];
	native[open_direction] = open_parameters[i];
	native_points.push_back(std::make_pair(native.x, native.y));
	outer.push_back(i);
	ON_2dPoint sample = native;
	if (i == 0 || i == 4 || i == 5 || i == 9)
	    sample[closed_direction] = offset;
	point_storage.push_back(surface->PointAt(sample.x, sample.y));
    }
    for (const ON_3dPoint &point : point_storage)
	points_3d.push_back(&point);
    topology_vertices[0] = 10;
    topology_vertices[4] = 10;
    topology_vertices[5] = 11;
    topology_vertices[9] = 11;
    outer.push_back(0);

    cdt_face_chart chart;
    if (!chart.build(*side, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CYLINDER ||
	    chart.outer.size() != 10) {
	std::cerr << "offset cylinder seam chart failed: "
	    << chart.failure() << std::endl;
	return false;
    }
    std::set<std::pair<double, double>> boundary_coordinates;
    for (int point : chart.outer)
	boundary_coordinates.insert(chart.points[(size_t)point]);
    if (boundary_coordinates.size() != chart.outer.size()) {
	std::cerr << "offset cylinder seam copies collapsed" << std::endl;
	return false;
    }

    /* Sampling and normal checks receive native point identities after CDT.
     * Ensure they select the coherent local image when a periodic repair has
     * cloned one of those identities at a distant chart coordinate. */
    const long image_triangle[3] = {0, 1, 5};
    const long image_edge[2] = {0, 1};
    ON_2dPoint expected_sample;
    ON_2dPoint expected_edge_sample;
    ON_2dPoint expected_edge_chart;
    if (!chart.triangle_interior_sample(image_triangle, expected_sample) ||
	    !chart.edge_midpoint_sample(image_edge, expected_edge_sample,
		expected_edge_chart)) {
	std::cerr << "cylinder chart could not sample a local cell"
	    << std::endl;
	return false;
    }
    cdt_face_chart multiple_image_chart = chart;
    cdt_chart_vertex distant = multiple_image_chart.vertices[0];
    distant.id = (cdt_chart_vertex_id)multiple_image_chart.points.size();
    std::pair<double, double> distant_point =
	multiple_image_chart.points[(size_t)distant.native_point];
    distant_point.first += 10.0 * 2.0 * ON_PI * radius;
    multiple_image_chart.points.push_back(distant_point);
    multiple_image_chart.vertices.insert(
	multiple_image_chart.vertices.begin(), distant);
    ON_2dPoint selected_sample;
    ON_2dPoint selected_edge_sample;
    ON_2dPoint selected_edge_chart;
    if (!multiple_image_chart.triangle_interior_sample(image_triangle,
	    selected_sample) ||
	    !multiple_image_chart.edge_midpoint_sample(image_edge,
		selected_edge_sample, selected_edge_chart) ||
	    surface->PointAt(expected_sample.x,
		expected_sample.y).DistanceTo(surface->PointAt(
		selected_sample.x, selected_sample.y)) > 1.0e-8 ||
	    expected_edge_chart.DistanceTo(selected_edge_chart) > 1.0e-8) {
	std::cerr << "cylinder chart selected a distant periodic image"
	    << std::endl;
	return false;
    }

    cdt_face_chart loose_sample_chart = chart;
    const std::vector<int> loose_edge_path = {0, 1, 2};
    loose_sample_chart.points[1].first =
	loose_sample_chart.points[0].first - 0.1;
    if (loose_sample_chart.repair_toleranced_edge_endpoint_samples(
	    loose_edge_path, points_3d, 20.0) != 1 ||
	    !(loose_sample_chart.points[1].first >
	    loose_sample_chart.points[0].first) ||
	    !(loose_sample_chart.points[1].first <
	    loose_sample_chart.points[2].first)) {
	std::cerr << "loose cylinder seam sample was not separated"
	    << std::endl;
	return false;
    }
    cdt_face_chart protected_sample_chart = chart;
    protected_sample_chart.points[1].first =
	protected_sample_chart.points[0].first - 0.1;
    protected_sample_chart.vertices[1].topo_vertex = 12;
    if (protected_sample_chart.repair_toleranced_edge_endpoint_samples(
	    loose_edge_path, points_3d, 20.0) != 0) {
	std::cerr << "cylinder seam repair moved a topology vertex"
	    << std::endl;
	return false;
    }

    std::vector<std::pair<double, double>> seam_hole_points = native_points;
    std::vector<ON_3dPoint> seam_hole_storage = point_storage;
    std::vector<cdt_topo_vertex_id> seam_hole_topology = topology_vertices;
    std::vector<int> seam_hole;
    const double hole_closed_radius = 0.03 * closed_domain.Length();
    const double hole_open_center = open_domain.Mid();
    const double hole_open_radius = 0.1 * open_domain.Length();
    for (int i = 0; i < 16; ++i) {
	const double angle = 2.0 * ON_PI * i / 16.0;
	ON_2dPoint native;
	native[closed_direction] = closed_domain.Min() +
	    hole_closed_radius * std::sin(angle);
	native[open_direction] = hole_open_center +
	    hole_open_radius * std::cos(angle);
	seam_hole.push_back((int)seam_hole_points.size());
	seam_hole_points.push_back(std::make_pair(native.x, native.y));
	double wrapped = native[closed_direction];
	while (wrapped < closed_domain.Min())
	    wrapped += closed_domain.Length();
	while (wrapped > closed_domain.Max())
	    wrapped -= closed_domain.Length();
	native[closed_direction] = wrapped;
	seam_hole_storage.push_back(surface->PointAt(native.x, native.y));
	seam_hole_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    seam_hole.push_back(seam_hole.front());
    std::vector<const ON_3dPoint *> seam_hole_3d;
    seam_hole_3d.reserve(seam_hole_storage.size());
    for (const ON_3dPoint &point : seam_hole_storage)
	seam_hole_3d.push_back(&point);
    cdt_face_chart seam_hole_chart;
    if (!seam_hole_chart.build(*side, seam_hole_points, outer,
	    std::vector<std::vector<int>>(1, seam_hole),
	    std::vector<int>(), std::vector<int>(), seam_hole_3d,
	    seam_hole_topology) || !seam_hole_chart.holes.empty() ||
	    seam_hole_chart.outer.size() <= chart.outer.size()) {
	std::cerr << "cylinder seam hole was not opened into the outline"
	    << std::endl;
	return false;
    }
    const auto opened_hole_triangulates = [](const cdt_face_chart &candidate,
	    std::string &failure) {
	std::vector<point2d_t> chart_points(candidate.points.size());
	for (size_t i = 0; i < candidate.points.size(); ++i)
	    V2SET(chart_points[i], candidate.points[i].first,
		candidate.points[i].second);
	std::vector<int> outline(candidate.outer);
	outline.push_back(outline.front());
	int *faces = NULL;
	int face_count = 0;
	struct bg_triangulation_report report = {0, -1, {0}};
	const int status = bg_nested_poly_triangulate_strict(&faces,
	    &face_count, NULL, NULL, outline.data(), outline.size(), NULL,
	    NULL, 0, NULL, 0, chart_points.data(), chart_points.size(),
	    &report);
	bu_free(faces, "opened cylinder seam hole triangles");
	failure = report.message + std::string(" (status ") +
	    std::to_string(status) + ", faces " +
	    std::to_string(face_count) + ")";
	return status == BRLCAD_OK && face_count > 0;
    };
    std::string seam_hole_failure;
    if (!opened_hole_triangulates(seam_hole_chart,
	    seam_hole_failure)) {
	std::cerr << "opened cylinder seam hole did not triangulate: "
	    << seam_hole_failure << std::endl;
	return false;
    }

    /* Sparse imported loops may straddle the artificial cut without a sample
     * inside the strict edge-miss snap distance.  The explicit repair chart
     * may relocate the cut to a nearby exact boundary sample, while ordinary
     * chart construction must retain the strict behavior. */
    std::vector<std::pair<double, double>> sparse_hole_points =
	native_points;
    std::vector<ON_3dPoint> sparse_hole_storage = point_storage;
    std::vector<cdt_topo_vertex_id> sparse_hole_topology =
	topology_vertices;
    std::vector<int> sparse_hole;
    const double sparse_closed[5] = {
	-0.015, 0.45, 0.45, -0.015, -0.015
    };
    const double sparse_open[5] = {
	0.6, 0.6, 0.4, 0.4, 0.5
    };
    for (int i = 0; i < 5; ++i) {
	ON_2dPoint native;
	native[closed_direction] = closed_domain.Min() +
	    sparse_closed[i] * closed_domain.Length();
	native[open_direction] = open_domain.ParameterAt(sparse_open[i]);
	sparse_hole.push_back((int)sparse_hole_points.size());
	sparse_hole_points.push_back(std::make_pair(native.x, native.y));
	while (native[closed_direction] < closed_domain.Min())
	    native[closed_direction] += closed_domain.Length();
	while (native[closed_direction] > closed_domain.Max())
	    native[closed_direction] -= closed_domain.Length();
	sparse_hole_storage.push_back(surface->PointAt(native.x, native.y));
	sparse_hole_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    sparse_hole.push_back(sparse_hole.front());
    std::vector<const ON_3dPoint *> sparse_hole_3d;
    sparse_hole_3d.reserve(sparse_hole_storage.size());
    for (const ON_3dPoint &point : sparse_hole_storage)
	sparse_hole_3d.push_back(&point);
    const std::vector<std::vector<int>> sparse_holes(1, sparse_hole);
    cdt_face_chart strict_sparse_chart;
    if (!strict_sparse_chart.build(*side, sparse_hole_points, outer,
	    sparse_holes, std::vector<int>(), std::vector<int>(),
	    sparse_hole_3d, sparse_hole_topology) ||
	    strict_sparse_chart.holes.size() != 1) {
	std::cerr << "strict cylinder chart unexpectedly opened a sparse hole"
	    << std::endl;
	return false;
    }
    cdt_face_chart relaxed_sparse_chart;
    if (!relaxed_sparse_chart.build(*side, sparse_hole_points, outer,
	    sparse_holes, std::vector<int>(), std::vector<int>(),
	    sparse_hole_3d, sparse_hole_topology, CDT_TOPOLOGY_ID_NONE,
	    true) || !relaxed_sparse_chart.holes.empty() ||
	    !opened_hole_triangulates(relaxed_sparse_chart,
		seam_hole_failure)) {
	std::cerr << "repair chart did not open a sparse cylinder seam hole: "
	    << relaxed_sparse_chart.failure() << " " << seam_hole_failure
	    << std::endl;
	return false;
    }

    std::vector<std::pair<double, double>> multi_hole_points =
	seam_hole_points;
    std::vector<ON_3dPoint> multi_hole_storage = seam_hole_storage;
    std::vector<cdt_topo_vertex_id> multi_hole_topology =
	seam_hole_topology;
    std::vector<int> second_seam_hole;
    const double second_open_center = open_domain.ParameterAt(0.32);
    const double second_open_radius = 0.04 * open_domain.Length();
    for (int i = 0; i < 16; ++i) {
	const double angle = 2.0 * ON_PI * i / 16.0;
	ON_2dPoint native;
	native[closed_direction] = closed_domain.Min() +
	    hole_closed_radius * std::sin(angle);
	native[open_direction] = second_open_center +
	    second_open_radius * std::cos(angle);
	second_seam_hole.push_back((int)multi_hole_points.size());
	multi_hole_points.push_back(std::make_pair(native.x, native.y));
	while (native[closed_direction] < closed_domain.Min())
	    native[closed_direction] += closed_domain.Length();
	while (native[closed_direction] > closed_domain.Max())
	    native[closed_direction] -= closed_domain.Length();
	multi_hole_storage.push_back(surface->PointAt(native.x, native.y));
	multi_hole_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    second_seam_hole.push_back(second_seam_hole.front());
    std::vector<const ON_3dPoint *> multi_hole_3d;
    multi_hole_3d.reserve(multi_hole_storage.size());
    for (const ON_3dPoint &point : multi_hole_storage)
	multi_hole_3d.push_back(&point);
    cdt_face_chart multi_hole_chart;
    const std::vector<std::vector<int>> two_seam_holes = {
	seam_hole, second_seam_hole
    };
    if (!multi_hole_chart.build(*side, multi_hole_points, outer,
	    two_seam_holes, std::vector<int>(), std::vector<int>(),
	    multi_hole_3d, multi_hole_topology) ||
	    !multi_hole_chart.holes.empty() ||
	    multi_hole_chart.outer.size() <= seam_hole_chart.outer.size() ||
	    !opened_hole_triangulates(multi_hole_chart,
		seam_hole_failure)) {
	std::cerr << "multiple cylinder seam holes did not open and "
	    "triangulate: " << multi_hole_chart.failure() << " "
	    << seam_hole_failure << std::endl;
	return false;
    }

    /* A physical hole edge may coincide with the artificial cut for an
     * entire sampled run.  The complementary arc still opens on the other
     * periodic side, and every point in the coincident run must survive. */
    std::vector<std::pair<double, double>> tangent_hole_points =
	native_points;
    std::vector<ON_3dPoint> tangent_hole_storage = point_storage;
    std::vector<cdt_topo_vertex_id> tangent_hole_topology =
	topology_vertices;
    std::vector<int> tangent_hole;
    const double tangent_closed[6] = {0.0, 0.0, 0.0,
	-hole_closed_radius, -1.5 * hole_closed_radius,
	-hole_closed_radius};
    const double tangent_open[6] = {hole_open_center + hole_open_radius,
	hole_open_center, hole_open_center - hole_open_radius,
	hole_open_center - hole_open_radius, hole_open_center,
	hole_open_center + hole_open_radius};
    for (int i = 0; i < 6; ++i) {
	ON_2dPoint native;
	native[closed_direction] = closed_domain.Min() + tangent_closed[i];
	native[open_direction] = tangent_open[i];
	tangent_hole.push_back((int)tangent_hole_points.size());
	tangent_hole_points.push_back(std::make_pair(native.x, native.y));
	while (native[closed_direction] < closed_domain.Min())
	    native[closed_direction] += closed_domain.Length();
	tangent_hole_storage.push_back(surface->PointAt(native.x, native.y));
	tangent_hole_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    tangent_hole.push_back(tangent_hole.front());
    std::vector<const ON_3dPoint *> tangent_hole_3d;
    tangent_hole_3d.reserve(tangent_hole_storage.size());
    for (const ON_3dPoint &point : tangent_hole_storage)
	tangent_hole_3d.push_back(&point);
    cdt_face_chart tangent_hole_chart;
    if (!tangent_hole_chart.build(*side, tangent_hole_points, outer,
	    std::vector<std::vector<int>>(1, tangent_hole),
	    std::vector<int>(), std::vector<int>(), tangent_hole_3d,
	    tangent_hole_topology) || !tangent_hole_chart.holes.empty() ||
	    tangent_hole_chart.outer.size() <= chart.outer.size() ||
	    !opened_hole_triangulates(tangent_hole_chart,
		seam_hole_failure)) {
	std::cerr << "cylinder seam-run hole did not open and triangulate: "
	    << tangent_hole_chart.failure() << " " << seam_hole_failure
	    << std::endl;
	return false;
    }
    std::set<long> tangent_boundary_sources;
    for (int point : tangent_hole_chart.outer) {
	if (point >= 0 && (size_t)point < tangent_hole_chart.vertices.size())
	    tangent_boundary_sources.insert(
		tangent_hole_chart.vertices[(size_t)point].native_point);
    }
    for (size_t source_index = tangent_hole_points.size() - 6;
	    source_index < tangent_hole_points.size(); ++source_index) {
	if (tangent_boundary_sources.find((long)source_index) ==
		tangent_boundary_sources.end()) {
	    std::cerr << "cylinder seam-run hole lost a boundary sample"
		<< std::endl;
	    return false;
	}
    }

    std::vector<std::pair<double, double>> high_tangent_points =
	native_points;
    std::vector<ON_3dPoint> high_tangent_storage = point_storage;
    std::vector<cdt_topo_vertex_id> high_tangent_topology =
	topology_vertices;
    std::vector<int> high_tangent_hole;
    for (int i = 0; i < 6; ++i) {
	ON_2dPoint native;
	native[closed_direction] = closed_domain.Max() + tangent_closed[i];
	native[open_direction] = tangent_open[i];
	high_tangent_hole.push_back((int)high_tangent_points.size());
	high_tangent_points.push_back(std::make_pair(native.x, native.y));
	high_tangent_storage.push_back(surface->PointAt(native.x, native.y));
	high_tangent_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    high_tangent_hole.push_back(high_tangent_hole.front());
    std::vector<const ON_3dPoint *> high_tangent_3d;
    high_tangent_3d.reserve(high_tangent_storage.size());
    for (const ON_3dPoint &point : high_tangent_storage)
	high_tangent_3d.push_back(&point);
    cdt_face_chart high_tangent_chart;
    if (!high_tangent_chart.build(*side, high_tangent_points, outer,
	    std::vector<std::vector<int>>(1, high_tangent_hole),
	    std::vector<int>(), std::vector<int>(), high_tangent_3d,
	    high_tangent_topology) || !high_tangent_chart.holes.empty() ||
	    high_tangent_chart.outer.size() <= chart.outer.size() ||
	    !opened_hole_triangulates(high_tangent_chart,
		seam_hole_failure)) {
	std::cerr << "high cylinder seam-run hole did not open and "
	    "triangulate: " << high_tangent_chart.failure() << " "
	    << seam_hole_failure << std::endl;
	return false;
    }

    std::vector<point2d_t> chart_points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(chart_points[i], chart.points[i].first, chart.points[i].second);
    std::vector<int> outline(chart.outer);
    outline.push_back(chart.outer.front());
    int *faces = NULL;
    int face_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    const int status = bg_nested_poly_triangulate_strict(&faces,
	&face_count, NULL, NULL, outline.data(), outline.size(), NULL, NULL,
	0, NULL, 0, chart_points.data(), chart_points.size(), &report);
    bu_free(faces, "offset cylinder seam triangles");
    if (status != BRLCAD_OK || face_count <= 0) {
	std::cerr << "offset cylinder seam triangulation failed: "
	    << report.message << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_periodic_metric_chart()
{
    ON_Circle major(ON_xy_plane, 9.0);
    ON_Torus torus(major, 2.5);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (!torus.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }

    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    ON_BrepLoop &loop = brep->NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double low = vdom.ParameterAt(0.2);
    const double high = vdom.ParameterAt(0.4);
    const int low_vertex = brep->NewVertex(surface->PointAt(
	udom.Min(), low)).m_vertex_index;
    const int high_vertex = brep->NewVertex(surface->PointAt(
	udom.Min(), high)).m_vertex_index;
    if (!add_iso_trim(*brep, loop, *surface, low_vertex, low_vertex,
	    ON_2dPoint(udom.Min(), low), ON_2dPoint(udom.Max(), low)) ||
	    !add_iso_trim(*brep, loop, *surface, low_vertex, high_vertex,
	    ON_2dPoint(udom.Max(), low), ON_2dPoint(udom.Max(), high)) ||
	    !add_iso_trim(*brep, loop, *surface, high_vertex, high_vertex,
	    ON_2dPoint(udom.Max(), high), ON_2dPoint(udom.Min(), high)) ||
	    !add_iso_trim(*brep, loop, *surface, high_vertex, low_vertex,
	    ON_2dPoint(udom.Min(), high), ON_2dPoint(udom.Min(), low)))
	return false;
    loop.Trim(1)->m_type = ON_BrepTrim::seam;
    loop.Trim(3)->m_type = ON_BrepTrim::seam;
    if (cdt_face_uses_cylinder_chart(face) ||
	    !cdt_face_uses_topology_chart(face)) {
	std::cerr << "generic periodic surface was not classified"
	    << std::endl;
	return false;
    }

    const double period = udom.Length();
    const double parameters[10] = {
	udom.Min(), udom.Min() - 0.25 * period,
	udom.Min() - 0.5 * period, udom.Min() - 0.75 * period,
	udom.Max(), udom.Max(), udom.ParameterAt(0.75),
	udom.ParameterAt(0.5), udom.ParameterAt(0.25), udom.Min()
    };
    const double heights[10] = {
	low, low, low, low, low, high, high, high, high, high
    };
    std::vector<std::pair<double, double>> native_points;
    std::vector<ON_3dPoint> point_storage;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices(10,
	CDT_TOPOLOGY_ID_NONE);
    std::vector<int> outer;
    native_points.reserve(10);
    point_storage.reserve(10);
    points_3d.reserve(10);
    for (int i = 0; i < 10; ++i) {
	outer.push_back(i);
	native_points.push_back(std::make_pair(parameters[i], heights[i]));
	double wrapped = std::fmod(parameters[i] - udom.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	point_storage.push_back(surface->PointAt(udom.Min() + wrapped,
	    heights[i]));
    }
    for (const ON_3dPoint &point : point_storage)
	points_3d.push_back(&point);
    topology_vertices[0] = low_vertex;
    topology_vertices[4] = low_vertex;
    topology_vertices[5] = high_vertex;
    topology_vertices[9] = high_vertex;
    outer.push_back(0);

    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_SURFACE_METRIC ||
	    chart.outer.size() != 10) {
	std::cerr << "periodic metric chart build failed: "
	    << chart.failure() << std::endl;
	return false;
    }
    const int low_path[5] = {0, 3, 2, 1, 4};
    for (int i = 1; i < 5; ++i) {
	if (!(chart.points[(size_t)low_path[i - 1]].first <
		chart.points[(size_t)low_path[i]].first)) {
	    std::cerr << "periodic metric winding was not repaired"
		<< std::endl;
	    return false;
	}
    }

    std::vector<std::pair<double, double>> seam_hole_points =
	native_points;
    std::vector<ON_3dPoint> seam_hole_storage = point_storage;
    std::vector<cdt_topo_vertex_id> seam_hole_topology =
	topology_vertices;
    std::vector<int> seam_hole;
    const double hole_u_radius = 0.04 * period;
    const double hole_v_center = 0.5 * (low + high);
    const double hole_v_radius = 0.2 * (high - low);
    for (int i = 0; i < 16; ++i) {
	const double angle = 2.0 * ON_PI * i / 16.0;
	double u = udom.Min() + hole_u_radius * std::sin(angle);
	const double v = hole_v_center + hole_v_radius * std::cos(angle);
	seam_hole.push_back((int)seam_hole_points.size());
	seam_hole_points.push_back(std::make_pair(u, v));
	while (u < udom.Min())
	    u += period;
	while (u > udom.Max())
	    u -= period;
	seam_hole_storage.push_back(surface->PointAt(u, v));
	seam_hole_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    }
    seam_hole.push_back(seam_hole.front());
    std::vector<const ON_3dPoint *> seam_hole_3d;
    seam_hole_3d.reserve(seam_hole_storage.size());
    for (const ON_3dPoint &point : seam_hole_storage)
	seam_hole_3d.push_back(&point);
    cdt_face_chart seam_hole_chart;
    if (!seam_hole_chart.build(face, seam_hole_points, outer,
	    std::vector<std::vector<int>>(1, seam_hole),
	    std::vector<int>(), std::vector<int>(), seam_hole_3d,
	    seam_hole_topology) ||
	    seam_hole_chart.type() != CDT_FACE_CHART_SURFACE_METRIC ||
	    !seam_hole_chart.holes.empty() ||
	    seam_hole_chart.outer.size() <= chart.outer.size()) {
	std::cerr << "periodic metric seam hole was not opened into the "
	    "outline: " << seam_hole_chart.failure() << std::endl;
	return false;
    }
    std::vector<point2d_t> seam_hole_chart_points(
	seam_hole_chart.points.size());
    for (size_t i = 0; i < seam_hole_chart.points.size(); ++i)
	V2SET(seam_hole_chart_points[i],
	    seam_hole_chart.points[i].first,
	    seam_hole_chart.points[i].second);
    std::vector<int> seam_hole_outline(seam_hole_chart.outer);
    seam_hole_outline.push_back(seam_hole_outline.front());
    int *seam_hole_faces = NULL;
    int seam_hole_face_count = 0;
    struct bg_triangulation_report seam_hole_report = {0, -1, {0}};
    const int seam_hole_status = bg_nested_poly_triangulate_strict(
	&seam_hole_faces, &seam_hole_face_count, NULL, NULL,
	seam_hole_outline.data(), seam_hole_outline.size(), NULL, NULL, 0,
	NULL, 0, seam_hole_chart_points.data(),
	seam_hole_chart_points.size(), &seam_hole_report);
    bu_free(seam_hole_faces, "periodic metric seam hole triangles");
    if (seam_hole_status != BRLCAD_OK || seam_hole_face_count <= 0) {
	std::cerr << "opened periodic metric seam hole did not "
	    "triangulate: " << seam_hole_report.message << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_seamless_cylinder_lift()
{
    const double radius = 2.0;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, radius), 5.0);
    std::unique_ptr<ON_Brep> brep(ON_BrepCylinder(cylinder, true, true));
    if (!brep || !brep->IsValid() || !brep->IsSolid())
	return false;
    ON_BrepFace *side = NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	ON_BrepFace &face = brep->m_F[face_index];
	if (face.SurfaceOf() && face.SurfaceOf()->IsCylinder(NULL, 0.05)) {
	    side = &face;
	    break;
	}
    }
    if (!side)
	return false;
    for (int loop_index = 0; loop_index < side->LoopCount(); ++loop_index) {
	ON_BrepLoop *loop = side->Loop(loop_index);
	for (int trim_index = 0; loop && trim_index < loop->TrimCount();
		++trim_index) {
	    ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (trim && trim->m_type == ON_BrepTrim::seam)
		trim->m_type = ON_BrepTrim::boundary;
	}
    }
    if (cdt_face_has_seam(*side) || !cdt_face_uses_cylinder_chart(*side))
	return false;

    const ON_Surface *surface = side->SurfaceOf();
    const int closed_direction = surface->IsClosed(0) ? 0 : 1;
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval open_domain = surface->Domain(open_direction);
    const int path_points = 10;
    std::vector<std::pair<double, double>> native_points;
    std::vector<ON_3dPoint> point_storage;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<int> outer;
    native_points.reserve((size_t)path_points * 2);
    point_storage.reserve((size_t)path_points * 2);
    points_3d.reserve((size_t)path_points * 2);
    const auto add_path_point = [&](double turn, double height) {
	double wrapped = std::fmod(0.07 + turn, 1.0);
	if (wrapped < 0.0)
	    wrapped += 1.0;
	ON_2dPoint native;
	native[closed_direction] = closed_domain.ParameterAt(wrapped);
	native[open_direction] = open_domain.ParameterAt(height);
	outer.push_back((int)native_points.size());
	native_points.push_back(std::make_pair(native.x, native.y));
	point_storage.push_back(surface->PointAt(native.x, native.y));
    };
    for (int i = 0; i < path_points; ++i) {
	const double turn = (double)i / (path_points - 1);
	add_path_point(turn, 0.2 + 0.2 * turn);
    }
    for (int i = path_points - 1; i >= 0; --i) {
	const double turn = (double)i / (path_points - 1);
	add_path_point(turn, 0.3 + 0.2 * turn);
    }
    for (const ON_3dPoint &point : point_storage)
	points_3d.push_back(&point);
    outer.push_back(outer.front());
    std::vector<cdt_topo_vertex_id> topology_vertices(
	native_points.size(), CDT_TOPOLOGY_ID_NONE);

    cdt_face_chart chart;
    if (!chart.build(*side, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CYLINDER ||
	    chart.outer.size() != native_points.size()) {
	std::cerr << "seamless cylinder lift failed: " << chart.failure()
	    << std::endl;
	return false;
    }
    const double half_circumference = ON_PI * radius;
    for (size_t i = 0; i < chart.outer.size(); ++i) {
	const int first = chart.outer[i];
	const int second = chart.outer[(i + 1) % chart.outer.size()];
	if (std::fabs(chart.points[(size_t)second].first -
		chart.points[(size_t)first].first) >= half_circumference) {
	    std::cerr << "seamless cylinder chart retained a cut chord"
		<< std::endl;
	    return false;
	}
    }

    std::vector<point2d_t> chart_points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(chart_points[i], chart.points[i].first, chart.points[i].second);
    std::vector<int> outline(chart.outer);
    outline.push_back(chart.outer.front());
    int *faces = NULL;
    int face_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    const int status = bg_nested_poly_triangulate_strict(&faces,
	&face_count, NULL, NULL, outline.data(), outline.size(), NULL, NULL,
	0, NULL, 0, chart_points.data(), chart_points.size(), &report);
    bu_free(faces, "seamless cylinder lift triangles");
    if (status != BRLCAD_OK || face_count <= 0) {
	std::cerr << "seamless cylinder lift triangulation failed: "
	    << report.message << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_planar_nesting_repair()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetExtents(0, ON_Interval(-10.0, 10.0), true);
    surface->SetExtents(1, ON_Interval(-10.0, 10.0), true);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    const std::pair<double, double> coordinates[8] = {
	std::make_pair(-1.0, -1.0), std::make_pair(1.0, -1.0),
	std::make_pair(1.0, 1.0), std::make_pair(-1.0, 1.0),
	std::make_pair(-5.0, -5.0), std::make_pair(5.0, -5.0),
	std::make_pair(5.0, 5.0), std::make_pair(-5.0, 5.0)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 8);
    const int outer_indices[5] = {0, 1, 2, 3, 0};
    const int hole_indices[5] = {4, 5, 6, 7, 4};
    std::vector<int> outer(outer_indices, outer_indices + 5);
    std::vector<std::vector<int>> holes(1,
	std::vector<int>(hole_indices, hole_indices + 5));
    std::vector<const ON_3dPoint *> points_3d(8, NULL);
    std::vector<cdt_topo_vertex_id> topology_vertices(8,
	CDT_TOPOLOGY_ID_NONE);
    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer, holes,
	    std::vector<int>(), std::vector<int>(), points_3d,
	    topology_vertices) || chart.outer.size() != 4 ||
	    chart.holes.size() != 1 || chart.holes[0].size() != 4 ||
	    chart.outer[0] < 4 || chart.holes[0][0] >= 4) {
	std::cerr << "planar outer/hole nesting was not repaired"
	    << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_cylinder_nesting_repair()
{
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 5.0), 10.0);
    std::unique_ptr<ON_Brep> brep(ON_BrepCylinder(cylinder, true, true));
    if (!brep || !brep->IsValid() || !brep->IsSolid())
	return false;
    const ON_BrepFace *side = NULL;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_BrepFace &face = brep->m_F[face_index];
	if (face.SurfaceOf() && face.SurfaceOf()->IsCylinder(NULL, 0.05)) {
	    side = &face;
	    break;
	}
    }
    if (!side || !side->SurfaceOf())
	return false;

    const ON_Surface *surface = side->SurfaceOf();
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const auto uv = [&](double u, double v) {
	return std::make_pair(udom.ParameterAt(u), vdom.ParameterAt(v));
    };
    const std::pair<double, double> coordinates[8] = {
	uv(0.22, 0.40), uv(0.28, 0.40), uv(0.28, 0.60),
	uv(0.22, 0.60), uv(0.15, 0.20), uv(0.35, 0.20),
	uv(0.35, 0.80), uv(0.15, 0.80)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 8);
    const int outer_indices[5] = {0, 1, 2, 3, 0};
    const int hole_indices[5] = {4, 5, 6, 7, 4};
    std::vector<int> outer(outer_indices, outer_indices + 5);
    std::vector<std::vector<int>> holes(1,
	std::vector<int>(hole_indices, hole_indices + 5));
    std::vector<const ON_3dPoint *> points_3d(8, NULL);
    std::vector<cdt_topo_vertex_id> topology_vertices(8,
	CDT_TOPOLOGY_ID_NONE);
    cdt_face_chart chart;
    if (!chart.build(*side, native_points, outer, holes,
	    std::vector<int>(), std::vector<int>(), points_3d,
	    topology_vertices) || chart.type() != CDT_FACE_CHART_CYLINDER ||
	    chart.outer.size() != 4 || chart.holes.size() != 1 ||
	    chart.holes[0].size() != 4 || chart.outer[0] < 4 ||
	    chart.holes[0][0] >= 4) {
	std::cerr << "cylinder outer/hole nesting was not repaired"
	    << std::endl;
	return false;
    }

    std::vector<point2d_t> chart_points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(chart_points[i], chart.points[i].first, chart.points[i].second);
    const int *hole_data[1] = {chart.holes[0].data()};
    const size_t hole_counts[1] = {chart.holes[0].size()};
    int *faces = NULL;
    int face_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    const int status = bg_nested_poly_triangulate_strict(&faces,
	&face_count, NULL, NULL, chart.outer.data(), chart.outer.size(),
	hole_data, hole_counts, 1, NULL, 0, chart_points.data(),
	chart_points.size(), &report);
    bu_free(faces, "cylinder nesting repair triangles");
    if (status != BRLCAD_OK || face_count <= 0) {
	std::cerr << "repaired cylinder nesting did not triangulate: "
	    << report.message << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_curved_metric_nesting_repair()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_NurbsSurface *surface = new ON_NurbsSurface(
	3, false, 3, 3, 3, 3);
    if (!surface->MakeClampedUniformKnotVector(0) ||
	    !surface->MakeClampedUniformKnotVector(1)) {
	delete surface;
	return false;
    }
    for (int u = 0; u < 3; ++u) {
	for (int v = 0; v < 3; ++v) {
	    const double height = u == 1 && v == 1 ? 1.0 : 0.0;
	    surface->SetCV(u, v, ON_3dPoint(u, v, height));
	}
    }
    if (!surface->IsValid() || surface->IsPlanar(NULL, BN_TOL_DIST)) {
	delete surface;
	return false;
    }
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const auto uv = [&](double u, double v) {
	return std::make_pair(udom.ParameterAt(u), vdom.ParameterAt(v));
    };
    const std::pair<double, double> coordinates[8] = {
	uv(0.40, 0.40), uv(0.60, 0.40), uv(0.60, 0.60),
	uv(0.40, 0.60), uv(0.10, 0.10), uv(0.90, 0.10),
	uv(0.90, 0.90), uv(0.10, 0.90)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 8);
    const int outer_indices[5] = {0, 1, 2, 3, 0};
    const int hole_indices[5] = {4, 5, 6, 7, 4};
    std::vector<int> outer(outer_indices, outer_indices + 5);
    std::vector<std::vector<int>> holes(1,
	std::vector<int>(hole_indices, hole_indices + 5));
    std::vector<const ON_3dPoint *> points_3d(8, NULL);
    std::vector<cdt_topo_vertex_id> topology_vertices(8,
	CDT_TOPOLOGY_ID_NONE);
    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer, holes,
	    std::vector<int>(), std::vector<int>(), points_3d,
	    topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_SURFACE_METRIC ||
	    chart.outer.size() != 4 || chart.holes.size() != 1 ||
	    chart.holes[0].size() != 4 || chart.outer[0] < 4 ||
	    chart.holes[0][0] >= 4) {
	std::cerr << "curved metric outer/hole nesting was not repaired"
	    << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_metric_component_partition()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetExtents(0, ON_Interval(0.0, 10.0), true);
    surface->SetExtents(1, ON_Interval(0.0, 10.0), true);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    const std::pair<double, double> coordinates[18] = {
	std::make_pair(0.0, 0.0), std::make_pair(4.0, 0.0),
	std::make_pair(4.0, 4.0), std::make_pair(0.0, 4.0),
	std::make_pair(1.0, 1.0), std::make_pair(1.0, 3.0),
	std::make_pair(3.0, 3.0), std::make_pair(3.0, 1.0),
	std::make_pair(1.5, 1.5), std::make_pair(2.5, 1.5),
	std::make_pair(2.5, 2.5), std::make_pair(1.5, 2.5),
	std::make_pair(6.0, 0.0), std::make_pair(8.0, 0.0),
	std::make_pair(8.0, 2.0), std::make_pair(6.0, 2.0),
	std::make_pair(0.5, 0.5), std::make_pair(7.0, 1.0)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 18);
    const int outer_indices[5] = {0, 1, 2, 3, 0};
    const int first_hole_indices[5] = {4, 5, 6, 7, 4};
    const int island_indices[5] = {8, 9, 10, 11, 8};
    const int disjoint_indices[5] = {12, 13, 14, 15, 12};
    std::vector<int> outer(outer_indices, outer_indices + 5);
    std::vector<std::vector<int>> holes;
    holes.push_back(std::vector<int>(first_hole_indices,
	first_hole_indices + 5));
    holes.push_back(std::vector<int>(island_indices,
	island_indices + 5));
    holes.push_back(std::vector<int>(disjoint_indices,
	disjoint_indices + 5));
    const std::vector<int> steiner = {16, 17};
    std::vector<const ON_3dPoint *> points_3d(18, NULL);
    std::vector<cdt_topo_vertex_id> topology_vertices(18,
	CDT_TOPOLOGY_ID_NONE);
    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer, holes, steiner,
	    std::vector<int>(), points_3d, topology_vertices)) {
	std::cerr << "component partition chart build failed: "
	    << chart.failure() << std::endl;
	return false;
    }
    std::vector<cdt_face_chart> components;
    std::string failure;
    if (!chart.partition_components(components, &failure) ||
	    components.size() != 3 || components[0].holes.size() != 1 ||
	    !components[1].holes.empty() || !components[2].holes.empty() ||
	    components[0].outer[0] != 0 ||
	    components[0].holes[0][0] != 4 ||
	    components[1].outer[0] != 8 || components[2].outer[0] != 12 ||
	    components[0].steiner != std::vector<int>(1, 16) ||
	    !components[1].steiner.empty() ||
	    components[2].steiner != std::vector<int>(1, 17)) {
	std::cerr << "metric chart components were not partitioned: "
	    << failure << std::endl;
	return false;
    }

    std::vector<point2d_t> chart_points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(chart_points[i], chart.points[i].first, chart.points[i].second);
    for (const cdt_face_chart &component : components) {
	std::vector<int> outline(component.outer);
	outline.push_back(outline.front());
	std::vector<std::vector<int>> component_holes(component.holes);
	std::vector<const int *> hole_data;
	std::vector<size_t> hole_counts;
	for (std::vector<int> &hole : component_holes) {
	    hole.push_back(hole.front());
	    hole_data.push_back(hole.data());
	    hole_counts.push_back(hole.size());
	}
	int *faces = NULL;
	int face_count = 0;
	struct bg_triangulation_report report = {0, -1, {0}};
	const int status = bg_nested_poly_triangulate_strict(&faces,
	    &face_count, NULL, NULL, outline.data(), outline.size(),
	    hole_data.empty() ? NULL : hole_data.data(),
	    hole_counts.empty() ? NULL : hole_counts.data(),
	    hole_data.size(), component.steiner.empty() ? NULL :
	    component.steiner.data(), component.steiner.size(),
	    chart_points.data(), chart_points.size(), &report);
	bu_free(faces, "metric component partition triangles");
	if (status != BRLCAD_OK || face_count <= 0) {
	    std::cerr << "metric chart component did not triangulate: "
		<< report.message << std::endl;
	    return false;
	}
    }
    return true;
}

static bool
exercise_toleranced_endpoint_sample_repair()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_NurbsSurface *surface = new ON_NurbsSurface(
	3, false, 2, 2, 2, 2);
    if (!surface->MakeClampedUniformKnotVector(0) ||
	    !surface->MakeClampedUniformKnotVector(1)) {
	delete surface;
	return false;
    }
    surface->SetCV(0, 0, ON_3dPoint(0.0, 0.0, 0.0));
    surface->SetCV(0, 1, ON_3dPoint(0.0, 2.0, 0.0));
    surface->SetCV(1, 0, ON_3dPoint(2.0, 0.0, 0.0));
    surface->SetCV(1, 1, ON_3dPoint(2.0, 2.0, 0.2));
    surface->SetDomain(0, 0.0, 2.0);
    surface->SetDomain(1, 0.0, 2.0);
    if (!surface->IsValid() || surface->IsPlanar(NULL, BN_TOL_DIST)) {
	delete surface;
	return false;
    }
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    const std::pair<double, double> coordinates[5] = {
	std::make_pair(0.0, 0.0), std::make_pair(2.0, 0.0),
	std::make_pair(2.0, 2.0), std::make_pair(2.0, 2.0),
	std::make_pair(0.0, 2.0)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 5);
    const int outer_indices[6] = {0, 1, 2, 3, 4, 0};
    std::vector<int> outer(outer_indices, outer_indices + 6);
    ON_3dPoint source_points[5];
    for (size_t i = 0; i < 5; ++i)
	source_points[i] = surface->PointAt(coordinates[i].first,
	    coordinates[i].second);
    source_points[2] = surface->PointAt(2.0, 1.0);
    std::vector<const ON_3dPoint *> points_3d;
    for (const ON_3dPoint &point : source_points)
	points_3d.push_back(&point);
    std::vector<cdt_topo_vertex_id> topology_vertices(5,
	CDT_TOPOLOGY_ID_NONE);
    topology_vertices[0] = 10;
    topology_vertices[1] = 11;
    topology_vertices[3] = 12;
    topology_vertices[4] = 13;
    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.points[2] != chart.points[3]) {
	std::cerr << "endpoint sample fixture did not retain its source "
	    "coincidence" << std::endl;
	return false;
    }
    const std::vector<int> edge_path = {1, 2, 3};
    if (chart.repair_toleranced_edge_endpoint_samples(edge_path,
	    points_3d, 0.5) != 0 || chart.points[2] != chart.points[3]) {
	std::cerr << "endpoint sample repair exceeded its tolerance"
	    << std::endl;
	return false;
    }
    cdt_face_chart protected_chart = chart;
    protected_chart.vertices[2].topo_vertex = 14;
    if (protected_chart.repair_toleranced_edge_endpoint_samples(
	    edge_path, points_3d, 1.1) != 0) {
	std::cerr << "endpoint sample repair moved a topology vertex"
	    << std::endl;
	return false;
    }
    if (chart.repair_toleranced_edge_endpoint_samples(edge_path,
	    points_3d, 1.1) != 1 ||
	    std::fabs(chart.points[2].first - 0.5 *
		(chart.points[1].first + chart.points[3].first)) > 1.0e-12 ||
	    std::fabs(chart.points[2].second - 0.5 *
		(chart.points[1].second + chart.points[3].second)) > 1.0e-12) {
	std::cerr << "endpoint sample was not redistributed by source length"
	    << std::endl;
	return false;
    }

    std::vector<point2d_t> chart_points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(chart_points[i], chart.points[i].first, chart.points[i].second);
    std::vector<int> outline(chart.outer);
    outline.push_back(chart.outer.front());
    int *faces = NULL;
    int face_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    const int status = bg_nested_poly_triangulate_strict(&faces,
	&face_count, NULL, NULL, outline.data(), outline.size(), NULL, NULL,
	0, NULL, 0, chart_points.data(), chart_points.size(), &report);
    bu_free(faces, "endpoint sample repair triangles");
    if (status != BRLCAD_OK || face_count != 3) {
	std::cerr << "repaired endpoint sample did not triangulate: "
	    << report.message << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_planar_master_curve_embedding()
{
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, 0.0, 2.0);
    surface->SetDomain(1, 0.0, 2.0);
    surface->SetExtents(0, surface->Domain(0), true);
    surface->SetExtents(1, surface->Domain(1), true);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    const std::pair<double, double> coordinates[5] = {
	std::make_pair(0.0, 0.0), std::make_pair(2.0, 0.0),
	std::make_pair(2.0, 2.0), std::make_pair(2.0, 2.0),
	std::make_pair(0.0, 2.0)
    };
    std::vector<std::pair<double, double>> native_points(
	coordinates, coordinates + 5);
    const int outer_indices[6] = {0, 1, 2, 3, 4, 0};
    std::vector<int> outer(outer_indices, outer_indices + 6);
    ON_3dPoint source_points[5] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 0.0, 0.0),
	ON_3dPoint(2.0, 1.0, 0.0), ON_3dPoint(2.0, 2.0, 0.0),
	ON_3dPoint(0.0, 2.0, 0.0)
    };
    std::vector<const ON_3dPoint *> points_3d;
    for (const ON_3dPoint &point : source_points)
	points_3d.push_back(&point);
    std::vector<cdt_topo_vertex_id> topology_vertices(5,
	CDT_TOPOLOGY_ID_NONE);
    topology_vertices[0] = 10;
    topology_vertices[1] = 11;
    topology_vertices[3] = 12;
    topology_vertices[4] = 13;
    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.points[2] == chart.points[3] ||
	    std::fabs(chart.points[2].first - 2.0) > 1.0e-12 ||
	    std::fabs(chart.points[2].second - 1.0) > 1.0e-12) {
	std::cerr << "planar chart did not preserve its master-curve samples"
	    << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_unbounded_cylinder_axis()
{
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 2.0), 5.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (!cylinder.IsValid() || 2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));

    /* Boundary samples may be collected before a face surface is shrunk.
     * The fitted finite cylinder then describes only the shrunken surface,
     * but its lateral chart remains valid for authoritative samples beyond
     * either cap. */
    const double angles[4] = {0.5, 1.5, 1.5, 0.5};
    const double heights[4] = {-2.0, -2.0, 7.0, 7.0};
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    std::vector<std::pair<double, double>> native_points;
    std::vector<ON_3dPoint> point_storage;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<int> outer;
    point_storage.reserve(4);
    for (int i = 0; i < 4; ++i) {
	native_points.push_back(std::make_pair(
	    udom.ParameterAt(i == 0 || i == 3 ? 0.2 : 0.4),
	    vdom.ParameterAt(i < 2 ? 0.0 : 1.0)));
	point_storage.push_back(cylinder.PointAt(angles[i], heights[i]));
	outer.push_back(i);
    }
    for (const ON_3dPoint &point : point_storage)
	points_3d.push_back(&point);
    outer.push_back(outer.front());
    std::vector<cdt_topo_vertex_id> topology_vertices(4,
	CDT_TOPOLOGY_ID_NONE);

    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CYLINDER) {
	std::cerr << "unbounded cylinder chart failed: " << chart.failure()
	    << std::endl;
	return false;
    }
    double minimum_height = DBL_MAX;
    double maximum_height = -DBL_MAX;
    for (const std::pair<double, double> &point : chart.points) {
	minimum_height = std::min(minimum_height, point.second);
	maximum_height = std::max(maximum_height, point.second);
    }
    if (std::fabs((maximum_height - minimum_height) - 9.0) > 1.0e-12) {
	std::cerr << "cylinder chart clamped samples to fitted caps"
	    << std::endl;
	return false;
    }
    return true;
}

static bool
exercise_weakly_simple_cylinder()
{
    return cdt_test_developable_clean() == 0;
}

int
main()
{
    if (!exercise_cylinder(ON_3dPoint::Origin,
	    ON_3dVector(0.0, 0.0, 1.0), 2.0, 5.0))
	return 1;
    if (!exercise_cylinder(ON_3dPoint(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(1.0, 2.0, 3.0), 1.0e3, 4.0e3))
	return 2;
    if (!exercise_cylinder(ON_3dPoint(1.0e-5, -2.0e-5, 3.0e-5),
	    ON_3dVector(-2.0, 1.0, 4.0), 2.0e-6, 7.0e-6))
	return 3;
    if (!exercise_full_cylinder(ON_3dPoint::Origin,
	    ON_3dVector(0.0, 0.0, 1.0), 2.0, 5.0))
	return 4;
    if (!exercise_full_cylinder(ON_3dPoint(1.0e6, -2.0e6, 3.0e6),
	    ON_3dVector(1.0, 2.0, 3.0), 1.0e3, 4.0e3))
	return 5;
    if (!exercise_full_cylinder(
	    ON_3dPoint(1.0e-5, -2.0e-5, 3.0e-5),
	    ON_3dVector(-2.0, 1.0, 4.0), 2.0e-6, 7.0e-6))
	return 6;
    if (!exercise_offset_full_cylinder_seam())
	return 7;
    if (!exercise_seamless_cylinder_lift())
	return 8;
    if (!exercise_periodic_metric_chart())
	return 9;
    if (!exercise_planar_nesting_repair())
	return 10;
    if (!exercise_cylinder_nesting_repair())
	return 11;
    if (!exercise_curved_metric_nesting_repair())
	return 12;
    if (!exercise_metric_component_partition())
	return 13;
    if (!exercise_toleranced_endpoint_sample_repair())
	return 14;
    if (!exercise_planar_master_curve_embedding())
	return 15;
    if (!exercise_unbounded_cylinder_axis())
	return 16;
    if (!exercise_weakly_simple_cylinder())
	return 17;
    return 0;
}
