/*              B R E P _ C D T _ C O N E _ C H A R T . C P P
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
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "brep/cdt.h"
#include "cdt/test_api.h"
#include "../cdt/chart.h"
#include "../cdt/mesh.h"

static bool
exercise_cone(const ON_3dPoint &origin, ON_3dVector axis, double height,
	double radius)
{
    if (!axis.Unitize())
	return false;
    ON_Plane plane(origin, axis);
    ON_Cone cone(plane, height, radius);
    if (!cone.IsValid())
	return false;
    std::unique_ptr<ON_Brep> brep(ON_BrepCone(cone, true, NULL));
    if (!brep || !brep->IsValid() || !brep->IsSolid()) {
	std::cerr << "invalid cone B-Rep" << std::endl;
	return false;
    }

    const ON_BrepFace *cone_face = NULL;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	ON_Cone recognized;
	const ON_Surface *surface = brep->m_F[fi].SurfaceOf();
	if (surface && surface->IsCone(&recognized, 0.05)) {
	    cone_face = &brep->m_F[fi];
	    break;
	}
    }
    if (!cone_face || !cdt_face_uses_cone_chart(*cone_face)) {
	std::cerr << "cone face was not classified as a wedge" << std::endl;
	return false;
    }

    const ON_BrepLoop *loop = cone_face->OuterLoop();
    if (!loop || loop->TrimCount() < 3)
	return false;
    std::vector<std::pair<double, double>> native_points;
    std::vector<int> outer;
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices;
    std::vector<ON_3dPoint> topology_points((size_t)brep->m_V.Count());
    for (int vi = 0; vi < brep->m_V.Count(); ++vi)
	topology_points[(size_t)vi] = brep->m_V[vi].Point();
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim || trim->m_vi[0] < 0)
	    return false;
	const ON_3dPoint uv3 = trim->PointAtStart();
	native_points.push_back(std::make_pair(uv3.x, uv3.y));
	outer.push_back((int)native_points.size() - 1);
	points_3d.push_back(&topology_points[(size_t)trim->m_vi[0]]);
	topology_vertices.push_back(trim->m_vi[0]);
	if (trim->Edge() && trim->Edge()->IsClosed()) {
	    const ON_3dPoint midpoint = trim->PointAt(trim->Domain().Mid());
	    native_points.push_back(std::make_pair(midpoint.x, midpoint.y));
	    outer.push_back((int)native_points.size() - 1);
	    points_3d.push_back(NULL);
	    topology_vertices.push_back(CDT_TOPOLOGY_ID_NONE);
	}
    }
    outer.push_back(outer[0]);
    const ON_Surface *surface = cone_face->SurfaceOf();
    const ON_2dPoint refinement_uv(surface->Domain(0).Mid(),
	surface->Domain(1).Mid());
    const int refinement_point = (int)native_points.size();
    native_points.push_back(std::make_pair(refinement_uv.x,
	refinement_uv.y));
    points_3d.push_back(NULL);
    topology_vertices.push_back(CDT_TOPOLOGY_ID_NONE);
    const std::vector<int> refinement(1, refinement_point);

    cdt_face_chart chart;
    if (!chart.build(*cone_face, native_points, outer,
	    std::vector<std::vector<int>>(), refinement, refinement,
	    points_3d, topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CONE_WEDGE ||
	    chart.pole_topology_vertex() < 0 || chart.outer.size() < 3 ||
	    chart.constraints.empty() || chart.steiner.size() != 1) {
	std::cerr << "cone chart build failed: " << chart.failure()
	    << std::endl;
	return false;
    }

    int pole_count = 0;
    for (const cdt_chart_vertex &vertex : chart.vertices) {
	if (!vertex.singular)
	    continue;
	pole_count++;
	if (vertex.topo_vertex != chart.pole_topology_vertex())
	    return false;
    }
    if (pole_count != 1) {
	std::cerr << "cone chart did not collapse to one pole" << std::endl;
	return false;
    }

    bool matched_seam_copies = false;
    for (const cdt_chart_vertex &low : chart.vertices) {
	if (low.seam_side >= 0 || low.native_point < 0 ||
		(size_t)low.native_point >= points_3d.size() ||
		!points_3d[(size_t)low.native_point])
	    continue;
	for (const cdt_chart_vertex &high : chart.vertices) {
	    if (high.seam_side <= 0 || high.native_point < 0 ||
		    (size_t)high.native_point >= points_3d.size() ||
		    points_3d[(size_t)high.native_point] !=
		    points_3d[(size_t)low.native_point])
		continue;
	    if (!(chart.points[(size_t)low.id].second > 0.0))
		continue;
	    matched_seam_copies = true;
	    if (!(chart.points[(size_t)low.id].first < 0.0) ||
		    !(chart.points[(size_t)high.id].first > 0.0)) {
		std::cerr << "cone seam copies occupy the same chart side"
		    << std::endl;
		return false;
	    }
	}
    }
    if (!matched_seam_copies) {
	std::cerr << "cone chart did not retain a matching seam sample"
	    << std::endl;
	return false;
    }

    /* A loop through the pole can meet the two chart copies of one seam
     * point at a radial value crossed by another boundary segment.  Joining
     * those copies with a false straight chart edge makes an otherwise valid
     * cone boundary self-intersect.  The chart must instead lift one branch
     * through an equivalent periodic image and merge the seam copies. */
    const int cut_closed = chart.closed_direction();
    const int cut_open = 1 - cut_closed;
    const ON_Interval cut_closed_domain = surface->Domain(cut_closed);
    const ON_Interval cut_open_domain = surface->Domain(cut_open);
    const bool cut_low_pole = chart.singular_side() == 0 ||
	chart.singular_side() == 3;
    const double cut_pole = cut_low_pole ? cut_open_domain.Min() :
	cut_open_domain.Max();
    const auto cut_uv = [&](double turn, double radial) {
	ON_2dPoint uv;
	uv[cut_closed] = cut_closed_domain.ParameterAt(turn);
	uv[cut_open] = cut_low_pole ?
	    cut_pole + radial * cut_open_domain.Length() :
	    cut_pole - radial * cut_open_domain.Length();
	return uv;
    };
    const ON_2dPoint cut_uvs[5] = {
	cut_uv(1.0, 0.8), cut_uv(0.85, 0.85), cut_uv(0.83, 0.75),
	cut_uv(0.5, 0.0), cut_uv(0.0, 0.8)
    };
    std::vector<std::pair<double, double>> cut_native;
    std::vector<ON_3dPoint> cut_storage(5);
    std::vector<const ON_3dPoint *> cut_points_3d(5);
    for (size_t i = 0; i < 5; ++i) {
	cut_native.push_back(std::make_pair(cut_uvs[i].x, cut_uvs[i].y));
	cut_storage[i] = surface->PointAt(cut_uvs[i].x, cut_uvs[i].y);
	cut_points_3d[i] = &cut_storage[i];
    }
    ON_3dPoint shared_cut_point = cut_storage[0];
    cut_points_3d[0] = &shared_cut_point;
    cut_points_3d[4] = &shared_cut_point;
    const std::vector<int> cut_outer = {0, 1, 2, 3, 4, 0};
    std::vector<cdt_topo_vertex_id> cut_topology(5,
	CDT_TOPOLOGY_ID_NONE);
    cut_topology[0] = 2001;
    cut_topology[3] = chart.pole_topology_vertex();
    cut_topology[4] = 2001;
    cdt_face_chart cut_chart;
    if (!cut_chart.build(*cone_face, cut_native, cut_outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), cut_points_3d, cut_topology,
	    chart.pole_topology_vertex()) || cut_chart.outer.size() != 4) {
	std::cerr << "crossing cone seam closure was not repaired: "
	    << cut_chart.failure() << std::endl;
	return false;
    }
    std::vector<point2d_t> cut_chart_points(cut_chart.points.size());
    for (size_t i = 0; i < cut_chart.points.size(); ++i)
	V2SET(cut_chart_points[i], cut_chart.points[i].first,
	    cut_chart.points[i].second);
    std::vector<int> cut_outline(cut_chart.outer);
    cut_outline.push_back(cut_outline.front());
    std::vector<int> cut_constraints;
    for (const std::pair<int, int> &constraint : cut_chart.constraints) {
	cut_constraints.push_back(constraint.first);
	cut_constraints.push_back(constraint.second);
    }
    int *cut_faces = NULL;
    int cut_face_count = 0;
    struct bg_triangulation_report cut_report = {
	BG_TRIANGULATION_OK, -1, {0}
    };
    const int cut_status = bg_nested_poly_triangulate_strict(
	&cut_faces, &cut_face_count, NULL, NULL, cut_outline.data(),
	cut_outline.size(), NULL, NULL, 0, NULL, 0,
	cut_constraints.empty() ? NULL : cut_constraints.data(),
	cut_chart.constraints.size(), cut_chart_points.data(),
	cut_chart_points.size(), &cut_report);
    bu_free(cut_faces, "crossing cone seam fixture faces");
    if (cut_status != BRLCAD_OK || cut_face_count <= 0) {
	std::cerr << "repaired cone seam closure did not triangulate: "
	    << cut_report.message << std::endl;
	return false;
    }

    /* A partial cone can begin on the native high bound and continue into
     * the next unwrapped period.  The chart must put that radial boundary
     * on the side selected by the adjacent rim, not blindly on the numeric
     * high side of the surface domain. */
    const int partial_closed = chart.closed_direction();
    const int partial_open = 1 - partial_closed;
    const ON_Interval partial_closed_domain =
	surface->Domain(partial_closed);
    const ON_Interval partial_open_domain = surface->Domain(partial_open);
    const bool partial_low_pole = chart.singular_side() == 0 ||
	chart.singular_side() == 3;
    const double partial_pole = partial_low_pole ?
	partial_open_domain.Min() : partial_open_domain.Max();
    const double partial_rim = partial_low_pole ?
	partial_open_domain.Max() : partial_open_domain.Min();
    const double partial_middle =
	0.5 * (partial_pole + partial_rim);
    const double partial_period = partial_closed_domain.Length();
    std::vector<std::pair<double, double>> partial_native(6);
    const auto set_partial = [&](size_t point, double angular,
	    double radial) {
	ON_2dPoint uv;
	uv[partial_closed] = angular;
	uv[partial_open] = radial;
	partial_native[point] = std::make_pair(uv.x, uv.y);
    };
    set_partial(0, partial_closed_domain.Max(), partial_pole);
    set_partial(1, partial_closed_domain.Max(), partial_middle);
    set_partial(2, partial_closed_domain.Max(), partial_rim);
    set_partial(3, partial_closed_domain.Max() +
	0.25 * partial_period, partial_rim);
    set_partial(4, partial_closed_domain.Max() +
	0.5 * partial_period, partial_rim);
    set_partial(5, partial_closed_domain.Max() +
	0.5 * partial_period, partial_middle);
    const std::vector<int> partial_outer = {0, 1, 2, 3, 4, 5, 0};
    std::vector<ON_3dPoint> partial_model(6);
    std::vector<const ON_3dPoint *> partial_points_3d(6);
    for (size_t i = 0; i < partial_native.size(); ++i) {
	ON_2dPoint uv(partial_native[i].first, partial_native[i].second);
	uv[partial_closed] = partial_closed_domain.Min() +
	    std::fmod(uv[partial_closed] - partial_closed_domain.Min(),
		partial_period);
	partial_model[i] = surface->PointAt(uv.x, uv.y);
	partial_points_3d[i] = &partial_model[i];
    }
    std::vector<cdt_topo_vertex_id> partial_topology(6,
	CDT_TOPOLOGY_ID_NONE);
    partial_topology[0] = chart.pole_topology_vertex();
    partial_topology[2] = 1001;
    partial_topology[4] = 1002;
    cdt_face_chart partial_chart;
    if (!partial_chart.build(*cone_face, partial_native, partial_outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), partial_points_3d, partial_topology)) {
	std::cerr << "partial cone wedge chart failed: "
	    << partial_chart.failure() << std::endl;
	return false;
    }
    bool oriented_partial_seam = true;
    for (const cdt_chart_vertex &vertex : partial_chart.vertices) {
	if (vertex.native_point != 1 && vertex.native_point != 2)
	    continue;
	oriented_partial_seam = oriented_partial_seam &&
	    vertex.seam_side < 0 &&
	    partial_chart.points[(size_t)vertex.id].first < 0.0;
    }
    if (!oriented_partial_seam) {
	std::cerr << "partial cone seam ignored boundary continuity"
	    << std::endl;
	return false;
    }

    /* A damaged pcurve can report its pole parameter for a legitimate
     * interior sample of a radial edge.  An atlas chart with an explicit
     * topological pole must recover that sample from the authoritative 3-D
     * edge point instead of collapsing it into the apex. */
    size_t radial_position = outer.size();
    for (size_t i = 0; i + 1 < outer.size(); ++i) {
	const int first = outer[i];
	const int second = outer[i + 1];
	const bool first_pole = topology_vertices[(size_t)first] ==
	    chart.pole_topology_vertex();
	const bool second_pole = topology_vertices[(size_t)second] ==
	    chart.pole_topology_vertex();
	if (first_pole == second_pole || !points_3d[(size_t)first] ||
		!points_3d[(size_t)second])
	    continue;
	radial_position = i;
	break;
    }
    if (radial_position == outer.size()) {
	std::cerr << "cone fixture has no radial pole edge" << std::endl;
	return false;
    }
    const int radial_first = outer[radial_position];
    const int radial_second = outer[radial_position + 1];
    const int pole_native = topology_vertices[(size_t)radial_first] ==
	chart.pole_topology_vertex() ? radial_first : radial_second;
    ON_3dPoint repaired_point = 0.5 *
	(*points_3d[(size_t)radial_first] +
	 *points_3d[(size_t)radial_second]);
    std::vector<std::pair<double, double>> repaired_native =
	native_points;
    std::vector<int> repaired_outer = outer;
    std::vector<const ON_3dPoint *> repaired_points_3d = points_3d;
    std::vector<cdt_topo_vertex_id> repaired_topology =
	topology_vertices;
    const int repaired_sample = (int)repaired_native.size();
    repaired_native.push_back(native_points[(size_t)pole_native]);
    repaired_points_3d.push_back(&repaired_point);
    repaired_topology.push_back(CDT_TOPOLOGY_ID_NONE);
    repaired_outer.insert(repaired_outer.begin() + radial_position + 1,
	repaired_sample);
    cdt_face_chart repaired_chart;
    if (!repaired_chart.build(*cone_face, repaired_native, repaired_outer,
	    std::vector<std::vector<int>>(), refinement, refinement,
	    repaired_points_3d, repaired_topology,
	    chart.pole_topology_vertex())) {
	std::cerr << "authoritative cone repair failed: "
	    << repaired_chart.failure() << std::endl;
	return false;
    }
    bool retained_repaired_sample = false;
    for (int boundary_point : repaired_chart.outer) {
	if (repaired_chart.native_point(boundary_point) != repaired_sample)
	    continue;
	retained_repaired_sample = !repaired_chart.vertices[
	    (size_t)boundary_point].singular && repaired_chart.points[
	    (size_t)boundary_point] != std::make_pair(0.0, 0.0);
    }
    if (!retained_repaired_sample) {
	std::cerr << "authoritative cone repair collapsed an edge sample"
	    << std::endl;
	return false;
    }

    /* Exercise non-topological copies of one sample on both native seam
     * bounds.  Analytic closest-point projection is free to return either
     * periodic image, but the bounds must remain opposite chart sides. */
    std::vector<std::pair<double, double>> seam_native = native_points;
    std::vector<const ON_3dPoint *> seam_points_3d = points_3d;
    std::vector<cdt_topo_vertex_id> seam_topology = topology_vertices;
    std::vector<int> seam_outer;
    std::vector<int> seam_samples;
    ON_3dPoint seam_point = ON_3dPoint::UnsetPoint;
    const int closed_direction = chart.closed_direction();
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const double seam_tolerance = 256.0 * DBL_EPSILON *
	std::max(std::max(std::fabs(closed_domain.Min()),
	    std::fabs(closed_domain.Max())), closed_domain.Length());
    const auto seam_side = [&](double parameter) {
	if (std::fabs(parameter - closed_domain.Min()) <= seam_tolerance)
	    return -1;
	if (std::fabs(parameter - closed_domain.Max()) <= seam_tolerance)
	    return 1;
	return 0;
    };
    for (size_t i = 0; i + 1 < outer.size(); ++i) {
	const int first = outer[i];
	const int second = outer[i + 1];
	seam_outer.push_back(first);
	const int first_side = seam_side(closed_direction ?
	    native_points[(size_t)first].second :
	    native_points[(size_t)first].first);
	const int second_side = seam_side(closed_direction ?
	    native_points[(size_t)second].second :
	    native_points[(size_t)second].first);
	if (!first_side || first_side != second_side)
	    continue;
	ON_2dPoint sample;
	sample[closed_direction] = first_side < 0 ? closed_domain.Min() :
	    closed_domain.Max();
	sample[open_direction] = 0.5 *
	    ((open_direction ? native_points[(size_t)first].second :
	    native_points[(size_t)first].first) +
	    (open_direction ? native_points[(size_t)second].second :
	    native_points[(size_t)second].first));
	if (seam_samples.empty())
	    seam_point = surface->PointAt(sample.x, sample.y);
	const int sample_index = (int)seam_native.size();
	seam_native.push_back(std::make_pair(sample.x, sample.y));
	seam_points_3d.push_back(&seam_point);
	seam_topology.push_back(CDT_TOPOLOGY_ID_NONE);
	seam_samples.push_back(sample_index);
	seam_outer.push_back(sample_index);
    }
    seam_outer.push_back(seam_outer.front());
    if (seam_samples.size() != 2 || !seam_point.IsValid()) {
	std::cerr << "cone fixture did not expose two seam paths" << std::endl;
	return false;
    }
    cdt_face_chart seam_chart;
    if (!seam_chart.build(*cone_face, seam_native, seam_outer,
	    std::vector<std::vector<int>>(), refinement, refinement,
	    seam_points_3d, seam_topology, chart.pole_topology_vertex())) {
	std::cerr << "sampled cone seam chart failed: "
	    << seam_chart.failure() << std::endl;
	return false;
    }
    bool low_sample = false;
    bool high_sample = false;
    for (const cdt_chart_vertex &vertex : seam_chart.vertices) {
	if (vertex.native_point != seam_samples[0] &&
		vertex.native_point != seam_samples[1])
	    continue;
	const double x = seam_chart.points[(size_t)vertex.id].first;
	low_sample = low_sample || (vertex.seam_side < 0 && x < 0.0);
	high_sample = high_sample || (vertex.seam_side > 0 && x > 0.0);
    }
    if (!low_sample || !high_sample) {
	std::cerr << "authoritative cone seam samples share one chart side"
	    << std::endl;
	return false;
    }

    /* Two distinct chart cells can use the low and high copies of one seam
     * sample and collapse onto the same three model-space vertices.  Verify
     * that the mesh refinement recognizes this quotient collision and adds
     * one authoritative surface sample inside each chart cell. */
    long alias_pole_native = -1;
    long low_native = -1;
    long high_native = -1;
    for (const cdt_chart_vertex &vertex : seam_chart.vertices) {
	if (vertex.singular)
	    alias_pole_native = vertex.native_point;
	if (vertex.native_point != seam_samples[0] &&
		vertex.native_point != seam_samples[1])
	    continue;
	if (vertex.seam_side < 0)
	    low_native = vertex.native_point;
	if (vertex.seam_side > 0)
	    high_native = vertex.native_point;
    }
    long boundary_native = -1;
    for (const cdt_chart_vertex &chart_vertex : seam_chart.vertices) {
	const long candidate = chart_vertex.native_point;
	if (candidate < 0 || candidate == alias_pole_native ||
		candidate == low_native || candidate == high_native ||
		(size_t)candidate >= seam_native.size())
	    continue;
	const long low_triangle[3] = {
	    alias_pole_native, candidate, low_native
	};
	const long high_triangle[3] = {
	    candidate, alias_pole_native, high_native
	};
	ON_2dPoint low_interior;
	ON_2dPoint high_interior;
	if (seam_chart.triangle_interior_sample(low_triangle,
		low_interior) &&
		seam_chart.triangle_interior_sample(high_triangle,
		high_interior)) {
	    boundary_native = candidate;
	    break;
	}
    }
    if (alias_pole_native < 0 || low_native < 0 || high_native < 0 ||
	    boundary_native < 0) {
	std::cerr << "cone fixture did not expose aliased chart cells"
	    << std::endl;
	return false;
    }
    cdt_mesh_t quotient_mesh;
    quotient_mesh.brep = brep.get();
    quotient_mesh.f_id = cone_face->m_face_index;
    quotient_mesh.m_pnts_2d = seam_native;
    quotient_mesh.m_face_charts.push_back(seam_chart);
    long shared_seam_point = -1;
    const long alias_points[4] = {
	alias_pole_native, boundary_native, low_native, high_native
    };
    for (long native : alias_points) {
	if ((native == low_native || native == high_native) &&
		shared_seam_point >= 0) {
	    quotient_mesh.p2d3d[native] = shared_seam_point;
	    continue;
	}
	ON_3dPoint value = seam_points_3d[(size_t)native] ?
	    *seam_points_3d[(size_t)native] : surface->PointAt(
		seam_native[(size_t)native].first,
		seam_native[(size_t)native].second);
	ON_3dPoint *stored = new ON_3dPoint(value);
	const long point = (long)quotient_mesh.pnts.size();
	quotient_mesh.pnts.push_back(stored);
	quotient_mesh.p2ind[stored] = point;
	quotient_mesh.p2d3d[native] = point;
	if (native == low_native || native == high_native)
	    shared_seam_point = point;
    }
    triangle_t low_triangle;
    low_triangle.v[0] = alias_pole_native;
    low_triangle.v[1] = boundary_native;
    low_triangle.v[2] = low_native;
    quotient_mesh.tris_2d.push_back(low_triangle);
    triangle_t high_triangle;
    high_triangle.v[0] = boundary_native;
    high_triangle.v[1] = alias_pole_native;
    high_triangle.v[2] = high_native;
    quotient_mesh.tris_2d.push_back(high_triangle);
    const size_t quotient_samples =
	quotient_mesh.refine_collapsed_chart_triangles(8);
    bool separated_cells = quotient_samples == 2 &&
	quotient_mesh.m_chart_refinement_pnts.size() == 2;
    std::vector<const ON_3dPoint *> refined_points;
    for (long native : quotient_mesh.m_chart_refinement_pnts) {
	const auto point = quotient_mesh.p2d3d.find(native);
	if (point == quotient_mesh.p2d3d.end() || point->second < 0 ||
		(size_t)point->second >= quotient_mesh.pnts.size()) {
	    separated_cells = false;
	    continue;
	}
	refined_points.push_back(
	    quotient_mesh.pnts[(size_t)point->second]);
    }
    separated_cells = separated_cells && refined_points.size() == 2 &&
	refined_points[0]->DistanceTo(*refined_points[1]) >
	ON_ZERO_TOLERANCE;
    for (ON_3dPoint *point : quotient_mesh.pnts)
	delete point;
    for (ON_3dPoint *normal : quotient_mesh.normals)
	delete normal;
    quotient_mesh.pnts.clear();
    quotient_mesh.normals.clear();
    if (!separated_cells) {
	std::cerr << "collapsed cone chart cells were not separated"
	    << std::endl;
	return false;
    }

    for (const std::pair<int, int> &constraint : chart.constraints) {
	if (constraint.first < 0 || constraint.second < 0 ||
		(size_t)constraint.first >= chart.vertices.size() ||
		(size_t)constraint.second >= chart.vertices.size() ||
		!chart.vertices[(size_t)constraint.first].singular) {
	    std::cerr << "cone chart fan constraint lost its apex"
		<< std::endl;
	    return false;
	}
    }

    for (size_t i = 0; i < chart.outer.size(); ++i) {
	const int first = chart.outer[i];
	const int second = chart.outer[(i + 1) % chart.outer.size()];
	if (first == second) {
	    std::cerr << "repeated cone chart boundary vertex" << std::endl;
	    return false;
	}
	const long native_first = chart.native_point(first);
	const long native_second = chart.native_point(second);
	if (native_first < 0 || native_second < 0) {
	    std::cerr << "cone chart lost native UV identity" << std::endl;
	    return false;
	}
	const ON_2dPoint uv_first(
	    native_points[(size_t)native_first].first,
	    native_points[(size_t)native_first].second);
	const ON_2dPoint uv_second(
	    native_points[(size_t)native_second].first,
	    native_points[(size_t)native_second].second);
	if (surface->PointAt(uv_first.x, uv_first.y).DistanceTo(
		surface->PointAt(uv_second.x, uv_second.y)) <=
		ON_ZERO_TOLERANCE) {
	    std::cerr << "cone chart retained a collapsed constraint"
		<< std::endl;
	    return false;
	}
    }

    for (size_t i = 0; i < native_points.size(); ++i) {
	ON_2dPoint chart_uv;
	ON_2dPoint round_trip;
	const ON_2dPoint native_uv(native_points[i].first,
	    native_points[i].second);
	if (!chart.native_to_chart(native_uv, chart_uv) ||
		!chart.chart_to_native(chart_uv, round_trip)) {
	    std::cerr << "cone chart mapping failed" << std::endl;
	    return false;
	}
	const ON_3dPoint expected = surface->PointAt(native_uv.x, native_uv.y);
	const ON_3dPoint actual = surface->PointAt(round_trip.x,
	    round_trip.y);
	const double scale = std::max(std::fabs(height), std::fabs(radius));
	if (actual.DistanceTo(expected) > 1.0e-10 * scale) {
	    std::cerr << "cone chart round trip exceeded tolerance: "
		<< actual.DistanceTo(expected) << std::endl;
	    return false;
	}
    }

    struct ON_Brep_CDT_State *state = ON_Brep_CDT_Create(brep.get(),
	"cone chart fixture");
    struct bg_tess_tol tolerance = BG_TESS_TOL_INIT_ZERO;
    tolerance.rel = 0.05;
    ON_Brep_CDT_Tol_Set(state, &tolerance);
    if (ON_Brep_CDT_Tessellate(state, 0, NULL) != 0) {
	struct brep_cdt_diagnostic diagnostic;
	ON_Brep_CDT_Diagnostic(&diagnostic, state);
	std::cerr << "cone tessellation failed at stage " << diagnostic.stage
	    << ": " << diagnostic.message << std::endl;
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
    bu_free(mesh_faces, "cone chart fixture faces");
    bu_free(mesh_vertices, "cone chart fixture vertices");
    ON_Brep_CDT_Destroy(state);
    if (!solid) {
	std::cerr << "cone chart output was not solid" << std::endl;
	return false;
    }
    return true;
}

int
main()
{
    const ON_3dPoint origins[10] = {
	ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(10.0, -20.0, 30.0),
	ON_3dPoint(-1.0e6, 2.0e6, -3.0e6),
	ON_3dPoint(1.0e-6, -2.0e-6, 3.0e-6),
	ON_3dPoint(4.0, 5.0, 6.0),
	ON_3dPoint(-7.0, 8.0, -9.0),
	ON_3dPoint(100.0, 200.0, 300.0),
	ON_3dPoint(-0.25, 0.5, -0.75),
	ON_3dPoint(9.0e4, -8.0e4, 7.0e4),
	ON_3dPoint(-11.0, -12.0, 13.0)
    };
    const ON_3dVector axes[10] = {
	ON_3dVector(0.0, 0.0, 1.0),
	ON_3dVector(1.0, 0.0, 0.0),
	ON_3dVector(0.0, 1.0, 0.0),
	ON_3dVector(1.0, 1.0, 1.0),
	ON_3dVector(-1.0, 2.0, 3.0),
	ON_3dVector(3.0, -2.0, 1.0),
	ON_3dVector(-2.0, -3.0, 4.0),
	ON_3dVector(5.0, 1.0, -2.0),
	ON_3dVector(2.0, -5.0, -1.0),
	ON_3dVector(-4.0, 3.0, -2.0)
    };
    const double scales[10] = {
	1.0e-6, 1.0e-3, 1.0, 1.0e3, 1.0e6,
	2.0, 0.125, 64.0, 4096.0, 0.03125
    };
    for (int i = 0; i < 10; ++i) {
	if (!exercise_cone(origins[i], axes[i], 4.0 * scales[i],
		1.5 * scales[i]))
	    return i + 1;
    }
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
