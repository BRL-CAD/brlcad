/*                   B R E P _ F A S T _ R E P A I R . C P P
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
#include <map>
#include <set>
#include <vector>

#include "brep/cdt.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"

struct fast_result {
    int ret = BREP_CDT_FAST_ERROR;
    int face_count = 0;
    int point_count = 0;
    int *faces = NULL;
    vect_t *normals = NULL;
    point_t *points = NULL;
    struct brep_cdt_fast_report report = {};

    ~fast_result()
    {
	bu_free(faces, "fast repair test faces");
	bu_free(normals, "fast repair test normals");
	bu_free(points, "fast repair test points");
    }
};

static fast_result *
run_fast(const ON_Brep &brep)
{
    fast_result *result = new fast_result;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    options.max_workers = 1;
    result->ret = brep_cdt_fast_ex(&result->faces, &result->face_count,
	&result->normals, &result->points, &result->point_count, &brep, -1,
	&ttol, &tol, &options, &result->report);
    return result;
}

static ON_PlaneSurface *
large_plane()
{
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, -10.0, 10.0);
    surface->SetDomain(1, -10.0, 10.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    return surface;
}

static ON_NurbsCurve *
nurbs_curve(int dimension, int degree,
	const std::vector<ON_3dPoint> &control_points)
{
    ON_NurbsCurve *curve = new ON_NurbsCurve(dimension, false, degree + 1,
	(int)control_points.size());
    for (size_t i = 0; i < control_points.size(); ++i)
	curve->SetCV((int)i, control_points[i]);
    curve->MakeClampedUniformKnotVector();
    curve->SetDomain(0.0, 1.0);
    return curve;
}

static ON_BrepTrim &
add_nurbs_trim(ON_Brep &brep, ON_BrepLoop &loop, int start_vertex,
	int end_vertex, const std::vector<ON_3dPoint> &control_points,
	double tolerance)
{
    ON_NurbsCurve *edge_curve = nurbs_curve(3,
	(int)control_points.size() == 4 ? 3 : 1, control_points);
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[start_vertex],
	brep.m_V[end_vertex], c3i);
    edge.m_tolerance = tolerance;

    ON_NurbsCurve *trim_curve = nurbs_curve(2,
	(int)control_points.size() == 4 ? 3 : 1, control_points);
    const int c2i = brep.AddTrimCurve(trim_curve);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = ON_Surface::not_iso;
    trim.m_tolerance[0] = tolerance;
    trim.m_tolerance[1] = tolerance;
    return trim;
}

static bool
thin_lens_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const int va = brep.NewVertex(ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
    const int vb = brep.NewVertex(ON_3dPoint(1.0, 0.0, 0.0)).m_vertex_index;
    const double height = 1.0e-4;
    add_nurbs_trim(brep, loop, va, vb, {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(0.33, height, 0.0),
	ON_3dPoint(0.67, height, 0.0), ON_3dPoint(1.0, 0.0, 0.0)
    }, 1.0e-6);
    add_nurbs_trim(brep, loop, vb, va, {
	ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(0.67, -height, 0.0),
	ON_3dPoint(0.33, -height, 0.0), ON_3dPoint(0.0, 0.0, 0.0)
    }, 1.0e-6);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
degenerate_line_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const int vertex = brep.NewVertex(
	ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
    add_nurbs_trim(brep, loop, vertex, vertex, {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0)
    }, 1.0e-6);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.completed_faces == 1 &&
	result->report.failed_faces == 0 &&
	result->report.skipped_degenerate_faces == 1 &&
	result->face_count == 0 && result->point_count == 0;
    delete result;
    return valid;
}

static bool
degenerate_collinear_loop_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const int v0 = brep.NewVertex(
	ON_3dPoint(0.0, 0.0, 0.0)).m_vertex_index;
    const int v1 = brep.NewVertex(
	ON_3dPoint(1.0, 0.0, 0.0)).m_vertex_index;
    const int v2 = brep.NewVertex(
	ON_3dPoint(2.0, 0.0, 0.0)).m_vertex_index;
    add_nurbs_trim(brep, loop, v0, v1, {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0)
    }, 1.0e-6);
    add_nurbs_trim(brep, loop, v1, v2, {
	ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(2.0, 0.0, 0.0)
    }, 1.0e-6);
    add_nurbs_trim(brep, loop, v2, v1, {
	ON_3dPoint(2.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0)
    }, 1.0e-6);
    add_nurbs_trim(brep, loop, v1, v0, {
	ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(0.0, 0.0, 0.0)
    }, 1.0e-6);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.completed_faces == 1 &&
	result->report.failed_faces == 0 &&
	result->report.skipped_degenerate_faces == 1 &&
	result->face_count == 0 && result->point_count == 0;
    delete result;
    return valid;
}

static ON_BrepTrim &
add_periodic_trim(ON_Brep &brep, ON_BrepLoop &loop,
	const ON_Surface &surface, double v, bool decreasing,
	ON_BrepLoop::TYPE loop_type, bool collapsed_parameter = false,
	bool extra_winding = false)
{
    const ON_Interval udom = surface.Domain(0);
    const ON_2dPoint start(decreasing ? udom.Max() : udom.Min(), v);
    const ON_2dPoint end(decreasing ? udom.Min() : udom.Max(), v);
    ON_Curve *edge_curve = surface.IsoCurve(0, v);
    if (collapsed_parameter)
	edge_curve->ChangeClosedCurveSeam(udom.Mid());
    const ON_3dPoint point = edge_curve->PointAt(
	edge_curve->Domain().Min());
    const int vertex = brep.NewVertex(point).m_vertex_index;

    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[vertex],
	brep.m_V[vertex], c3i);
    edge.m_tolerance = 1.0e-6;
    ON_Curve *trim_curve = NULL;
    if (collapsed_parameter) {
	const double seam = udom.Mid();
	const double wobble = surface.Domain(1).Length() * 2.0e-4;
	trim_curve = nurbs_curve(2, 3, {
	    ON_3dPoint(seam, v, 0.0),
	    ON_3dPoint(seam, v + wobble, 0.0),
	    ON_3dPoint(seam, v - wobble, 0.0),
	    ON_3dPoint(seam, v, 0.0)
	});
    } else if (extra_winding) {
	const double period = udom.Length();
	trim_curve = nurbs_curve(2, 3, {
	    ON_3dPoint(start.x, v, 0.0),
	    ON_3dPoint(start.x + 2.0 * period, v, 0.0),
	    ON_3dPoint(end.x - 2.0 * period, v, 0.0),
	    ON_3dPoint(end.x, v, 0.0)
	});
    } else {
	trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
    }
    const int c2i = brep.AddTrimCurve(trim_curve);
    loop.m_type = loop_type;
    ON_BrepTrim &trim = brep.NewTrim(edge, decreasing, loop, c2i);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = ON_Surface::x_iso;
    trim.m_tolerance[0] = 1.0e-6;
    trim.m_tolerance[1] = 1.0e-6;
    return trim;
}

static void
add_uv_polyline_trim(ON_Brep &brep, ON_BrepLoop &loop,
	const ON_Surface &surface, int first_vertex, int second_vertex,
	const ON_2dPoint &start, const ON_2dPoint &end)
{
    ON_3dPointArray edge_points;
    for (int sample = 0; sample <= 64; ++sample) {
	const ON_2dPoint uv = start + (double)sample / 64.0 *
	    (end - start);
	edge_points.Append(surface.PointAt(uv.x, uv.y));
    }
    ON_PolylineCurve *edge_curve = new ON_PolylineCurve(edge_points);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first_vertex],
	brep.m_V[second_vertex], brep.AddEdgeCurve(edge_curve));
    edge.m_tolerance = 1.0e-6;
    ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
    trim_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	brep.AddTrimCurve(trim_curve));
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = surface.IsIsoparametric(*trim_curve);
    trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
}

static bool
singular_cap_case(bool decreasing, ON_BrepLoop::TYPE loop_type,
	bool reverse_face, bool north_pole)
{
    ON_Brep brep;
    ON_Sphere sphere(ON_3dPoint::Origin, 1.0);
    ON_RevSurface *surface = sphere.RevSurfaceForm(false);
    if (!surface)
	return false;
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    face.m_bRev = reverse_face;
    ON_BrepLoop &loop = brep.NewLoop(loop_type, face);
    const double v = surface->Domain(1).Mid();
    add_periodic_trim(brep, loop, *surface, v, decreasing, loop_type);

    fast_result *result = run_fast(brep);
    bool found_pole = false;
    const ON_3dPoint pole = north_pole ? sphere.NorthPole() :
	sphere.SouthPole();
    for (int i = 0; result->points && i < result->point_count; ++i) {
	const ON_3dPoint point(result->points[i][X], result->points[i][Y],
	    result->points[i][Z]);
	found_pole = found_pole || point.DistanceTo(pole) < 1.0e-6;
    }
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 && found_pole;
    delete result;
    return valid;
}

static bool
singular_cap_test()
{
    return singular_cap_case(true, ON_BrepLoop::outer, false, false) &&
	singular_cap_case(false, ON_BrepLoop::outer, false, true) &&
	singular_cap_case(true, ON_BrepLoop::inner, false, true) &&
	singular_cap_case(true, ON_BrepLoop::outer, true, false);
}

static bool
periodic_strip_case(bool collapsed_inner,
	ON_BrepLoop::TYPE second_type = ON_BrepLoop::inner)
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    add_periodic_trim(brep, outer, *surface, surface->Domain(1).Min(),
	false, ON_BrepLoop::outer);
    ON_BrepLoop &second = brep.NewLoop(second_type, face);
    add_periodic_trim(brep, second, *surface, surface->Domain(1).Max(),
	false, second_type, collapsed_inner);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
periodic_strip_test()
{
    return periodic_strip_case(false) && periodic_strip_case(true) &&
	periodic_strip_case(false, ON_BrepLoop::outer);
}

static bool
redundant_periodic_boundaries_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const ON_Interval vdom = surface->Domain(1);
    ON_BrepLoop &low = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, low, *surface, vdom.ParameterAt(0.15),
	false, ON_BrepLoop::inner);
    ON_BrepLoop &middle = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, middle, *surface, vdom.ParameterAt(0.50),
	false, ON_BrepLoop::inner, false, true);
    ON_BrepLoop &high = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, high, *surface, vdom.ParameterAt(0.85),
	false, ON_BrepLoop::inner);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
periodic_rectangle_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double low_v = vdom.ParameterAt(0.15);
    const double high_v = vdom.ParameterAt(0.85);
    const int low = brep.NewVertex(surface->PointAt(udom.Min(),
	low_v)).m_vertex_index;
    const int high = brep.NewVertex(surface->PointAt(udom.Min(),
	high_v)).m_vertex_index;

    auto add_trim = [&](ON_Curve *edge_curve, int first, int second,
	    const ON_2dPoint &start, const ON_2dPoint &end,
	    ON_BrepTrim::TYPE type) {
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first],
	    brep.m_V[second], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = type;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    };
    add_trim(surface->IsoCurve(0, low_v), low, low,
	ON_2dPoint(udom.Min(), low_v), ON_2dPoint(udom.Max(), low_v),
	ON_BrepTrim::boundary);
    ON_Curve *east = surface->IsoCurve(1, udom.Max());
    east->Trim(ON_Interval(low_v, high_v));
    add_trim(east, low, high, ON_2dPoint(udom.Max(), low_v),
	ON_2dPoint(udom.Max(), high_v), ON_BrepTrim::seam);
    ON_Curve *top = surface->IsoCurve(0, high_v);
    top->Reverse();
    add_trim(top, high, high, ON_2dPoint(udom.Max(), high_v),
	ON_2dPoint(udom.Min(), high_v), ON_BrepTrim::boundary);
    ON_Curve *west = surface->IsoCurve(1, udom.Min());
    west->Trim(ON_Interval(low_v, high_v));
    west->Reverse();
    add_trim(west, high, low, ON_2dPoint(udom.Min(), high_v),
	ON_2dPoint(udom.Min(), low_v), ON_BrepTrim::seam);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
periodic_zero_area_subcycle_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 17.5);
    ON_Cylinder cylinder(base, 105.65675393633153);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    surface->SetDomain(0, 54.977868507454808, 164.93361138309757);
    surface->SetDomain(1, 0.0, 105.65675393633153);
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const double low_v = 46.828376968165784;
    const double high_v = 58.828376968165784;

    int low_vertices[4];
    int high_vertices[4];
    for (int i = 0; i < 4; ++i) {
	const double u = udom.ParameterAt((double)i / 4.0);
	low_vertices[i] = brep.NewVertex(surface->PointAt(u,
	    low_v)).m_vertex_index;
	high_vertices[i] = brep.NewVertex(surface->PointAt(u,
	    high_v)).m_vertex_index;
    }

    auto add_trim = [&](ON_Curve *edge_curve, int first, int second,
	    const ON_2dPoint &start, const ON_2dPoint &end,
	    ON_BrepTrim::TYPE type) {
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first],
	    brep.m_V[second], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = type;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
	return trim.m_trim_index;
    };

    ON_Curve *west = surface->IsoCurve(1, udom.Min());
    west->Trim(ON_Interval(low_v, high_v));
    west->Reverse();
    const int west_trim_index = add_trim(west, high_vertices[0],
	low_vertices[0], ON_2dPoint(udom.Min(), high_v),
	ON_2dPoint(udom.Min(), low_v), ON_BrepTrim::seam);
    const int seam_edge_index = brep.m_T[west_trim_index].m_ei;

    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	const double start_u = udom.ParameterAt((double)i / 4.0);
	const double end_u = udom.ParameterAt((double)(i + 1) / 4.0);
	ON_Curve *edge_curve = surface->IsoCurve(0, low_v);
	edge_curve->Trim(ON_Interval(start_u, end_u));
	add_trim(edge_curve, low_vertices[i], low_vertices[next],
	    ON_2dPoint(start_u, low_v), ON_2dPoint(end_u, low_v),
	    ON_BrepTrim::boundary);
    }

    ON_LineCurve *east_trim_curve = new ON_LineCurve(
	ON_2dPoint(udom.Max(), low_v), ON_2dPoint(udom.Max(), high_v));
    east_trim_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &east = brep.NewTrim(brep.m_E[seam_edge_index], true,
	loop, brep.AddTrimCurve(east_trim_curve));
    east.m_type = ON_BrepTrim::seam;
    east.m_iso = surface->IsIsoparametric(*east_trim_curve);
    east.m_tolerance[0] = east.m_tolerance[1] = 1.0e-6;

    for (int i = 4; i > 0; --i) {
	const int first = i % 4;
	const int second = (i - 1) % 4;
	const double start_u = udom.ParameterAt((double)i / 4.0);
	const double end_u = udom.ParameterAt((double)(i - 1) / 4.0);
	ON_Curve *edge_curve = surface->IsoCurve(0, high_v);
	edge_curve->Trim(ON_Interval(end_u, start_u));
	edge_curve->Reverse();
	add_trim(edge_curve, high_vertices[first], high_vertices[second],
	    ON_2dPoint(start_u, high_v), ON_2dPoint(end_u, high_v),
	    ON_BrepTrim::boundary);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
overlapping_periodic_pcurves_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double seam = udom.Min();
    const double middle = udom.Mid();
    const double low_v = vdom.ParameterAt(0.20);
    const double high_v = vdom.ParameterAt(0.80);
    const int seam_vertex = brep.NewVertex(surface->PointAt(seam,
	low_v)).m_vertex_index;
    const int middle_vertex = brep.NewVertex(surface->PointAt(middle,
	low_v)).m_vertex_index;

    auto add_half = [&](const ON_Interval &edge_interval, int first,
	    int second, const ON_2dPoint &start, const ON_2dPoint &end) {
	ON_Curve *edge_curve = surface->IsoCurve(0, low_v);
	edge_curve->Trim(edge_interval);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first],
	    brep.m_V[second], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, outer,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = ON_Surface::x_iso;
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    };
    add_half(ON_Interval(udom.Min(), middle), seam_vertex,
	middle_vertex, ON_2dPoint(seam, low_v),
	ON_2dPoint(middle, low_v));
    add_half(ON_Interval(middle, udom.Max()), middle_vertex,
	seam_vertex, ON_2dPoint(middle, low_v),
	ON_2dPoint(seam, low_v));
    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, inner, *surface, high_v, true,
	ON_BrepLoop::inner);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
paired_periodic_strip_case(bool single_high_boundary)
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double seam_u = udom.Mid();
    const double low_v = vdom.ParameterAt(0.25);
    const double high_v = vdom.ParameterAt(0.75);
    const int low_mid = brep.NewVertex(surface->PointAt(
	seam_u, low_v)).m_vertex_index;
    const int low_end = brep.NewVertex(surface->PointAt(
	udom.Min(), low_v)).m_vertex_index;
    const int high_mid = brep.NewVertex(surface->PointAt(
	seam_u, high_v)).m_vertex_index;
    const int high_end = brep.NewVertex(surface->PointAt(
	udom.Min(), high_v)).m_vertex_index;

    auto add_half_boundary = [&](int start_vertex, int end_vertex,
	    double v, const ON_Interval &edge_interval, bool reverse_edge,
	    const ON_2dPoint &trim_start, const ON_2dPoint &trim_end) {
	ON_Curve *edge_curve = surface->IsoCurve(0, v);
	edge_curve->Trim(edge_interval);
	if (reverse_edge)
	    edge_curve->Reverse();
	const int c3i = brep.AddEdgeCurve(edge_curve);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[start_vertex],
	    brep.m_V[end_vertex], c3i);
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(trim_start, trim_end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = ON_Surface::x_iso;
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    };

    add_half_boundary(low_mid, low_end, low_v,
	ON_Interval(udom.Min(), seam_u), true,
	ON_2dPoint(seam_u, low_v), ON_2dPoint(udom.Min(), low_v));
    add_half_boundary(low_end, low_mid, low_v,
	ON_Interval(seam_u, udom.Max()), true,
	ON_2dPoint(udom.Min(), low_v), ON_2dPoint(seam_u, low_v));

    ON_Curve *seam_curve = surface->IsoCurve(1, seam_u);
    seam_curve->Trim(ON_Interval(low_v, high_v));
    const int seam_c3i = brep.AddEdgeCurve(seam_curve);
    ON_BrepEdge &seam_edge = brep.NewEdge(brep.m_V[low_mid],
	brep.m_V[high_mid], seam_c3i);
    const int seam_edge_index = seam_edge.m_edge_index;
    seam_edge.m_tolerance = 1.0e-6;
    ON_LineCurve *up_curve = new ON_LineCurve(
	ON_2dPoint(seam_u, low_v), ON_2dPoint(seam_u, high_v));
    up_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &up = brep.NewTrim(seam_edge, false, loop,
	brep.AddTrimCurve(up_curve));
    up.m_type = ON_BrepTrim::seam;
    up.m_iso = ON_Surface::W_iso;
    up.m_tolerance[0] = up.m_tolerance[1] = 1.0e-6;

    if (single_high_boundary) {
	ON_Curve *edge_curve = surface->IsoCurve(0, high_v);
	edge_curve->ChangeClosedCurveSeam(seam_u);
	const int c3i = brep.AddEdgeCurve(edge_curve);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[high_mid],
	    brep.m_V[high_mid], c3i);
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(
	    ON_2dPoint(seam_u, high_v),
	    ON_2dPoint(seam_u + udom.Length(), high_v));
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = ON_Surface::x_iso;
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    } else {
	add_half_boundary(high_mid, high_end, high_v,
	    ON_Interval(seam_u, udom.Max()), false,
	    ON_2dPoint(seam_u, high_v),
	    ON_2dPoint(udom.Max(), high_v));
	add_half_boundary(high_end, high_mid, high_v,
	    ON_Interval(udom.Min(), seam_u), false,
	    ON_2dPoint(udom.Max(), high_v),
	    ON_2dPoint(seam_u, high_v));
    }

    ON_LineCurve *down_curve = new ON_LineCurve(
	ON_2dPoint(seam_u, high_v), ON_2dPoint(seam_u, low_v));
    down_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &down = brep.NewTrim(brep.m_E[seam_edge_index], true, loop,
	brep.AddTrimCurve(down_curve));
    down.m_type = ON_BrepTrim::seam;
    down.m_iso = ON_Surface::W_iso;
    down.m_tolerance[0] = down.m_tolerance[1] = 1.0e-6;

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
paired_periodic_strip_test()
{
    return paired_periodic_strip_case(false) &&
	paired_periodic_strip_case(true);
}

static bool
split_periodic_boundary_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    ON_BrepLoop &low = brep.NewLoop(ON_BrepLoop::outer, face);
    add_periodic_trim(brep, low, *surface, vdom.ParameterAt(0.2),
	false, ON_BrepLoop::outer);

    ON_BrepLoop &high = brep.NewLoop(ON_BrepLoop::outer, face);
    const double v = vdom.ParameterAt(0.8);
    const double middle = udom.Mid();
    const int middle_vertex = brep.NewVertex(surface->PointAt(
	middle, v)).m_vertex_index;
    const int seam_vertex = brep.NewVertex(surface->PointAt(
	udom.Min(), v)).m_vertex_index;
    const double excursion = udom.ParameterAt(0.68);
    const int excursion_vertex = brep.NewVertex(surface->PointAt(
	excursion, v)).m_vertex_index;
    auto add_boundary = [&](const ON_Interval &edge_domain,
	    int first_vertex, int second_vertex, const ON_2dPoint &start,
	    const ON_2dPoint &end) {
	ON_Curve *edge_curve = surface->IsoCurve(0, v);
	edge_curve->Trim(edge_domain);
	if (end.x < start.x)
	    edge_curve->Reverse();
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first_vertex],
	    brep.m_V[second_vertex], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, high,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = ON_Surface::x_iso;
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    };
    add_boundary(ON_Interval(middle, udom.Max()), middle_vertex,
	seam_vertex, ON_2dPoint(middle, v), ON_2dPoint(udom.Max(), v));
    add_boundary(ON_Interval(udom.Min(), middle), seam_vertex,
	middle_vertex, ON_2dPoint(udom.Min(), v), ON_2dPoint(middle, v));
    add_boundary(ON_Interval(middle, excursion), middle_vertex,
	excursion_vertex, ON_2dPoint(middle, v),
	ON_2dPoint(excursion, v));
    add_boundary(ON_Interval(middle, excursion), excursion_vertex,
	middle_vertex, ON_2dPoint(excursion, v), ON_2dPoint(middle, v));

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static void
add_surface_iso_trim(ON_Brep &brep, ON_BrepLoop &loop,
	const ON_Surface &surface, int start_vertex, int end_vertex,
	const ON_2dPoint &start, const ON_2dPoint &end,
	double tolerance = 1.0e-6, bool segmented_pcurve = false,
	bool cubic_pcurve = false)
{
    const bool vary_u = fabs(end.x - start.x) > fabs(end.y - start.y);
    const int direction = vary_u ? 0 : 1;
    const double constant = vary_u ? start.y : start.x;
    ON_Curve *edge_curve = surface.IsoCurve(direction, constant);
    const double first = start[direction];
    const double second = end[direction];
    edge_curve->Trim(ON_Interval(std::min(first, second),
	std::max(first, second)));
    if (second < first)
	edge_curve->Reverse();
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[start_vertex],
	brep.m_V[end_vertex], c3i);
    edge.m_tolerance = tolerance;
    ON_Curve *trim_curve = NULL;
    if (segmented_pcurve) {
	std::vector<ON_3dPoint> control_points;
	const int count = cubic_pcurve ? 7 : 3;
	for (int i = 0; i < count; ++i) {
	    const ON_2dPoint point = start +
		(double)i / (double)(count - 1) * (end - start);
	    control_points.push_back(ON_3dPoint(point.x, point.y, 0.0));
	}
	trim_curve = nurbs_curve(2, cubic_pcurve ? 3 : 1,
	    control_points);
    } else {
	trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
    }
    const int c2i = brep.AddTrimCurve(trim_curve);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = surface.IsIsoparametric(*trim_curve);
    trim.m_tolerance[0] = trim.m_tolerance[1] = tolerance;
}

static bool
degenerate_closed_surface_slit_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const double u = surface->Domain(0).Mid();
    const double low_v = surface->Domain(1).ParameterAt(0.2);
    const double high_v = surface->Domain(1).ParameterAt(0.8);
    const ON_2dPoint low(u, low_v);
    const ON_2dPoint high(u, high_v);
    const int low_vertex = brep.NewVertex(
	surface->PointAt(low.x, low.y)).m_vertex_index;
    const int high_vertex = brep.NewVertex(
	surface->PointAt(high.x, high.y)).m_vertex_index;
    add_surface_iso_trim(brep, loop, *surface, low_vertex, high_vertex,
	low, high);
    const double pcurve_tolerance = 1.0e-5;
    const ON_2dPoint perturbed_high(u + 0.5 * pcurve_tolerance, high_v);
    const ON_2dPoint perturbed_low(u + 0.5 * pcurve_tolerance, low_v);
    add_surface_iso_trim(brep, loop, *surface, high_vertex, low_vertex,
	perturbed_high, perturbed_low, pcurve_tolerance, true, true);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.completed_faces == 1 &&
	result->report.failed_faces == 0 &&
	result->report.skipped_degenerate_faces == 1 &&
	result->face_count == 0 && result->point_count == 0;
    delete result;
    return valid;
}

static bool
multiple_empty_loops_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    brep.NewLoop(ON_BrepLoop::outer, face);
    brep.NewLoop(ON_BrepLoop::inner, face);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.completed_faces == 1 &&
	result->report.failed_faces == 0 &&
	result->report.skipped_degenerate_faces == 1 &&
	result->face_count == 0 && result->point_count == 0;
    delete result;
    return valid;
}

static bool
degenerate_inner_loop_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = large_plane();
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(-2.0, -2.0), ON_2dPoint(2.0, -2.0),
	ON_2dPoint(2.0, 2.0), ON_2dPoint(-2.0, 2.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(surface->PointAt(corners[i].x,
	    corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, outer, *surface, vertices[i],
	    vertices[next], corners[i], corners[next]);
    }

    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    const ON_2dPoint low(0.0, -1.0);
    const ON_2dPoint high(0.0, 1.0);
    const int low_vertex = brep.NewVertex(
	surface->PointAt(low.x, low.y)).m_vertex_index;
    const int high_vertex = brep.NewVertex(
	surface->PointAt(high.x, high.y)).m_vertex_index;
    add_surface_iso_trim(brep, inner, *surface, low_vertex, high_vertex,
	low, high);
    add_surface_iso_trim(brep, inner, *surface, high_vertex, low_vertex,
	high, low);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
bridged_inner_loop_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = large_plane();
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_2dPoint outer_corners[4] = {
	ON_2dPoint(-2.0, -2.0), ON_2dPoint(2.0, -2.0),
	ON_2dPoint(2.0, 4.0), ON_2dPoint(-2.0, 4.0)
    };
    int outer_vertices[4];
    for (int i = 0; i < 4; ++i)
	outer_vertices[i] = brep.NewVertex(surface->PointAt(
	    outer_corners[i].x, outer_corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, outer, *surface, outer_vertices[i],
	    outer_vertices[next], outer_corners[i], outer_corners[next]);
    }

    const ON_2dPoint walk[10] = {
	ON_2dPoint(0.0, 0.0), ON_2dPoint(-1.0, 0.0),
	ON_2dPoint(-1.0, -1.0), ON_2dPoint(0.0, -1.0),
	ON_2dPoint(0.0, 0.0), ON_2dPoint(0.0, 2.0),
	ON_2dPoint(-1.0, 2.0), ON_2dPoint(-1.0, 3.0),
	ON_2dPoint(0.0, 3.0), ON_2dPoint(0.0, 2.0)
    };
    int walk_vertices[10];
    for (int i = 0; i < 10; ++i) {
	if (i == 4)
	    walk_vertices[i] = walk_vertices[0];
	else if (i == 9)
	    walk_vertices[i] = walk_vertices[5];
	else
	    walk_vertices[i] = brep.NewVertex(surface->PointAt(
		walk[i].x, walk[i].y)).m_vertex_index;
    }
    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    for (int i = 0; i < 10; ++i) {
	const int next = (i + 1) % 10;
	add_surface_iso_trim(brep, inner, *surface, walk_vertices[i],
	    walk_vertices[next], walk[i], walk[next]);
    }

    fast_result *result = run_fast(brep);
    const double expected_area = 22.0;
    double triangulated_area = 0.0;
    for (int fi = 0; result->faces && result->points &&
	    fi < result->face_count; ++fi) {
	const int ia = result->faces[3 * fi];
	const int ib = result->faces[3 * fi + 1];
	const int ic = result->faces[3 * fi + 2];
	if (ia < 0 || ib < 0 || ic < 0 || ia >= result->point_count ||
		ib >= result->point_count || ic >= result->point_count) {
	    triangulated_area = -1.0;
	    break;
	}
	const double ab_x = result->points[ib][X] - result->points[ia][X];
	const double ab_y = result->points[ib][Y] - result->points[ia][Y];
	const double ac_x = result->points[ic][X] - result->points[ia][X];
	const double ac_y = result->points[ic][Y] - result->points[ia][Y];
	triangulated_area += 0.5 * fabs(ab_x * ac_y - ab_y * ac_x);
    }
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	NEAR_EQUAL(triangulated_area, expected_area, 1.0e-6);
    delete result;
    return valid;
}

static bool
winding_periodic_strip_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double period = udom.Length();
    const double low_v = vdom.ParameterAt(0.25);
    const double high_v = vdom.ParameterAt(0.80);

    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    add_periodic_trim(brep, outer, *surface,
	vdom.ParameterAt(0.10), false, ON_BrepLoop::outer);

    const ON_2dPoint walk[5] = {
	ON_2dPoint(udom.Min(), low_v),
	ON_2dPoint(udom.Min() + 0.75 * period, low_v),
	ON_2dPoint(udom.Min() + 28.75 * period, high_v),
	ON_2dPoint(udom.Min() + 29.0 * period, high_v),
	ON_2dPoint(udom.Min() + period, low_v)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i) {
	double wrapped_u = udom.Min() +
	    fmod(walk[i].x - udom.Min(), period);
	if (wrapped_u < udom.Min())
	    wrapped_u += period;
	vertices[i] = brep.NewVertex(surface->PointAt(wrapped_u,
	    walk[i].y)).m_vertex_index;
    }

    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	const ON_2dPoint start = walk[i];
	const ON_2dPoint end = walk[i + 1];
	const int sample_count = std::max(8,
	    (int)ceil(fabs(end.x - start.x) / period * 64.0));
	ON_3dPointArray edge_points;
	for (int sample = 0; sample <= sample_count; ++sample) {
	    const ON_2dPoint uv = start +
		(double)sample / (double)sample_count * (end - start);
	    double wrapped_u = udom.Min() +
		fmod(uv.x - udom.Min(), period);
	    if (wrapped_u < udom.Min())
		wrapped_u += period;
	    edge_points.Append(surface->PointAt(wrapped_u, uv.y));
	}
	ON_PolylineCurve *edge_curve = new ON_PolylineCurve(edge_points);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[vertices[i]],
	    brep.m_V[vertices[next]], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, inner,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	result->point_count > 512;
    if (!valid)
	bu_log("winding strip: ret=%d failed=%d faces=%d points=%d\n",
	    result->ret, result->report.failed_faces, result->face_count,
	    result->point_count);
    delete result;
    return valid;
}

static bool
periodic_vertex_copy_strip_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 28.0);
    ON_Cylinder cylinder(base, 286.08030853825159);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    surface->SetDomain(0, M_PI, 3.0 * M_PI);
    surface->SetDomain(1, 0.0, 286.08030853825159);
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_2dPoint walk[8] = {
	ON_2dPoint(M_PI, 239.999),
	ON_2dPoint(2.0 * M_PI, 240.0),
	ON_2dPoint(2.5 * M_PI, 239.999),
	ON_2dPoint(3.0 * M_PI, 239.999),
	ON_2dPoint(3.0 * M_PI, 280.0),
	ON_2dPoint(2.0 * M_PI, 280.0),
	ON_2dPoint(M_PI, 280.0),
	ON_2dPoint(M_PI, 239.999)
    };
    int vertices[7];
    vertices[0] = brep.NewVertex(surface->PointAt(
	walk[0].x, walk[0].y)).m_vertex_index;
    vertices[1] = brep.NewVertex(surface->PointAt(
	walk[1].x, walk[1].y)).m_vertex_index;
    vertices[2] = brep.NewVertex(surface->PointAt(
	walk[2].x, walk[2].y)).m_vertex_index;
    vertices[3] = vertices[0];
    vertices[4] = brep.NewVertex(surface->PointAt(
	walk[4].x, walk[4].y)).m_vertex_index;
    vertices[5] = brep.NewVertex(surface->PointAt(
	walk[5].x, walk[5].y)).m_vertex_index;
    vertices[6] = vertices[4];
    for (int i = 0; i < 7; ++i) {
	const int next = (i + 1) % 7;
	add_uv_polyline_trim(brep, loop, *surface, vertices[i],
	    vertices[next], walk[i], walk[i + 1]);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
doubly_periodic_winding_strip_test()
{
    ON_Brep brep;
    ON_Circle major(ON_xy_plane, 9.0);
    ON_Torus torus(major, 2.5);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (!torus.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);

    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const double outer_u = udom.Min();
    const ON_2dPoint outer_walk[3] = {
	ON_2dPoint(outer_u, vdom.Min()),
	ON_2dPoint(outer_u, vdom.Mid()),
	ON_2dPoint(outer_u, vdom.Max())
    };
    const int outer_start = brep.NewVertex(surface->PointAt(
	outer_walk[0].x, outer_walk[0].y)).m_vertex_index;
    const int outer_mid = brep.NewVertex(surface->PointAt(
	outer_walk[1].x, outer_walk[1].y)).m_vertex_index;
    add_uv_polyline_trim(brep, outer, *surface, outer_start, outer_mid,
	outer_walk[0], outer_walk[1]);
    add_uv_polyline_trim(brep, outer, *surface, outer_mid, outer_start,
	outer_walk[1], outer_walk[2]);

    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    const ON_2dPoint inner_walk[3] = {
	ON_2dPoint(udom.Min(), vdom.Max()),
	ON_2dPoint(udom.Max(), vdom.Mid()),
	ON_2dPoint(udom.Min(), vdom.Min())
    };
    const int inner_start = brep.NewVertex(surface->PointAt(
	inner_walk[0].x, inner_walk[0].y)).m_vertex_index;
    const int inner_mid = brep.NewVertex(surface->PointAt(
	inner_walk[1].x, inner_walk[1].y)).m_vertex_index;
    add_uv_polyline_trim(brep, inner, *surface, inner_start, inner_mid,
	inner_walk[0], inner_walk[1]);
    add_uv_polyline_trim(brep, inner, *surface, inner_mid, inner_start,
	inner_walk[1], inner_walk[2]);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
collapsed_closed_pcurve_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = large_plane();
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(-5.0, -5.0), ON_2dPoint(5.0, -5.0),
	ON_2dPoint(5.0, 5.0), ON_2dPoint(-5.0, 5.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(surface->PointAt(corners[i].x,
	    corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, outer, *surface, vertices[i],
	    vertices[next], corners[i], corners[next]);
    }

    ON_BrepLoop &hole = brep.NewLoop(ON_BrepLoop::inner, face);
    ON_Circle circle(ON_xy_plane, 1.0);
    ON_ArcCurve *edge_curve = new ON_ArcCurve(circle);
    const ON_3dPoint edge_start = edge_curve->PointAtStart();
    const int vertex = brep.NewVertex(edge_start).m_vertex_index;
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[vertex],
	brep.m_V[vertex], c3i);
    edge.m_tolerance = 1.0e-6;
    ON_LineCurve *collapsed = new ON_LineCurve(ON_2dPoint(1.0, 0.0),
	ON_2dPoint(1.0 + 1.0e-9, 0.0));
    collapsed->SetDomain(0.0, 1.0);
    ON_BrepTrim &trim = brep.NewTrim(edge, true, hole,
	brep.AddTrimCurve(collapsed));
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = ON_Surface::not_iso;
    trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;

    fast_result *result = run_fast(brep);
    bool center_covered = false;
    for (int fi = 0; result->faces && result->points &&
	    fi < result->face_count; ++fi) {
	const int ia = result->faces[3 * fi];
	const int ib = result->faces[3 * fi + 1];
	const int ic = result->faces[3 * fi + 2];
	if (ia < 0 || ib < 0 || ic < 0 || ia >= result->point_count ||
		ib >= result->point_count || ic >= result->point_count) {
	    center_covered = true;
	    break;
	}
	auto cross = [](const point_t a, const point_t b) {
	    return a[X] * b[Y] - a[Y] * b[X];
	};
	const double ab = cross(result->points[ia], result->points[ib]);
	const double bc = cross(result->points[ib], result->points[ic]);
	const double ca = cross(result->points[ic], result->points[ia]);
	const bool negative = ab < -1.0e-9 || bc < -1.0e-9 ||
	    ca < -1.0e-9;
	const bool positive = ab > 1.0e-9 || bc > 1.0e-9 ||
	    ca > 1.0e-9;
	center_covered = !(negative && positive);
	if (center_covered)
	    break;
    }
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	!center_covered;
    delete result;
    return valid;
}

static bool
misclassified_periodic_boundaries_case(bool declare_cutout_outer)
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);

    ON_BrepLoop &cutout = brep.NewLoop(declare_cutout_outer ?
	ON_BrepLoop::outer : ON_BrepLoop::inner, face);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(udom.ParameterAt(0.30), vdom.ParameterAt(0.40)),
	ON_2dPoint(udom.ParameterAt(0.30), vdom.ParameterAt(0.60)),
	ON_2dPoint(udom.ParameterAt(0.40), vdom.ParameterAt(0.60)),
	ON_2dPoint(udom.ParameterAt(0.40), vdom.ParameterAt(0.40))
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(surface->PointAt(corners[i].x,
	    corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, cutout, *surface, vertices[i],
	    vertices[next], corners[i], corners[next]);
    }

    ON_BrepLoop &lower = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, lower, *surface, vdom.ParameterAt(0.20),
	false, ON_BrepLoop::inner);
    ON_BrepLoop &upper = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, upper, *surface, vdom.ParameterAt(0.80),
	false, ON_BrepLoop::inner);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
misclassified_periodic_boundaries_test()
{
    return misclassified_periodic_boundaries_case(true) &&
	misclassified_periodic_boundaries_case(false);
}

static bool
touching_periodic_subloops_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double u0 = udom.ParameterAt(0.20);
    const double u1 = udom.ParameterAt(0.80);
    const double v0 = vdom.ParameterAt(0.20);
    const double v1 = vdom.ParameterAt(0.80);
    const ON_2dPoint shared(u0, v0);
    const ON_2dPoint hole_a(udom.ParameterAt(0.32),
	vdom.ParameterAt(0.44));
    const ON_2dPoint hole_b(udom.ParameterAt(0.44),
	vdom.ParameterAt(0.32));
    const ON_2dPoint outer_b(u1, v0);
    const ON_2dPoint outer_c(u1, v1);
    const ON_2dPoint outer_d(u0, v1);
    const ON_2dPoint parameters[7] = {
	shared, hole_a, hole_b, shared,
	outer_b, outer_c, outer_d
    };
    int vertices[7];
    vertices[0] = brep.NewVertex(surface->PointAt(shared.x,
	shared.y)).m_vertex_index;
    vertices[1] = brep.NewVertex(surface->PointAt(hole_a.x,
	hole_a.y)).m_vertex_index;
    vertices[2] = brep.NewVertex(surface->PointAt(hole_b.x,
	hole_b.y)).m_vertex_index;
    vertices[3] = vertices[0];
    vertices[4] = brep.NewVertex(surface->PointAt(outer_b.x,
	outer_b.y)).m_vertex_index;
    vertices[5] = brep.NewVertex(surface->PointAt(outer_c.x,
	outer_c.y)).m_vertex_index;
    vertices[6] = brep.NewVertex(surface->PointAt(outer_d.x,
	outer_d.y)).m_vertex_index;

    auto add_line_trim = [&](int first, int second,
	    const ON_2dPoint &start, const ON_2dPoint &end) {
	ON_LineCurve *edge_curve = new ON_LineCurve(
	    surface->PointAt(start.x, start.y),
	    surface->PointAt(end.x, end.y));
	edge_curve->SetDomain(0.0, 1.0);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[first],
	    brep.m_V[second], brep.AddEdgeCurve(edge_curve));
	edge.m_tolerance = 1.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
	trim_curve->SetDomain(0.0, 1.0);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop,
	    brep.AddTrimCurve(trim_curve));
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
    };
    for (int i = 0; i < 7; ++i) {
	const int next = (i + 1) % 7;
	add_line_trim(vertices[i], vertices[next], parameters[i],
	    parameters[next]);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
inner_only_planar_loop_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = large_plane();
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::inner, face);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(-2.0, -1.0), ON_2dPoint(2.0, -1.0),
	ON_2dPoint(2.0, 1.0), ON_2dPoint(-2.0, 1.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(surface->PointAt(corners[i].x,
	    corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, loop, *surface, vertices[i],
	    vertices[next], corners[i], corners[next]);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
empty_periodic_boundary_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &boundary = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, boundary, *surface,
	surface->Domain(1).Max(), false, ON_BrepLoop::inner);
    brep.NewLoop(ON_BrepLoop::inner, face);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
full_periodic_hole_test()
{
    ON_Brep brep;
    ON_Circle base(ON_xy_plane, 1.0);
    ON_Cylinder cylinder(base, 1.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double seam_u = udom.Mid();
    const ON_2dPoint seam_low(seam_u, vdom.Min());
    const ON_2dPoint seam_high(seam_u, vdom.Max());
    const int low_vertex = brep.NewVertex(surface->PointAt(
	seam_low.x, seam_low.y)).m_vertex_index;
    const int high_vertex = brep.NewVertex(surface->PointAt(
	seam_high.x, seam_high.y)).m_vertex_index;
    ON_Curve *seam_curve = surface->IsoCurve(1, seam_u);
    const int seam_c3i = brep.AddEdgeCurve(seam_curve);
    ON_BrepEdge &seam_edge = brep.NewEdge(brep.m_V[low_vertex],
	brep.m_V[high_vertex], seam_c3i);
    seam_edge.m_tolerance = 1.0e-6;
    ON_LineCurve *up_curve = new ON_LineCurve(seam_low, seam_high);
    up_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &up = brep.NewTrim(seam_edge, false, outer,
	brep.AddTrimCurve(up_curve));
    up.m_type = ON_BrepTrim::seam;
    up.m_iso = ON_Surface::W_iso;
    up.m_tolerance[0] = up.m_tolerance[1] = 1.0e-6;
    ON_LineCurve *down_curve = new ON_LineCurve(seam_high, seam_low);
    down_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &down = brep.NewTrim(seam_edge, true, outer,
	brep.AddTrimCurve(down_curve));
    down.m_type = ON_BrepTrim::seam;
    down.m_iso = ON_Surface::W_iso;
    down.m_tolerance[0] = down.m_tolerance[1] = 1.0e-6;

    ON_BrepLoop &hole = brep.NewLoop(ON_BrepLoop::inner, face);
    const double u0 = udom.ParameterAt(0.65);
    const double u1 = udom.ParameterAt(0.80);
    const double v0 = vdom.ParameterAt(0.35);
    const double v1 = vdom.ParameterAt(0.65);
    const ON_2dPoint corners[4] = {
	ON_2dPoint(u0, v0), ON_2dPoint(u0, v1),
	ON_2dPoint(u1, v1), ON_2dPoint(u1, v0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(surface->PointAt(corners[i].x,
	    corners[i].y)).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_surface_iso_trim(brep, hole, *surface, vertices[i],
	    vertices[next], corners[i], corners[next]);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
periodic_singular_domain_test()
{
    ON_Brep brep;
    ON_Sphere sphere(ON_3dPoint::Origin, 1.0);
    ON_RevSurface *surface = sphere.RevSurfaceForm(false);
    if (!surface)
	return false;
    const ON_Interval original_vdom = surface->Domain(1);
    if (!surface->Trim(1, ON_Interval(original_vdom.Mid(),
	    original_vdom.Max()))) {
	delete surface;
	return false;
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const ON_2dPoint seam_low(udom.Min(), vdom.Min());
    const ON_2dPoint seam_high(udom.Min(), vdom.Max());
    const int low_vertex = brep.NewVertex(surface->PointAt(
	seam_low.x, seam_low.y)).m_vertex_index;
    const int pole_vertex = brep.NewVertex(surface->PointAt(
	seam_high.x, seam_high.y)).m_vertex_index;
    ON_Curve *seam_curve = surface->IsoCurve(1, udom.Min());
    const int seam_c3i = brep.AddEdgeCurve(seam_curve);
    ON_BrepEdge &seam_edge = brep.NewEdge(brep.m_V[low_vertex],
	brep.m_V[pole_vertex], seam_c3i);
    seam_edge.m_tolerance = 1.0e-6;

    ON_LineCurve *up_curve = new ON_LineCurve(seam_low, seam_high);
    up_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &up = brep.NewTrim(seam_edge, false, loop,
	brep.AddTrimCurve(up_curve));
    up.m_type = ON_BrepTrim::seam;
    up.m_iso = ON_Surface::W_iso;
    up.m_tolerance[0] = up.m_tolerance[1] = 1.0e-6;

    ON_LineCurve *pole_curve = new ON_LineCurve(
	ON_2dPoint(udom.Min(), vdom.Max()),
	ON_2dPoint(udom.Max(), vdom.Max()));
    pole_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &pole = brep.NewSingularTrim(brep.m_V[pole_vertex], loop,
	ON_Surface::N_iso, brep.AddTrimCurve(pole_curve));
    pole.m_tolerance[0] = pole.m_tolerance[1] = 1.0e-6;

    ON_LineCurve *down_curve = new ON_LineCurve(
	ON_2dPoint(udom.Max(), vdom.Max()),
	ON_2dPoint(udom.Max(), vdom.Min()));
    down_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &down = brep.NewTrim(seam_edge, true, loop,
	brep.AddTrimCurve(down_curve));
    down.m_type = ON_BrepTrim::seam;
    down.m_iso = ON_Surface::E_iso;
    down.m_tolerance[0] = down.m_tolerance[1] = 1.0e-6;

    ON_Curve *bottom_edge_curve = surface->IsoCurve(0, vdom.Min());
    const int bottom_c3i = brep.AddEdgeCurve(bottom_edge_curve);
    ON_BrepEdge &bottom_edge = brep.NewEdge(brep.m_V[low_vertex],
	brep.m_V[low_vertex], bottom_c3i);
    bottom_edge.m_tolerance = 1.0e-6;
    ON_LineCurve *bottom_curve = new ON_LineCurve(
	ON_2dPoint(udom.Max(), vdom.Min()),
	ON_2dPoint(udom.Min(), vdom.Min()));
    bottom_curve->SetDomain(0.0, 1.0);
    ON_BrepTrim &bottom = brep.NewTrim(bottom_edge, true, loop,
	brep.AddTrimCurve(bottom_curve));
    bottom.m_type = ON_BrepTrim::boundary;
    bottom.m_iso = ON_Surface::S_iso;
    bottom.m_tolerance[0] = bottom.m_tolerance[1] = 1.0e-6;

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
full_periodic_face_test()
{
    ON_Brep brep;
    ON_Sphere sphere(ON_3dPoint::Origin, 1.0);
    ON_RevSurface *surface = sphere.RevSurfaceForm(false);
    if (!surface)
	return false;
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double seam_u = udom.Mid();
    const ON_3dPoint north = surface->PointAt(seam_u, vdom.Max());
    const ON_3dPoint south = surface->PointAt(seam_u, vdom.Min());
    const int north_vertex = brep.NewVertex(north).m_vertex_index;
    const int south_vertex = brep.NewVertex(south).m_vertex_index;
    ON_LineCurve *edge_curve = new ON_LineCurve(north, south);
    edge_curve->SetDomain(0.0, 1.0);
    const int c3i = brep.AddEdgeCurve(edge_curve);
    ON_BrepEdge &edge = brep.NewEdge(brep.m_V[north_vertex],
	brep.m_V[south_vertex], c3i);

    const ON_2dPoint north_uv(seam_u, vdom.Max());
    const ON_2dPoint south_uv(seam_u, vdom.Min());
    ON_LineCurve *down_curve = new ON_LineCurve(north_uv, south_uv);
    down_curve->SetDomain(0.0, 1.0);
    const int down_c2i = brep.AddTrimCurve(down_curve);
    ON_BrepTrim &down = brep.NewTrim(edge, false, loop, down_c2i);
    down.m_type = ON_BrepTrim::seam;
    down.m_iso = ON_Surface::W_iso;
    down.m_tolerance[0] = down.m_tolerance[1] = 1.0e-6;

    ON_LineCurve *up_curve = new ON_LineCurve(south_uv, north_uv);
    up_curve->SetDomain(0.0, 1.0);
    const int up_c2i = brep.AddTrimCurve(up_curve);
    ON_BrepTrim &up = brep.NewTrim(edge, true, loop, up_c2i);
    up.m_type = ON_BrepTrim::seam;
    up.m_iso = ON_Surface::W_iso;
    up.m_tolerance[0] = up.m_tolerance[1] = 1.0e-6;

    fast_result *result = run_fast(brep);
    bool found_north = false;
    bool found_south = false;
    for (int i = 0; result->points && i < result->point_count; ++i) {
	const ON_3dPoint point(result->points[i][X], result->points[i][Y],
	    result->points[i][Z]);
	found_north = found_north || point.DistanceTo(north) < 1.0e-6;
	found_south = found_south || point.DistanceTo(south) < 1.0e-6;
    }
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	found_north && found_south;
    delete result;
    return valid;
}

static bool
untrimmed_planar_face_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    brep.NewLoop(ON_BrepLoop::outer, face);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count == 2 &&
	result->point_count == 4;
    delete result;
    return valid;
}

static bool
untrimmed_curved_surface_test()
{
    ON_Brep brep;
    ON_NurbsSurface *surface = new ON_NurbsSurface(
	3, false, 3, 3, 3, 3);
    if (!surface->MakeClampedUniformKnotVector(0) ||
	    !surface->MakeClampedUniformKnotVector(1)) {
	delete surface;
	return false;
    }
    for (int u = 0; u < 3; ++u) {
	for (int v = 0; v < 3; ++v) {
	    const double height = u == 1 && v == 1 ? 0.5 : 0.0;
	    surface->SetCV(u, v, ON_3dPoint(u, v, height));
	}
    }
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    brep.NewLoop(ON_BrepLoop::outer, face);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	result->point_count > 0;
    delete result;
    return valid;
}

static bool
skinny_planar_strip_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, 0.0, 0.01);
    surface->SetDomain(1, 0.0, 1000.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_3dPoint corners[4] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(0.01, 0.0, 0.0),
	ON_3dPoint(0.01, 1000.0, 0.0), ON_3dPoint(0.0, 1000.0, 0.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(corners[i]).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_nurbs_trim(brep, loop, vertices[i], vertices[next], {
	    corners[i], corners[next]
	}, 1.0e-6);
    }

    fast_result *result = run_fast(brep);
    point_t bmin = {INFINITY, INFINITY, INFINITY};
    point_t bmax = {-INFINITY, -INFINITY, -INFINITY};
    for (int i = 0; result->points && i < result->point_count; ++i)
	VMINMAX(bmin, bmax, result->points[i]);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0 &&
	result->face_count <= 128 && result->point_count <= 128 &&
	NEAR_EQUAL(bmin[X], 0.0, 1.0e-9) &&
	NEAR_EQUAL(bmax[X], 0.01, 1.0e-9) &&
	NEAR_EQUAL(bmin[Y], 0.0, 1.0e-9) &&
	NEAR_EQUAL(bmax[Y], 1000.0, 1.0e-9);
    if (!valid)
	bu_log("skinny strip: ret=%d faces=%d points=%d "
	    "bbox=(%.17g %.17g)-(%.17g %.17g)\n", result->ret,
	    result->face_count, result->point_count, bmin[X], bmin[Y],
	    bmax[X], bmax[Y]);
    delete result;
    return valid;
}

static bool
tolerant_narrow_planar_strip_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = large_plane();
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const double width = 1.0e-3;
    const double trim_tolerance = 1.0e-2;
    const ON_3dPoint corners[4] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 0.0, 0.0),
	ON_3dPoint(2.0, width, 0.0), ON_3dPoint(0.0, width, 0.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(corners[i]).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	add_nurbs_trim(brep, loop, vertices[i], vertices[next], {
	    corners[i], corners[next]
	}, trim_tolerance);
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 &&
	result->report.skipped_degenerate_faces == 0 &&
	result->face_count > 0 && result->point_count > 0;
    delete result;
    return valid;
}

static bool
near_closed_planar_loop_test()
{
    ON_Brep brep;
    ON_PlaneSurface *surface = new ON_PlaneSurface(ON_xy_plane);
    surface->SetDomain(0, 0.0, 1.0);
    surface->SetDomain(1, 0.0, 1.0);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int si = brep.AddSurface(surface);
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_3dPoint corners[4] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(corners[i]).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	ON_LineCurve *edge_curve = new ON_LineCurve(corners[i],
	    corners[next]);
	edge_curve->SetDomain(0.0, 1.0);
	const int c3i = brep.AddEdgeCurve(edge_curve);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[vertices[i]],
	    brep.m_V[vertices[next]], c3i);
	edge.m_tolerance = 1.0e-4;
	ON_2dPoint trim_start(corners[i].x, corners[i].y);
	ON_2dPoint trim_end(corners[next].x, corners[next].y);
	if (i == 3)
	    trim_end.y = 2.0e-6;
	ON_LineCurve *trim_curve = new ON_LineCurve(trim_start, trim_end);
	trim_curve->SetDomain(0.0, 1.0);
	const int c2i = brep.AddTrimCurve(trim_curve);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = surface->IsIsoparametric(*trim_curve);
	trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-4;
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
malformed_pcurve_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_3dPoint corners[4] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0)
    };
    int vertices[4];
    for (int i = 0; i < 4; ++i)
	vertices[i] = brep.NewVertex(corners[i]).m_vertex_index;
    for (int i = 0; i < 4; ++i) {
	const int next = (i + 1) % 4;
	ON_LineCurve *edge_curve = new ON_LineCurve(corners[i], corners[next]);
	edge_curve->SetDomain(0.0, 1.0);
	const int c3i = brep.AddEdgeCurve(edge_curve);
	ON_BrepEdge &edge = brep.NewEdge(brep.m_V[vertices[i]],
	    brep.m_V[vertices[next]], c3i);
	ON_Curve *trim_curve = NULL;
	if (i == 2) {
	    trim_curve = nurbs_curve(2, 1, {
		ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 0.0, 0.0),
		ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0)
	    });
	} else {
	    trim_curve = new ON_LineCurve(ON_2dPoint(corners[i].x,
		corners[i].y), ON_2dPoint(corners[next].x, corners[next].y));
	    trim_curve->SetDomain(0.0, 1.0);
	}
	const int c2i = brep.AddTrimCurve(trim_curve);
	ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_iso = ON_Surface::not_iso;
	trim.m_tolerance[0] = i == 2 ? 1.0 : 1.0e-6;
	trim.m_tolerance[1] = trim.m_tolerance[0];
    }

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

struct source_sample {
    fastf_t parameter;
    fastf_t uv[2];
    fastf_t point[3];
    int identity;
};

struct source_store {
    std::map<int, std::vector<source_sample>> trims;
    std::set<const void *> output;
};

static size_t
source_sample_count(int UNUSED(face_index), int trim_index, void *data)
{
    source_store *store = (source_store *)data;
    const auto trim = store->trims.find(trim_index);
    return trim == store->trims.end() ? 0 : trim->second.size();
}

static int
source_sample_get(int UNUSED(face_index), int trim_index,
	size_t sample_index, fastf_t *parameter, point2d_t uv, point_t point,
	void *data)
{
    source_store *store = (source_store *)data;
    const auto trim = store->trims.find(trim_index);
    if (trim == store->trims.end() || sample_index >= trim->second.size())
	return -1;
    const source_sample &sample = trim->second[sample_index];
    *parameter = sample.parameter;
    V2MOVE(uv, sample.uv);
    VMOVE(point, sample.point);
    return 0;
}

static const void *
source_sample_identity(int UNUSED(face_index), int trim_index,
	size_t sample_index, void *data)
{
    source_store *store = (source_store *)data;
    const auto trim = store->trims.find(trim_index);
    if (trim == store->trims.end() || sample_index >= trim->second.size())
	return NULL;
    return &trim->second[sample_index].identity;
}

static void
source_point_output(int UNUSED(face_index), size_t UNUSED(point_index),
	const void *source, void *data)
{
    source_store *store = (source_store *)data;
    if (source)
	store->output.insert(source);
}

static bool
constrained_source_identity_test()
{
    ON_Brep brep;
    const int si = brep.AddSurface(large_plane());
    ON_BrepFace &face = brep.NewFace(si);
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const ON_3dPoint corners[4] = {
	ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 1.0, 0.0), ON_3dPoint(0.0, 1.0, 0.0)
    };
    int vertices[4];
    for (int corner = 0; corner < 4; ++corner)
	vertices[corner] = brep.NewVertex(corners[corner]).m_vertex_index;

    source_store store;
    for (int corner = 0; corner < 4; ++corner) {
	const int next = (corner + 1) % 4;
	ON_BrepTrim &trim = add_nurbs_trim(brep, loop, vertices[corner],
	    vertices[next], {corners[corner], corners[next]}, 1.0e-6);
	std::vector<source_sample> &samples = store.trims[trim.m_trim_index];
	samples.resize(3);
	for (int sample_index = 0; sample_index < 3; ++sample_index) {
	    const double fraction = 0.5 * sample_index;
	    const ON_3dPoint point = (1.0 - fraction) * corners[corner] +
		fraction * corners[next];
	    source_sample &sample = samples[(size_t)sample_index];
	    sample.parameter = trim.Domain().ParameterAt(fraction);
	    V2SET(sample.uv, point.x, point.y);
	    VSET(sample.point, point.x, point.y, point.z);
	    sample.identity = corner * 3 + sample_index;
	}
    }

    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    options.max_workers = 1;
    options.trim_sample_count = source_sample_count;
    options.trim_sample = source_sample_get;
    options.trim_sample_data = &store;
    options.trim_sample_source = source_sample_identity;
    options.point_source = source_point_output;
    options.point_source_data = &store;
    int *faces = NULL;
    int face_count = 0;
    vect_t *normals = NULL;
    point_t *points = NULL;
    int point_count = 0;
    struct brep_cdt_fast_report report = {};
    const int result = brep_cdt_fast_ex(&faces, &face_count, &normals,
	&points, &point_count, &brep, -1, &ttol, &tol, &options, &report);
    bool valid = result == BREP_CDT_FAST_OK && face_count > 0 &&
	report.failed_faces == 0;
    for (const auto &trim : store.trims) {
	const void *middle = &trim.second[1].identity;
	valid = valid && store.output.find(middle) != store.output.end();
    }
    bu_free(faces, "source identity faces");
    bu_free(normals, "source identity normals");
    bu_free(points, "source identity points");
    return valid;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 2;

#define RUN_FAST_TEST(_test) \
    do { \
	if (!(_test)()) { \
	    bu_log("FAIL %s\n", #_test); \
	    return 1; \
	} \
    } while (0)
    RUN_FAST_TEST(thin_lens_test);
    RUN_FAST_TEST(degenerate_line_test);
    RUN_FAST_TEST(degenerate_collinear_loop_test);
    RUN_FAST_TEST(singular_cap_test);
    RUN_FAST_TEST(periodic_strip_test);
    RUN_FAST_TEST(redundant_periodic_boundaries_test);
    RUN_FAST_TEST(periodic_rectangle_test);
    RUN_FAST_TEST(periodic_zero_area_subcycle_test);
    RUN_FAST_TEST(overlapping_periodic_pcurves_test);
    RUN_FAST_TEST(paired_periodic_strip_test);
    RUN_FAST_TEST(split_periodic_boundary_test);
    RUN_FAST_TEST(degenerate_closed_surface_slit_test);
    RUN_FAST_TEST(multiple_empty_loops_test);
    RUN_FAST_TEST(degenerate_inner_loop_test);
    RUN_FAST_TEST(bridged_inner_loop_test);
    RUN_FAST_TEST(winding_periodic_strip_test);
    RUN_FAST_TEST(periodic_vertex_copy_strip_test);
    RUN_FAST_TEST(doubly_periodic_winding_strip_test);
    RUN_FAST_TEST(collapsed_closed_pcurve_test);
    RUN_FAST_TEST(misclassified_periodic_boundaries_test);
    RUN_FAST_TEST(touching_periodic_subloops_test);
    RUN_FAST_TEST(inner_only_planar_loop_test);
    RUN_FAST_TEST(empty_periodic_boundary_test);
    RUN_FAST_TEST(full_periodic_hole_test);
    RUN_FAST_TEST(periodic_singular_domain_test);
    RUN_FAST_TEST(full_periodic_face_test);
    RUN_FAST_TEST(untrimmed_planar_face_test);
    RUN_FAST_TEST(untrimmed_curved_surface_test);
    RUN_FAST_TEST(skinny_planar_strip_test);
    RUN_FAST_TEST(tolerant_narrow_planar_strip_test);
    RUN_FAST_TEST(near_closed_planar_loop_test);
    RUN_FAST_TEST(malformed_pcurve_test);
    RUN_FAST_TEST(constrained_source_identity_test);
#undef RUN_FAST_TEST
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
