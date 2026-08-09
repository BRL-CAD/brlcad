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

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "bg/trimesh.h"
#include "brep/cdt.h"
#include "../cdt/chart.h"

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

    cdt_face_chart chart;
    if (!chart.build(*cone_face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(), points_3d,
	    topology_vertices) ||
	    chart.type() != CDT_FACE_CHART_CONE_WEDGE ||
	    chart.pole_topology_vertex() < 0 || chart.outer.size() < 3 ||
	    chart.constraints.empty()) {
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

    const ON_Surface *surface = cone_face->SurfaceOf();
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
