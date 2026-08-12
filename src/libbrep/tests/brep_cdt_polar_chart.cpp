/*               B R E P _ C D T _ P O L A R _ C H A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

#include "bg/trimesh.h"
#include "brep/cdt.h"
#include "../cdt/chart.h"

static bool
exercise_coincident_seams()
{
    const ON_Sphere sphere(ON_3dPoint(4.0, -3.0, 2.0), 2.5);
    std::unique_ptr<ON_Brep> source(ON_BrepSphere(sphere));
    if (!source || !source->IsValid() || source->m_S.Count() != 1)
	return false;
    ON_Surface *surface = source->m_S[0]->DuplicateSurface();
    if (!surface)
	return false;

    std::unique_ptr<ON_Brep> brep(new ON_Brep);
    ON_BrepFace &face = brep->NewFace(brep->AddSurface(surface));
    face.m_bRev = source->m_F[0].m_bRev;
    ON_BrepLoop &loop = brep->NewLoop(ON_BrepLoop::outer, face);
    int open_dir = -1;
    for (int side = 0; side < 4; ++side) {
	if (surface->IsSingular(side)) {
	    open_dir = (side == 0 || side == 2) ? 1 : 0;
	    break;
	}
    }
    if (open_dir < 0)
	return false;
    const int angular_dir = 1 - open_dir;
    const ON_Interval open_domain = surface->Domain(open_dir);
    const ON_Interval angular_domain = surface->Domain(angular_dir);
    const double seam = angular_domain.Mid();
    ON_2dPoint low_uv;
    ON_2dPoint high_uv;
    low_uv[angular_dir] = high_uv[angular_dir] = seam;
    low_uv[open_dir] = open_domain.Min();
    high_uv[open_dir] = open_domain.Max();
    const int low_vertex = brep->NewVertex(surface->PointAt(low_uv.x,
	low_uv.y), 1.0e-8).m_vertex_index;
    const int high_vertex = brep->NewVertex(surface->PointAt(high_uv.x,
	high_uv.y), 1.0e-8).m_vertex_index;
    ON_Curve *edge_curve = surface->IsoCurve(open_dir, seam);
    if (!edge_curve)
	return false;
    if (edge_curve->PointAtStart().DistanceTo(
	    brep->m_V[low_vertex].Point()) >
		edge_curve->PointAtEnd().DistanceTo(
		brep->m_V[low_vertex].Point()))
	edge_curve->Reverse();
    ON_BrepEdge &edge = brep->NewEdge(brep->m_V[low_vertex],
	brep->m_V[high_vertex], brep->AddEdgeCurve(edge_curve));
    edge.m_tolerance = 1.0e-8;
    const auto add_seam = [&](bool reversed) {
	const ON_2dPoint start = reversed ? high_uv : low_uv;
	const ON_2dPoint end = reversed ? low_uv : high_uv;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep->NewTrim(edge, reversed, loop,
	    brep->AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::seam;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-8;
    };
    add_seam(false);
    add_seam(true);

    ON_TextLog validation_log(stderr);
    const bool valid_brep = brep->IsValid(&validation_log);
    const bool solid_brep = brep->IsSolid();
    const bool polar_face = cdt_face_uses_polar_chart(face);
    if (!valid_brep || !solid_brep || !polar_face) {
	std::cerr << "invalid coincident-seam sphere fixture: valid="
	    << valid_brep << " solid=" << solid_brep << " polar="
	    << polar_face << std::endl;
	return false;
    }

    /* Keep the paired-seam recovery independent of unrelated inner loops.
     * Imported spherical faces can put both seam p-curves on the same
     * periodic image while also carrying a hole elsewhere on the face. */
    std::vector<std::pair<double, double>> native_points;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices;
    std::vector<ON_3dPoint> stored_points;
    stored_points.reserve(9);
    const auto add_chart_point = [&](const ON_2dPoint &uv,
	    const ON_3dPoint *point, cdt_topo_vertex_id topology) {
	const int index = (int)native_points.size();
	native_points.push_back(std::make_pair(uv.x, uv.y));
	points_3d.push_back(point);
	topology_vertices.push_back(topology);
	return index;
    };
    std::vector<int> outer;
    stored_points.push_back(brep->m_V[low_vertex].Point());
    outer.push_back(add_chart_point(low_uv, &stored_points.back(),
	low_vertex));
    std::vector<int> first_seam;
    for (int sample = 1; sample < 4; ++sample) {
	ON_2dPoint uv;
	uv[angular_dir] = seam;
	uv[open_dir] = open_domain.ParameterAt(0.25 * sample);
	stored_points.push_back(surface->PointAt(uv.x, uv.y));
	first_seam.push_back(add_chart_point(uv, &stored_points.back(),
	    CDT_TOPOLOGY_ID_NONE));
	outer.push_back(first_seam.back());
    }
    stored_points.push_back(brep->m_V[high_vertex].Point());
    outer.push_back(add_chart_point(high_uv, &stored_points.back(),
	high_vertex));
    for (auto sample = first_seam.rbegin(); sample != first_seam.rend();
	    ++sample) {
	const int native_source = *sample;
	const ON_2dPoint uv(native_points[(size_t)native_source].first,
	    native_points[(size_t)native_source].second);
	outer.push_back(add_chart_point(uv, points_3d[(size_t)native_source],
	    CDT_TOPOLOGY_ID_NONE));
    }
    outer.push_back(outer.front());

    std::vector<int> hole;
    for (int corner = 0; corner < 4; ++corner) {
	ON_2dPoint uv;
	const double angular_fraction = corner == 0 || corner == 3 ?
	    0.20 : 0.30;
	const double open_fraction = corner < 2 ? 0.45 : 0.55;
	uv[angular_dir] = angular_domain.ParameterAt(angular_fraction);
	uv[open_dir] = open_domain.ParameterAt(open_fraction);
	stored_points.push_back(surface->PointAt(uv.x, uv.y));
	hole.push_back(add_chart_point(uv, &stored_points.back(),
	    CDT_TOPOLOGY_ID_NONE));
    }
    hole.push_back(hole.front());
    cdt_face_chart holed_chart;
    const std::vector<std::vector<int>> holes = {hole};
    if (!holed_chart.build(face, native_points, outer, holes,
	    std::vector<int>(), std::vector<int>(), points_3d,
	    topology_vertices) || holed_chart.type() != CDT_FACE_CHART_POLAR ||
	    holed_chart.holes.size() != 1) {
	std::cerr << "polar seam recovery with a hole failed: "
	    << holed_chart.failure() << std::endl;
	return false;
    }
    const std::set<std::pair<double, double>> unique_chart_points(
	holed_chart.points.begin(), holed_chart.points.end());
    if (unique_chart_points.size() != holed_chart.points.size()) {
	std::cerr << "polar seam recovery with a hole retained duplicate "
	    "chart coordinates" << std::endl;
	return false;
    }
    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(brep.get(),
	"coincident polar seam fixture");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.01;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    if (ON_Brep_CDT_Tessellate(state, 0, NULL) != 0) {
	struct brep_cdt_diagnostic diagnostic;
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	std::cerr << "coincident-seam sphere tessellation failed at stage "
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
    bu_free(mesh_faces, "coincident polar seam faces");
    bu_free(mesh_vertices, "coincident polar seam vertices");
    ON_Brep_CDT_Destroy(state);
    if (!solid)
	std::cerr << "coincident-seam sphere output was not solid"
	    << std::endl;
    return solid;
}

static bool
exercise_sphere(const ON_3dPoint &center, double radius)
{
    ON_Sphere sphere(center, radius);
    if (!sphere.IsValid())
	return false;
    std::unique_ptr<ON_Brep> brep(ON_BrepSphere(sphere));
    if (!brep || !brep->IsValid() || !brep->IsSolid() ||
	    brep->m_F.Count() != 1) {
	std::cerr << "invalid sphere B-Rep" << std::endl;
	return false;
    }
    const ON_BrepFace &face = brep->m_F[0];
    if (!cdt_face_uses_polar_chart(face)) {
	std::cerr << "sphere face was not classified as polar" << std::endl;
	return false;
    }

    const ON_Surface *surface = face.SurfaceOf();
    const ON_BrepLoop *loop = face.OuterLoop();
    if (!surface || !loop)
	return false;
    int open_dir = -1;
    for (int side = 0; side < 4; ++side) {
	if (surface->IsSingular(side)) {
	    open_dir = (side == 0 || side == 2) ? 1 : 0;
	    break;
	}
    }
    if (open_dir < 0)
	return false;
    const int angular_dir = 1 - open_dir;
    const ON_Interval open_domain = surface->Domain(open_dir);
    const ON_Interval angular_domain = surface->Domain(angular_dir);

    std::vector<std::pair<double, double>> native_points;
    std::vector<int> outer;
    std::vector<int> steiner;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices;
    std::vector<ON_3dPoint> topology_points((size_t)brep->m_V.Count());
    for (int vi = 0; vi < brep->m_V.Count(); ++vi)
	topology_points[(size_t)vi] = brep->m_V[vi].Point();
    const auto add_point = [&](const ON_2dPoint &uv,
	    cdt_topo_vertex_id topology, const ON_3dPoint *point_3d) {
	const int index = (int)native_points.size();
	native_points.push_back(std::make_pair(uv.x, uv.y));
	points_3d.push_back(point_3d);
	topology_vertices.push_back(topology);
	return index;
    };
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim || trim->m_vi[0] < 0)
	    return false;
	const ON_3dPoint uv3 = trim->PointAtStart();
	outer.push_back(add_point(ON_2dPoint(uv3.x, uv3.y),
	    trim->m_vi[0], &topology_points[(size_t)trim->m_vi[0]]));
	if (trim->m_type != ON_BrepTrim::singular) {
	    const ON_3dPoint midpoint = trim->PointAt(trim->Domain().Mid());
	    outer.push_back(add_point(ON_2dPoint(midpoint.x, midpoint.y),
		CDT_TOPOLOGY_ID_NONE, NULL));
	}
    }
    outer.push_back(outer[0]);

    for (int angular_sample = 1; angular_sample <= 3;
	    ++angular_sample) {
	for (int open_sample = 1; open_sample <= 3; ++open_sample) {
	    ON_2dPoint uv;
	    uv[angular_dir] = angular_domain.ParameterAt(
		0.2 * angular_sample);
	    uv[open_dir] = open_domain.ParameterAt(0.25 * open_sample);
	    steiner.push_back(add_point(uv, CDT_TOPOLOGY_ID_NONE, NULL));
	}
    }

    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), steiner, std::vector<int>(),
	    points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_POLAR ||
	    chart.pole_topology_vertex() < 0 ||
	    chart.second_pole_topology_vertex() < 0 ||
	    chart.constraints.empty()) {
	std::cerr << "polar chart build failed: " << chart.failure()
	    << std::endl;
	return false;
    }

    /* A physical spherical lune needs no interior meridian samples: its two
     * distinct sides already form a nonzero-area disk in the polar chart. */
    std::vector<std::pair<double, double>> lune_native;
    std::vector<int> lune_outer;
    std::vector<const ON_3dPoint *> lune_points_3d;
    std::vector<cdt_topo_vertex_id> lune_topology;
    const auto add_lune_point = [&](double angular_fraction,
	    double open_fraction, cdt_topo_vertex_id topology) {
	ON_2dPoint uv;
	uv[angular_dir] = angular_domain.ParameterAt(angular_fraction);
	uv[open_dir] = open_domain.ParameterAt(open_fraction);
	const int index = (int)lune_native.size();
	lune_native.push_back(std::make_pair(uv.x, uv.y));
	lune_points_3d.push_back(NULL);
	lune_topology.push_back(topology);
	lune_outer.push_back(index);
    };
    add_lune_point(0.2, 0.0, chart.pole_topology_vertex());
    add_lune_point(0.2, 0.25, CDT_TOPOLOGY_ID_NONE);
    add_lune_point(0.2, 0.5, CDT_TOPOLOGY_ID_NONE);
    add_lune_point(0.2, 0.75, CDT_TOPOLOGY_ID_NONE);
    add_lune_point(0.4, 1.0, chart.second_pole_topology_vertex());
    add_lune_point(0.4, 0.75, CDT_TOPOLOGY_ID_NONE);
    add_lune_point(0.4, 0.5, CDT_TOPOLOGY_ID_NONE);
    add_lune_point(0.4, 0.25, CDT_TOPOLOGY_ID_NONE);
    lune_outer.push_back(lune_outer.front());
    cdt_face_chart lune_chart;
    if (!lune_chart.build(face, lune_native, lune_outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), lune_points_3d, lune_topology) ||
	    lune_chart.type() != CDT_FACE_CHART_POLAR) {
	std::cerr << "zero-interior-sample sphere lune failed: "
	    << lune_chart.failure() << std::endl;
	return false;
    }

    int pole_count = 0;
    for (const cdt_chart_vertex &vertex : chart.vertices)
	pole_count += vertex.singular ? 1 : 0;
    if (pole_count != 2) {
	std::cerr << "polar chart did not preserve both poles" << std::endl;
	return false;
    }

    const long sample_triangle[3] = {
	steiner[0], steiner[4], steiner[8]
    };
    const int sample_orientation =
	chart.triangle_orientation(sample_triangle);
    const long reversed_triangle[3] = {
	sample_triangle[0], sample_triangle[2], sample_triangle[1]
    };
    ON_2dPoint interior_native;
    if (!sample_orientation ||
	    chart.triangle_orientation(reversed_triangle) !=
	    -sample_orientation ||
	    !chart.triangle_interior_sample(sample_triangle,
		interior_native)) {
	std::cerr << "polar chart triangle sampling failed" << std::endl;
	return false;
    }
    ON_2dPoint interior_chart;
    if (!chart.native_to_chart(interior_native, interior_chart))
	return false;
    const auto chart_coordinate = [&](long native, ON_2dPoint &point) {
	for (const cdt_chart_vertex &vertex : chart.vertices) {
	    if (vertex.native_point != native || vertex.id < 0 ||
		    (size_t)vertex.id >= chart.points.size())
		continue;
	    point = ON_2dPoint(chart.points[(size_t)vertex.id].first,
		chart.points[(size_t)vertex.id].second);
	    return true;
	}
	return false;
    };
    ON_2dPoint sample_chart[3];
    for (int corner = 0; corner < 3; ++corner) {
	if (!chart_coordinate(sample_triangle[corner],
		sample_chart[corner]))
	    return false;
    }
    for (int corner = 0; corner < 3; ++corner) {
	const int next = (corner + 1) % 3;
	const long double edge_x =
	    (long double)sample_chart[next].x - sample_chart[corner].x;
	const long double edge_y =
	    (long double)sample_chart[next].y - sample_chart[corner].y;
	const long double point_x =
	    (long double)interior_chart.x - sample_chart[corner].x;
	const long double point_y =
	    (long double)interior_chart.y - sample_chart[corner].y;
	const long double side = edge_x * point_y - edge_y * point_x;
	if ((side > 0.0L) - (side < 0.0L) != sample_orientation) {
	    std::cerr << "polar chart sample left its triangle" << std::endl;
	    return false;
	}
    }

    const std::pair<int, int> split_constraint = chart.constraints.front();
    const long split_edge[2] = {
	chart.native_point(split_constraint.first),
	chart.native_point(split_constraint.second)
    };
    ON_2dPoint edge_native;
    ON_2dPoint edge_chart;
    ON_2dPoint edge_chart_endpoints[2];
    const size_t old_point_count = chart.points.size();
    const size_t old_constraint_count = chart.constraints.size();
    if (split_edge[0] < 0 || split_edge[1] < 0 ||
	    !chart_coordinate(split_edge[0], edge_chart_endpoints[0]) ||
	    !chart_coordinate(split_edge[1], edge_chart_endpoints[1]) ||
	    !chart.edge_midpoint_sample(split_edge, edge_native,
		edge_chart)) {
	std::cerr << "polar chart edge sampling failed" << std::endl;
	return false;
    }
    const ON_2dPoint expected_edge_chart(
	0.5 * (edge_chart_endpoints[0].x + edge_chart_endpoints[1].x),
	0.5 * (edge_chart_endpoints[0].y + edge_chart_endpoints[1].y));
    if (edge_chart.DistanceTo(expected_edge_chart) > 1.0e-12)
	return false;
    chart.add_refinement_point((long)native_points.size(), edge_native,
	edge_chart, split_edge);
    bool retained_constraint = false;
    int replacement_constraints = 0;
    for (const auto &constraint : chart.constraints) {
	retained_constraint = retained_constraint ||
	    ((constraint.first == split_constraint.first &&
	      constraint.second == split_constraint.second) ||
	     (constraint.first == split_constraint.second &&
	      constraint.second == split_constraint.first));
	replacement_constraints +=
	    constraint.first == (int)old_point_count ||
	    constraint.second == (int)old_point_count;
    }
    if (chart.points.size() != old_point_count + 1 ||
	    chart.vertices.size() != old_point_count + 1 ||
	    chart.constraints.size() != old_constraint_count + 1 ||
	    retained_constraint || replacement_constraints != 2) {
	std::cerr << "polar chart constraint split failed" << std::endl;
	return false;
    }

    for (size_t i = 0; i < native_points.size(); ++i) {
	const ON_2dPoint native_uv(native_points[i].first,
	    native_points[i].second);
	ON_2dPoint chart_uv;
	ON_2dPoint round_trip;
	if (!chart.native_to_chart(native_uv, chart_uv) ||
		!chart.chart_to_native(chart_uv, round_trip)) {
	    std::cerr << "polar chart mapping failed" << std::endl;
	    return false;
	}
	const ON_3dPoint expected = surface->PointAt(native_uv.x,
	    native_uv.y);
	const ON_3dPoint actual = surface->PointAt(round_trip.x,
	    round_trip.y);
	if (actual.DistanceTo(expected) > 1.0e-10 * radius) {
	    std::cerr << "polar chart round trip exceeded tolerance"
		<< std::endl;
	    return false;
	}
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(brep.get(),
	"polar chart fixture");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    if (ON_Brep_CDT_Tessellate(state, 0, NULL) != 0) {
	struct brep_cdt_diagnostic diagnostic;
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	std::cerr << "sphere tessellation failed at stage "
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
    bu_free(mesh_faces, "polar chart fixture faces");
    bu_free(mesh_vertices, "polar chart fixture vertices");
    ON_Brep_CDT_Destroy(state);
    if (!solid) {
	std::cerr << "polar chart output was not solid" << std::endl;
	return false;
    }
    return true;
}

int
main()
{

    if (!exercise_coincident_seams())
	return 1;
    if (cdt_test_periodic_path_orientation())
	return 2;
    if (cdt_test_pole_wedge_seam_orientation())
	return 3;
    const ON_3dPoint centers[6] = {
	ON_3dPoint::Origin,
	ON_3dPoint(10.0, -20.0, 30.0),
	ON_3dPoint(-1.0e6, 2.0e6, -3.0e6),
	ON_3dPoint(1.0e-6, -2.0e-6, 3.0e-6),
	ON_3dPoint(-7.0, 8.0, -9.0),
	ON_3dPoint(9.0e4, -8.0e4, 7.0e4)
    };
    const double radii[6] = {
	1.0e-6, 1.0e-3, 1.0, 1.0e3, 1.0e6, 64.0
    };
    for (int i = 0; i < 6; ++i) {
	if (!exercise_sphere(centers[i], radii[i]))
	    return i + 4;
    }
    return 0;
}
