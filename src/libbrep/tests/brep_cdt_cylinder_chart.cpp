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
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

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
    return 0;
}
