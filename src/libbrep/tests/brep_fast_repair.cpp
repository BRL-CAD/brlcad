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
	ON_BrepLoop::TYPE loop_type, bool collapsed_parameter = false)
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
periodic_strip_case(bool collapsed_inner)
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
    ON_BrepLoop &inner = brep.NewLoop(ON_BrepLoop::inner, face);
    add_periodic_trim(brep, inner, *surface, surface->Domain(1).Max(),
	false, ON_BrepLoop::inner, collapsed_inner);

    fast_result *result = run_fast(brep);
    const bool valid = result->ret == BREP_CDT_FAST_OK &&
	result->report.failed_faces == 0 && result->face_count > 0;
    delete result;
    return valid;
}

static bool
periodic_strip_test()
{
    return periodic_strip_case(false) && periodic_strip_case(true);
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
    ON_BrepTrim &down = brep.NewTrim(seam_edge, true, loop,
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

static void
add_surface_iso_trim(ON_Brep &brep, ON_BrepLoop &loop,
	const ON_Surface &surface, int start_vertex, int end_vertex,
	const ON_2dPoint &start, const ON_2dPoint &end)
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
    edge.m_tolerance = 1.0e-6;
    ON_LineCurve *trim_curve = new ON_LineCurve(start, end);
    trim_curve->SetDomain(0.0, 1.0);
    const int c2i = brep.AddTrimCurve(trim_curve);
    ON_BrepTrim &trim = brep.NewTrim(edge, false, loop, c2i);
    trim.m_type = ON_BrepTrim::boundary;
    trim.m_iso = surface.IsIsoparametric(*trim_curve);
    trim.m_tolerance[0] = trim.m_tolerance[1] = 1.0e-6;
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
misclassified_periodic_boundaries_test()
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

    ON_BrepLoop &outer = brep.NewLoop(ON_BrepLoop::outer, face);
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
	add_surface_iso_trim(brep, outer, *surface, vertices[i],
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

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 2;
    return thin_lens_test() && degenerate_line_test() &&
	degenerate_collinear_loop_test() &&
	singular_cap_test() && periodic_strip_test() &&
	paired_periodic_strip_test() &&
	collapsed_closed_pcurve_test() &&
	misclassified_periodic_boundaries_test() &&
	full_periodic_hole_test() && periodic_singular_domain_test() &&
	full_periodic_face_test() && untrimmed_planar_face_test() &&
	skinny_planar_strip_test() && near_closed_planar_loop_test() &&
	malformed_pcurve_test() ? 0 : 1;
}
