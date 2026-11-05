/*                  A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbrep/assembly.cpp */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "brep.h"


namespace {

/* A failed proof must leave edges separate, including when refinement is
 * too expensive.  Divide the error budget between both approximations
 * and the comparison of their polygonal paths. */
constexpr size_t MAX_CURVE_SUBDIVISIONS = 65536;
constexpr unsigned int MAX_CURVE_SUBDIVISION_DEPTH = 48;
constexpr double CURVE_APPROXIMATION_FRACTION = 0.125;
/* A tolerance comparable to a boundary's size cannot distinguish a duplicate
 * edge from a neighboring small feature.  In that regime stitching must leave
 * the topology unresolved instead of accepting a weak coincidence proof. */
constexpr double MAX_RELATIVE_STITCH_DEVIATION = 0.01;
constexpr double MIN_STITCH_MATCH_SEPARATION_RATIO = 4.0;
constexpr double MAX_RELATIVE_TRIM_GAP = 1.0e-4;
constexpr size_t MAX_POINT_CURVE_SUBDIVISIONS = 4096;
constexpr size_t MAX_PAIRED_CURVE_SUBDIVISIONS = 4096;
constexpr size_t MAX_BALL_CURVE_SUBDIVISIONS = 4096;

int
next_curve_span(const ON_NurbsCurve &curve, int span)
{
    while (span <= curve.CVCount() - curve.Order() &&
	!(curve.Knot(span + curve.Order() - 2) < curve.Knot(span + curve.Order() - 1)))
	++span;
    return span;
}

template <typename Curve>
bool
matching_basis_controls(const Curve &first, const Curve &second,
    double tolerance)
{
    if (first.Order() != second.Order() || first.CVCount() != second.CVCount())
	return false;

    ON_3dPoint origin;
    if (!first.GetCV(0, origin) || !origin.IsValid())
	return false;
    double radius = 0.0;
    double deviation = 0.0;
    double minimum_ratio = ON_DBL_MAX;
    double maximum_ratio = 0.0;
    for (int i = 0; i < first.CVCount(); ++i) {
	ON_3dPoint a, b;
	const double ratio = second.Weight(i) / first.Weight(i);
	if (!(first.Weight(i) > 0.0) || !(second.Weight(i) > 0.0) ||
	    !(ratio > 0.0) || !std::isfinite(ratio) ||
	    !first.GetCV(i, a) || !second.GetCV(i, b) ||
	    !a.IsValid() || !b.IsValid() || a.DistanceTo(b) > tolerance)
	    return false;
	minimum_ratio = std::min(minimum_ratio, ratio);
	maximum_ratio = std::max(maximum_ratio, ratio);
	radius = std::max(radius, origin.DistanceTo(a));
	deviation = std::max(deviation, a.DistanceTo(b));
    }
    /* Identical nonnegative bases bound control-point displacement.  A
     * change of rational weights contributes at most this additional
     * displacement about the first control point. */
    const double weight_error = radius * ((maximum_ratio - minimum_ratio) / minimum_ratio);
    return std::isfinite(weight_error) && deviation + weight_error <= tolerance;
}

bool
matching_bezier_pair(const ON_BezierCurve &first, const ON_BezierCurve &second,
    double tolerance, unsigned int depth, size_t &subdivisions)
{
    if (matching_basis_controls(first, second, tolerance))
	return true;
    /* Subdivision tightens a loose control-polygon bound without replacing
     * either curved span with line segments.  Unequal paired points only
     * reject this parameter correspondence, not the curves' loci. */
    for (double parameter : {0.0, 0.5, 1.0})
	if (first.PointAt(parameter).DistanceTo(second.PointAt(parameter)) > tolerance)
	    return false;
    if (depth >= MAX_CURVE_SUBDIVISION_DEPTH || ++subdivisions > MAX_PAIRED_CURVE_SUBDIVISIONS)
	return false;
    ON_BezierCurve first_left, first_right, second_left, second_right;
    return first.Split(0.5, first_left, first_right) && second.Split(0.5, second_left, second_right) &&
	matching_bezier_pair(first_left, second_left, tolerance, depth + 1, subdivisions) &&
	matching_bezier_pair(first_right, second_right, tolerance, depth + 1, subdivisions);
}

bool
matching_bezier_controls(ON_BezierCurve &first, ON_BezierCurve &second, double tolerance)
{
    if (matching_basis_controls(first, second, tolerance))
	return true;
    /* Positive rational end weights can encode different speeds along
     * the same span.  Normalize them without changing the curve's locus. */
    const int degree = std::max(first.Degree(), second.Degree());
    if (!first.IncreaseDegree(degree) || !second.IncreaseDegree(degree))
	return false;
    size_t subdivisions = 0;
    if (matching_bezier_pair(first, second, tolerance, 0, subdivisions))
	return true;
    subdivisions = 0;
    return first.ChangeWeights(0, 1.0, degree, 1.0) && second.ChangeWeights(0, 1.0, degree, 1.0) &&
	matching_bezier_pair(first, second, tolerance, 0, subdivisions);
}

bool
matching_bezier_spans(const ON_NurbsCurve &first, const ON_NurbsCurve &second,
    double tolerance)
{
    int a = next_curve_span(first, 0);
    int b = next_curve_span(second, 0);
    while (true) {
	const bool first_done = a > first.CVCount() - first.Order();
	const bool second_done = b > second.CVCount() - second.Order();
	if (first_done || second_done)
	    return first_done && second_done;
	ON_BezierCurve first_span, second_span;
	/* Corresponding spans may have different parameter intervals.  Their
	 * Bezier bases agree on [0,1], so the proof does not require equal
	 * knot spacing or bit-identical domain normalization. */
	if (!first.ConvertSpanToBezier(a, first_span) ||
	    !second.ConvertSpanToBezier(b, second_span) ||
	    !matching_bezier_controls(first_span, second_span, tolerance))
	    return false;
	a = next_curve_span(first, a + 1);
	b = next_curve_span(second, b + 1);
    }
}

bool
matching_refined_spans(const ON_NurbsCurve &first, const ON_NurbsCurve &second,
    double tolerance)
{
    int a = next_curve_span(first, 0);
    int b = next_curve_span(second, 0);
    while (a <= first.CVCount() - first.Order() && b <= second.CVCount() - second.Order()) {
	const ON_Interval first_domain(first.Knot(a + first.Order() - 2), first.Knot(a + first.Order() - 1));
	const ON_Interval second_domain(second.Knot(b + second.Order() - 2), second.Knot(b + second.Order() - 1));
	const double start = std::max(first_domain.Min(), second_domain.Min());
	const double end = std::min(first_domain.Max(), second_domain.Max());
	ON_BezierCurve first_span, second_span;
	/* Refinement aligns equivalent curves with different knot insertion
	 * or degree elevation histories without flattening their curvature. */
	if (!(start < end) || !first.ConvertSpanToBezier(a, first_span) ||
	    !second.ConvertSpanToBezier(b, second_span) ||
	    !first_span.Trim(ON_Interval(first_domain.NormalizedParameterAt(start), first_domain.NormalizedParameterAt(end))) ||
	    !second_span.Trim(ON_Interval(second_domain.NormalizedParameterAt(start), second_domain.NormalizedParameterAt(end))))
	    return false;
	if (!matching_bezier_controls(first_span, second_span, tolerance))
	    return false;
	if (first_domain.Max() <= second_domain.Max())
	    a = next_curve_span(first, a + 1);
	if (second_domain.Max() <= first_domain.Max())
	    b = next_curve_span(second, b + 1);
    }
    return a > first.CVCount() - first.Order() && b > second.CVCount() - second.Order();
}

bool
append_curve_polygon(const ON_BezierCurve &curve, double tolerance,
    unsigned int depth, size_t &subdivisions, std::vector<ON_3dPoint> &points)
{
    const ON_3dPoint start = curve.PointAt(0.0);
    const ON_3dPoint end = curve.PointAt(1.0);
    if (!start.IsValid() || !end.IsValid())
	return false;
    const ON_Line chord(start, end);
    bool flat = true;
    for (int i = 0; i < curve.CVCount(); ++i) {
	ON_3dPoint point;
	if (!(curve.Weight(i) > 0.0) || !std::isfinite(curve.Weight(i)) ||
	    !curve.GetCV(i, point) || !point.IsValid())
	    return false;
	double parameter = 0.0;
	chord.ClosestPointTo(point, &parameter);
	parameter = std::max(0.0, std::min(1.0, parameter));
	flat = flat && point.DistanceTo(chord.PointAt(parameter)) <= tolerance;
    }
    if (flat) {
	points.push_back(end);
	return true;
    }
    if (depth >= MAX_CURVE_SUBDIVISION_DEPTH || ++subdivisions > MAX_CURVE_SUBDIVISIONS)
	return false;
    ON_BezierCurve left, right;
    return curve.Split(0.5, left, right) &&
	append_curve_polygon(left, tolerance, depth + 1, subdivisions, points) &&
	append_curve_polygon(right, tolerance, depth + 1, subdivisions, points);
}

bool
curve_polygon(const ON_NurbsCurve &curve, double tolerance,
    std::vector<ON_3dPoint> &points)
{
    size_t subdivisions = 0;
    points.push_back(curve.PointAtStart());
    for (int span = 0; span <= curve.CVCount() - curve.Order(); ++span) {
	const int knot = span + curve.Order() - 2;
	if (!(curve.Knot(knot) < curve.Knot(knot + 1)))
	    continue;
	ON_BezierCurve bezier;
	if (++subdivisions > MAX_CURVE_SUBDIVISIONS || !curve.ConvertSpanToBezier(span, bezier) ||
	    points.back().DistanceTo(bezier.PointAt(0.0)) > tolerance ||
	    !append_curve_polygon(bezier, tolerance, 0, subdivisions, points))
	    return false;
    }
    return points.size() > 1;
}

bool
polygon_lengths(const std::vector<ON_3dPoint> &points, std::vector<double> &lengths)
{
    lengths.assign(points.size(), 0.0);
    for (size_t i = 1; i < points.size(); ++i)
	lengths[i] = lengths[i - 1] + points[i].DistanceTo(points[i - 1]);
    const double total = lengths.back();
    if (!std::isfinite(total) || !(total > 0.0))
	return false;
    for (double &length : lengths)
	length /= total;
    return true;
}

bool
polygon_vertices_match(const std::vector<ON_3dPoint> &first,
    const std::vector<double> &first_lengths, const std::vector<ON_3dPoint> &second,
    const std::vector<double> &second_lengths, double tolerance)
{
    size_t segment = 1;
    for (size_t i = 0; i < first.size(); ++i) {
	while (segment + 1 < second.size() && second_lengths[segment] < first_lengths[i])
	    ++segment;
	const double length = second_lengths[segment] - second_lengths[segment - 1];
	const double fraction = length > 0.0 ?
	    (first_lengths[i] - second_lengths[segment - 1]) / length : 0.0;
	const ON_3dPoint point = ON_Line(second[segment - 1], second[segment]).PointAt(fraction);
	if (first[i].DistanceTo(point) > tolerance)
	    return false;
    }
    return true;
}

bool
matching_polygons(const std::vector<ON_3dPoint> &first,
    const std::vector<ON_3dPoint> &second, double tolerance)
{
    std::vector<double> first_lengths, second_lengths;
    /* Between vertices of either polygon the paired difference is linear;
     * testing both vertex sets bounds the entire paired paths. */
    return polygon_lengths(first, first_lengths) && polygon_lengths(second, second_lengths) &&
	polygon_vertices_match(first, first_lengths, second, second_lengths, tolerance) &&
	polygon_vertices_match(second, second_lengths, first, first_lengths, tolerance);
}

bool
point_may_meet_bezier(const ON_3dPoint &point, const ON_BezierCurve &curve,
    double tolerance, unsigned int depth, size_t &subdivisions)
{
    ON_BoundingBox bounds;
    for (int i = 0; i < curve.CVCount(); ++i) {
	ON_3dPoint control;
	if (!(curve.Weight(i) > 0.0) || !std::isfinite(curve.Weight(i)) ||
	    !curve.GetCV(i, control) || !control.IsValid())
	    return true;
	bounds.Set(control, i != 0);
    }
    ON_3dPoint closest;
    for (int axis = 0; axis < 3; ++axis)
	closest[axis] = std::max(bounds.m_min[axis], std::min(bounds.m_max[axis], point[axis]));
    if (point.DistanceTo(closest) > tolerance)
	return false;
    if (point.DistanceTo(curve.PointAt(0.0)) <= tolerance ||
	point.DistanceTo(curve.PointAt(1.0)) <= tolerance ||
	depth >= MAX_CURVE_SUBDIVISION_DEPTH || ++subdivisions > MAX_POINT_CURVE_SUBDIVISIONS)
	return true;
    ON_BezierCurve left, right;
    return !curve.Split(0.5, left, right) ||
	point_may_meet_bezier(point, left, tolerance, depth + 1, subdivisions) ||
	point_may_meet_bezier(point, right, tolerance, depth + 1, subdivisions);
}


bool
bezier_locus_within_ball(const ON_BezierCurve &curve,
    const ON_3dPoint &center, double tolerance, unsigned int depth,
    size_t &subdivisions)
{
    const ON_3dPoint start = curve.PointAt(0.0);
    const ON_3dPoint end = curve.PointAt(1.0);
    if (!start.IsValid() || !end.IsValid() ||
	start.DistanceTo(center) > tolerance ||
	end.DistanceTo(center) > tolerance)
    {
	return false;
    }

    bool hull_is_inside = true;
    for (int control_index = 0; control_index < curve.CVCount();
	 ++control_index)
    {
	ON_3dPoint control;
	const double weight = curve.Weight(control_index);
	if (!(weight > 0.0) || !std::isfinite(weight) ||
	    !curve.GetCV(control_index, control) || !control.IsValid())
	{
	    return false;
	}
	hull_is_inside = hull_is_inside &&
	    control.DistanceTo(center) <= tolerance;
    }
    /* A positive-weight rational Bezier curve is a convex combination of
     * its Euclidean control points.  A ball is convex, so an enclosed
     * control hull certifies the complete curve locus without sampling. */
    if (hull_is_inside) {
	return true;
    }

    if (depth >= MAX_CURVE_SUBDIVISION_DEPTH ||
	++subdivisions > MAX_BALL_CURVE_SUBDIVISIONS)
    {
	return false;
    }
    ON_BezierCurve left;
    ON_BezierCurve right;
    return curve.Split(0.5, left, right) &&
	bezier_locus_within_ball(left, center, tolerance, depth + 1,
	    subdivisions) &&
	bezier_locus_within_ball(right, center, tolerance, depth + 1,
	    subdivisions);
}

bool
curve_has_separated_point(const ON_NurbsCurve &first, const ON_NurbsCurve &second,
    double tolerance)
{
    /* Samples are used only to DISPROVE coincidence.  Subdivided positive
     * weight control hulls enclose every point of the other curve; an
     * exhausted work budget is inconclusive, never a separation proof. */
    constexpr double PROBES[] = {0.25, 0.5, 0.75};
    for (double parameter : PROBES) {
	const ON_3dPoint point = first.PointAt(parameter);
	if (!point.IsValid())
	    return false;
	size_t subdivisions = 0;
	bool may_meet = false;
	for (int span = next_curve_span(second, 0); span <= second.CVCount() - second.Order();
	    span = next_curve_span(second, span + 1)) {
	    ON_BezierCurve bezier;
	    if (!second.ConvertSpanToBezier(span, bezier) ||
		point_may_meet_bezier(point, bezier, tolerance, 0, subdivisions)) {
		may_meet = true;
		break;
	    }
	}
	if (!may_meet)
	    return true;
    }
    return false;
}


struct edge_match {
    int first;
    int second;
    bool reversed;
    double tolerance;
    double endpoint_deviation;
};


double
edge_match_endpoint_deviation(const ON_BrepEdge &first,
    const ON_BrepEdge &second, bool reversed)
{
    const ON_3dPoint second_start = reversed ? second.PointAtEnd() :
	second.PointAtStart();
    const ON_3dPoint second_end = reversed ? second.PointAtStart() :
	second.PointAtEnd();
    return std::max(first.PointAtStart().DistanceTo(second_start),
	first.PointAtEnd().DistanceTo(second_end));
}


double
edge_coordinate_roundoff_floor(const ON_BrepEdge &edge)
{
    const ON_3dPoint start = edge.PointAtStart();
    const ON_3dPoint end = edge.PointAtEnd();
    if (!start.IsValid() || !end.IsValid())
	return std::numeric_limits<double>::infinity();

    double coordinate_scale = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(start[axis]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(end[axis]));
    }
    return std::max(ON_ZERO_TOLERANCE,
	64.0 * ON_EPSILON * coordinate_scale);
}


int
naked_edge_count(const ON_Brep &brep)
{
    int count = 0;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (brep.m_E[i].m_ti.Count() == 1)
	    ++count;
    return count;
}


int
nonmanifold_edge_count(const ON_Brep &brep)
{
    int count = 0;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (brep.m_E[i].m_ti.Count() > 2)
	    ++count;
    return count;
}


double
stitch_match_tolerance(const ON_BrepEdge &first, const ON_BrepEdge &second,
    double requested_tolerance)
{
    ON_BoundingBox first_bounds;
    ON_BoundingBox second_bounds;
    if (!first.GetBoundingBox(first_bounds, false) ||
	!second.GetBoundingBox(second_bounds, false) ||
	!first_bounds.IsValid() || !second_bounds.IsValid())
    {
	return 0.0;
    }
    const double first_extent = first_bounds.Diagonal().Length();
    const double second_extent = second_bounds.Diagonal().Length();
    if (!(first_extent > 0.0) || !(second_extent > 0.0) ||
	!std::isfinite(first_extent) || !std::isfinite(second_extent))
    {
	return 0.0;
    }

    /* Keep the relative feature-size guard inclusive in floating point.
     * Exact construction at the guard can accumulate a few ulps when a
     * transformed endpoint is evaluated independently on the two faces.
     * Without a coordinate-scale allowance, a seam whose mathematical
     * deviation is exactly the permitted fraction is rejected merely because
     * its computed distance is microscopically larger.  This allowance is
     * many orders of magnitude below both the requested model tolerance and
     * the feature-size guard, so it cannot admit a geometrically distinct
     * nearby edge. */
    const double roundoff_allowance = std::max(
	edge_coordinate_roundoff_floor(first),
	edge_coordinate_roundoff_floor(second));
    const double relative_limit = MAX_RELATIVE_STITCH_DEVIATION *
	std::min(first_extent, second_extent);
    return std::min(requested_tolerance,
	relative_limit + roundoff_allowance);
}


bool
merge_edge_pair(ON_Brep &brep, int first_index, int second_index,
	bool reversed, enum brep_assembly_error *error)
{
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int first_vertex = brep.m_E[first_index].m_vi[endpoint];
	const int second_vertex = brep.m_E[second_index].m_vi[
	    reversed ? 1 - endpoint : endpoint];
	if (first_vertex < 0 || first_vertex >= brep.m_V.Count() ||
		second_vertex < 0 || second_vertex >= brep.m_V.Count()) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
	if (first_vertex == second_vertex)
	    continue;
	ON_BrepVertex &first = brep.m_V[first_vertex];
	ON_BrepVertex &second = brep.m_V[second_vertex];
	const double separation = first.Point().DistanceTo(second.Point());
	if (!std::isfinite(separation)) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
	/* Matching edge geometry establishes the shared endpoint.  Authored
	 * face boundaries may locate its topological vertices farther apart than
	 * the global resolution, so retain that deviation as vertex tolerance. */
	first.m_tolerance = std::isfinite(first.m_tolerance) &&
	    first.m_tolerance >= 0.0 ?
	    std::max(first.m_tolerance, separation) : separation;
	second.m_tolerance = std::isfinite(second.m_tolerance) &&
	    second.m_tolerance >= 0.0 ?
	    std::max(second.m_tolerance, separation) : separation;
	if (!brep.CombineCoincidentVertices(first, second)) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
    }

    /* CombineCoincidentEdges requires identical vertex order.  Reverse the
     * duplicate edge as a semantic operation; OpenNURBS also updates all of
     * that edge's trim senses. */
    if (reversed && !brep.m_E[second_index].Reverse()) {
	*error = BREP_ASSEMBLY_EDGE_MERGE_FAILED;
	return false;
    }
    if (!brep.CombineCoincidentEdges(brep.m_E[first_index],
	    brep.m_E[second_index])) {
	*error = BREP_ASSEMBLY_EDGE_MERGE_FAILED;
	return false;
    }
    return true;
}


int
collapse_stitch_self_edges(ON_Brep &brep,
	const std::vector<bool> &distinct_endpoints,
	enum brep_assembly_error *error)
{
    std::vector<int> candidates;
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = brep.m_E[edge_index];
	if (edge_index < static_cast<int>(distinct_endpoints.size()) &&
		distinct_endpoints[edge_index] &&
		edge.m_edge_index == edge_index && edge.m_ti.Count() > 0 &&
		edge.m_vi[0] == edge.m_vi[1])
	{
	    candidates.push_back(edge_index);
	}
    }
    std::stable_sort(candidates.begin(), candidates.end(),
	[&brep](int first, int second) {
	    const int first_trim_count = brep.m_E[first].m_ti.Count();
	    const int second_trim_count = brep.m_E[second].m_ti.Count();
	    return first_trim_count != second_trim_count ?
		first_trim_count > second_trim_count : first > second;
	});

    int collapsed = 0;
    for (int edge_index : candidates) {
	ON_BrepEdge &edge = brep.m_E[edge_index];
	if (edge.m_edge_index != edge_index || edge.m_ti.Count() == 0 ||
		edge.m_vi[0] != edge.m_vi[1])
	{
	    continue;
	}
	if (edge.m_ti.Count() == 1) {
	    const int trim_index = edge.m_ti[0];
	    if (trim_index >= 0 && trim_index < brep.m_T.Count()) {
		const int loop_index = brep.m_T[trim_index].m_li;
		if (loop_index >= 0 && loop_index < brep.m_L.Count()) {
		    ON_BrepLoop &loop = brep.m_L[loop_index];
		    if (loop.m_loop_index == loop_index &&
			    loop.m_ti.Count() == 1 && loop.m_ti[0] == trim_index)
		    {
			if (loop.m_type == ON_BrepLoop::outer &&
				loop.m_fi >= 0 && loop.m_fi < brep.m_F.Count() &&
				brep.m_F[loop.m_fi].m_face_index == loop.m_fi)
			{
			    brep.DeleteFace(brep.m_F[loop.m_fi], true);
			} else if (loop.m_type == ON_BrepLoop::inner) {
			    brep.DeleteLoop(loop, true);
			}
			if (edge.m_edge_index != edge_index ||
				edge.m_ti.Count() == 0)
			{
			    ++collapsed;
			    continue;
			}
		    }
		}
	    }
	}
	const int vertex_index = edge.m_vi[0];
	if (vertex_index < 0 || vertex_index >= brep.m_V.Count() ||
		!brep.CollapseEdge(edge_index, true, vertex_index))
	{
	    *error = BREP_ASSEMBLY_EDGE_COLLAPSE_FAILED;
	    return -1;
	}
	++collapsed;
    }
    return collapsed;
}


bool
close_stitch_trim_gaps(ON_Brep &brep, double model_tolerance)
{
    if (!(model_tolerance > 0.0) || !std::isfinite(model_tolerance))
	return false;
    for (int loop_index = 0; loop_index < brep.m_L.Count(); ++loop_index) {
	ON_BrepLoop &loop = brep.m_L[loop_index];
	if (loop.m_loop_index != loop_index || loop.m_ti.Count() < 2 ||
		loop.m_fi < 0 || loop.m_fi >= brep.m_F.Count())
	{
	    continue;
	}
	const ON_Surface *surface = brep.m_F[loop.m_fi].SurfaceOf();
	if (surface == NULL)
	    return false;
	const double parameter_scale = std::hypot(
	    surface->Domain(0).Length(), surface->Domain(1).Length());
	if (!(parameter_scale > 0.0) || !std::isfinite(parameter_scale))
	    return false;
	const double maximum_gap = MAX_RELATIVE_TRIM_GAP * parameter_scale;
	for (int trim_position = 0; trim_position < loop.m_ti.Count();
		++trim_position)
	{
	    const int first_index = loop.m_ti[trim_position];
	    const int second_index = loop.m_ti[
		(trim_position + 1) % loop.m_ti.Count()];
	    if (first_index < 0 || first_index >= brep.m_T.Count() ||
		    second_index < 0 || second_index >= brep.m_T.Count())
	    {
		return false;
	    }
	    ON_BrepTrim &first = brep.m_T[first_index];
	    ON_BrepTrim &second = brep.m_T[second_index];
	    if (first.m_vi[1] != second.m_vi[0])
		return false;
	    const ON_3dPoint first_end = first.PointAtEnd();
	    const ON_3dPoint second_start = second.PointAtStart();
	    const double gap = first_end.DistanceTo(second_start);
	    if (!first_end.IsValid() || !second_start.IsValid() ||
		    !std::isfinite(gap) || gap > maximum_gap)
	    {
		return false;
	    }
	    if (gap > ON_ZERO_TOLERANCE) {
		const int vertex_index = first.m_vi[1];
		const ON_3dPoint first_lift = surface->PointAt(first_end.x,
		    first_end.y);
		const ON_3dPoint second_lift = surface->PointAt(second_start.x,
		    second_start.y);
		if (vertex_index < 0 || vertex_index >= brep.m_V.Count() ||
		    !first_lift.IsValid() || !second_lift.IsValid() ||
		    first_lift.DistanceTo(second_lift) > model_tolerance ||
		    first_lift.DistanceTo(brep.m_V[vertex_index].Point()) >
			model_tolerance ||
		    second_lift.DistanceTo(brep.m_V[vertex_index].Point()) >
			model_tolerance)
		{
		    return false;
		}
		if (!brep.CloseTrimGap(first, second))
		    return false;
		if (first.PointAtEnd().DistanceTo(second.PointAtStart()) >
			ON_ZERO_TOLERANCE) {
		    if (second.m_c2i < 0 || second.m_c2i >= brep.m_C2.Count() ||
			    brep.m_C2[second.m_c2i] == NULL)
		    {
			return false;
		    }
		    ON_Curve *parameter_curve = brep.m_C2[second.m_c2i];
		    const bool endpoint_set = second.ProxyCurveIsReversed() ?
			parameter_curve->SetEndPoint(first.PointAtEnd()) :
			parameter_curve->SetStartPoint(first.PointAtEnd());
		    if (!endpoint_set || !parameter_curve->IsValid())
			return false;
		    second.DestroyPspaceInformation();
		    if (first.PointAtEnd().DistanceTo(second.PointAtStart()) >
			    ON_ZERO_TOLERANCE)
		    {
			return false;
		    }
		}
	    }
	}
    }
    brep.SetTrimBoundingBoxes(false);
    return true;
}


void
set_unset_tolerances(ON_Brep &brep, double tolerance)
{
    for (int i = 0; i < brep.m_V.Count(); ++i)
	if (!std::isfinite(brep.m_V[i].m_tolerance) ||
		brep.m_V[i].m_tolerance < 0.0)
	    brep.m_V[i].m_tolerance = tolerance;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (!std::isfinite(brep.m_E[i].m_tolerance) ||
		brep.m_E[i].m_tolerance < 0.0)
	    brep.m_E[i].m_tolerance = tolerance;
    for (int i = 0; i < brep.m_T.Count(); ++i)
	for (int axis = 0; axis < 2; ++axis)
	    if (!std::isfinite(brep.m_T[i].m_tolerance[axis]) ||
		    brep.m_T[i].m_tolerance[axis] < 0.0)
		brep.m_T[i].m_tolerance[axis] = tolerance;
}

}


bool
brep_curves_coincident(const ON_Curve &first, const ON_Curve &second,
	double tolerance, bool *reversed)
{
    if (!std::isfinite(tolerance) || tolerance < 0.0)
	return false;
    const bool forward = first.PointAtStart().DistanceTo(second.PointAtStart()) <= tolerance &&
	first.PointAtEnd().DistanceTo(second.PointAtEnd()) <= tolerance;
    const bool reverse = first.PointAtStart().DistanceTo(second.PointAtEnd()) <= tolerance &&
	first.PointAtEnd().DistanceTo(second.PointAtStart()) <= tolerance;
    if (!forward && !reverse)
	return false;
    ON_NurbsCurve a, b;
    if (!first.GetNurbForm(a) || !second.GetNurbForm(b) || !a.IsValid() || !b.IsValid() ||
	!a.SetDomain(0.0, 1.0) || !b.SetDomain(0.0, 1.0))
	return false;
    for (bool direction : {false, true}) {
	if (direction && (!b.Reverse() || !b.SetDomain(0.0, 1.0)))
	    return false;
	const bool same_knots = a.Order() == b.Order() && a.CVCount() == b.CVCount() &&
	    std::equal(a.m_knot, a.m_knot + a.KnotCount(), b.m_knot);
	if ((direction ? reverse : forward) &&
	    ((same_knots && matching_basis_controls(a, b, tolerance)) ||
	     matching_bezier_spans(a, b, tolerance) || matching_refined_spans(a, b, tolerance))) {
	    if (reversed)
		*reversed = direction;
	    return true;
	}
    }
    if (!(tolerance > 0.0))
	return false;
    std::vector<ON_3dPoint> first_polygon, second_polygon;
    if (curve_has_separated_point(a, b, tolerance) || curve_has_separated_point(b, a, tolerance))
	return false;
    const double approximation = tolerance * CURVE_APPROXIMATION_FRACTION;
    if (!curve_polygon(a, approximation, first_polygon) || !curve_polygon(b, approximation, second_polygon))
	return false;
    /* b is reversed from the control-point comparison above. */
    /* Each curve budget also covers endpoint roundoff between spans. */
    const double comparison = tolerance - 4.0 * approximation;
    for (bool direction : {true, false}) {
	if (!direction)
	    std::reverse(second_polygon.begin(), second_polygon.end());
	if ((direction ? reverse : forward) && matching_polygons(first_polygon, second_polygon, comparison)) {
	    if (reversed)
		*reversed = direction;
	    return true;
	}
    }
    return false;
}


bool
brep_curve_locus_within_ball(const ON_Curve &curve,
	const ON_3dPoint &center, double tolerance)
{
    if (!center.IsValid() || !std::isfinite(tolerance) || tolerance < 0.0)
	return false;

    ON_NurbsCurve nurbs;
    if (curve.GetNurbForm(nurbs) <= 0 || !nurbs.IsValid())
	return false;

    size_t subdivisions = 0;
    bool found_span = false;
    for (int span = 0; span <= nurbs.CVCount() - nurbs.Order(); ++span) {
	const int knot = span + nurbs.Order() - 2;
	if (!(nurbs.Knot(knot) < nurbs.Knot(knot + 1)))
	    continue;
	found_span = true;
	ON_BezierCurve bezier;
	if (!nurbs.ConvertSpanToBezier(span, bezier) ||
	    !bezier_locus_within_ball(bezier, center, tolerance, 0,
		subdivisions))
	{
	    return false;
	}
    }
    return found_span;
}


bool
brep_set_edge_endpoint_tolerances(ON_Brep &brep,
	double minimum_tolerance)
{
    if (!std::isfinite(minimum_tolerance) || minimum_tolerance <= 0.0)
	return false;

    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	ON_BrepEdge &edge = brep.m_E[edge_index];
	const ON_3dPoint edge_start = edge.PointAtStart();
	const ON_3dPoint edge_end = edge.PointAtEnd();
	if (!edge_start.IsValid() || !edge_end.IsValid())
	    return false;
	double measured = std::isfinite(edge.m_tolerance) &&
	    edge.m_tolerance >= 0.0 ? edge.m_tolerance : minimum_tolerance;
	measured = std::max(measured, minimum_tolerance);

	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vertex_index = edge.m_vi[endpoint];
	    if (vertex_index < 0 || vertex_index >= brep.m_V.Count())
		return false;
	    const ON_3dPoint vertex = brep.m_V[vertex_index].Point();
	    if (!vertex.IsValid())
		return false;
	    const ON_3dPoint edge_point = endpoint == 0 ? edge_start : edge_end;
	    const double distance = vertex.DistanceTo(edge_point);
	    if (!std::isfinite(distance))
		return false;
	    measured = std::max(measured, distance);
	}

	for (int use = 0; use < edge.m_ti.Count(); ++use) {
	    const int trim_index = edge.m_ti[use];
	    if (trim_index < 0 || trim_index >= brep.m_T.Count())
		return false;
	    const ON_BrepTrim &trim = brep.m_T[trim_index];
	    ON_3dPoint trim_start;
	    ON_3dPoint trim_end;
	    if (!brep.GetTrim3dStart(trim_index, trim_start) ||
		    !brep.GetTrim3dEnd(trim_index, trim_end) ||
		    !trim_start.IsValid() || !trim_end.IsValid())
		return false;
	    const ON_3dPoint expected_start = trim.m_bRev3d ?
		edge_end : edge_start;
	    const ON_3dPoint expected_end = trim.m_bRev3d ?
		edge_start : edge_end;
	    const double start_distance = trim_start.DistanceTo(expected_start);
	    const double end_distance = trim_end.DistanceTo(expected_end);
	    if (!std::isfinite(start_distance) || !std::isfinite(end_distance))
		return false;
	    measured = std::max(measured,
		std::max(start_distance, end_distance));
	}
	edge.m_tolerance = measured;
    }
    return true;
}


int
brep_stitch_naked_edges(ON_Brep &brep, double tolerance,
	brep_assembly_result *result)
{
    brep_assembly_result local;
    brep_assembly_result &report = result ? *result : local;
    report = brep_assembly_result();
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
	report.error = BREP_ASSEMBLY_INVALID_TOLERANCE;
	return -1;
    }
    report.input_naked_edges = naked_edge_count(brep);

    std::vector<int> match_counts(brep.m_E.Count(), 0);
    std::vector<ON_BoundingBox> edge_boxes(brep.m_E.Count());
    std::vector<edge_match> matches;
    ON_RTree edge_tree(report.input_naked_edges);
    for (int first = 0; first < brep.m_E.Count(); ++first) {
	if (brep.m_E[first].m_ti.Count() != 1)
	    continue;
	if (!brep.m_E[first].GetBoundingBox(edge_boxes[first], false) ||
		!edge_boxes[first].IsValid())
	    continue;
	edge_tree.Insert(edge_boxes[first].m_min, edge_boxes[first].m_max, first);
    }
    for (int first = 0; first < brep.m_E.Count(); ++first) {
	if (!edge_boxes[first].IsValid())
	    continue;
	const double search_min[3] = {
	    edge_boxes[first].m_min.x - tolerance,
	    edge_boxes[first].m_min.y - tolerance,
	    edge_boxes[first].m_min.z - tolerance
	};
	const double search_max[3] = {
	    edge_boxes[first].m_max.x + tolerance,
	    edge_boxes[first].m_max.y + tolerance,
	    edge_boxes[first].m_max.z + tolerance
	};
	ON_SimpleArray<int> candidates;
	edge_tree.Search(search_min, search_max, candidates);
	for (int candidate = 0; candidate < candidates.Count(); ++candidate) {
	    const int second = candidates[candidate];
	    if (second <= first)
		continue;
	    const double match_tolerance = stitch_match_tolerance(
		brep.m_E[first], brep.m_E[second], tolerance);
	    if (!(match_tolerance > 0.0))
		continue;
	    bool reversed = false;
	    if (!brep_curves_coincident(brep.m_E[first], brep.m_E[second],
		    match_tolerance, &reversed))
		continue;
	    const double endpoint_deviation = edge_match_endpoint_deviation(
		brep.m_E[first], brep.m_E[second], reversed);
	    if (!std::isfinite(endpoint_deviation))
		continue;
	    ++match_counts[first];
	    ++match_counts[second];
	    matches.push_back({first, second, reversed, match_tolerance,
		endpoint_deviation});
	}
    }

    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<int> best_match(brep.m_E.Count(), -1);
    std::vector<double> best_deviation(brep.m_E.Count(), infinity);
    std::vector<double> second_deviation(brep.m_E.Count(), infinity);
    for (size_t match_index = 0; match_index < matches.size(); ++match_index) {
	const edge_match &match = matches[match_index];
	for (int edge_index : {match.first, match.second}) {
	    if (match.endpoint_deviation < best_deviation[edge_index]) {
		second_deviation[edge_index] = best_deviation[edge_index];
		best_deviation[edge_index] = match.endpoint_deviation;
		best_match[edge_index] = static_cast<int>(match_index);
	    } else {
		second_deviation[edge_index] = std::min(
		    second_deviation[edge_index], match.endpoint_deviation);
	    }
	}
    }

    std::vector<bool> selected_matches(matches.size(), false);
    std::vector<bool> selected_edges(brep.m_E.Count(), false);
    for (size_t match_index = 0; match_index < matches.size(); ++match_index) {
	edge_match &match = matches[match_index];
	if (best_match[match.first] != static_cast<int>(match_index) ||
		best_match[match.second] != static_cast<int>(match_index))
	{
	    continue;
	}

	double alternative_lower_bound = infinity;
	bool decisive = true;
	for (int edge_index : {match.first, match.second}) {
	    if (match_counts[edge_index] <= 1)
		continue;
	    const double roundoff_floor =
		edge_coordinate_roundoff_floor(brep.m_E[edge_index]);
	    const double comparison_deviation = std::max(
		best_deviation[edge_index], roundoff_floor);
	    if (!std::isfinite(second_deviation[edge_index]) ||
		second_deviation[edge_index] <
		    MIN_STITCH_MATCH_SEPARATION_RATIO * comparison_deviation)
	    {
		decisive = false;
		break;
	    }
	    alternative_lower_bound = std::min(alternative_lower_bound,
		second_deviation[edge_index]);
	}
	if (!decisive)
	    continue;

	/* Endpoint separation is a lower bound on full-curve deviation.  Prove
	 * the preferred pair coincident again below half the closest runner-up's
	 * lower bound, so every rejected candidate is geometrically excluded at
	 * the tolerance used to authorize this merge. */
	double certification_tolerance = match.tolerance;
	if (std::isfinite(alternative_lower_bound))
	    certification_tolerance = std::min(certification_tolerance,
		0.5 * alternative_lower_bound);
	bool certified_reversed = false;
	if (!(certification_tolerance > 0.0) ||
		!brep_curves_coincident(brep.m_E[match.first],
		    brep.m_E[match.second], certification_tolerance,
		    &certified_reversed))
	{
	    continue;
	}
	match.reversed = certified_reversed;
	selected_matches[match_index] = true;
	selected_edges[match.first] = true;
	selected_edges[match.second] = true;
    }

    report.ambiguous_edges = 0;
    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index)
	if (match_counts[edge_index] > 0 && !selected_edges[edge_index])
	    ++report.ambiguous_edges;

    /* Vertex unification can turn a short intervening boundary into an
     * invalid nonclosed self-edge.  Work on a copy so a failed OpenNURBS
     * topology repair never exposes a partially stitched B-Rep. */
    ON_Brep stitched(brep);
    std::vector<bool> distinct_endpoints(stitched.m_E.Count(), false);
    for (int edge_index = 0; edge_index < stitched.m_E.Count(); ++edge_index) {
	const ON_BrepEdge &edge = stitched.m_E[edge_index];
	distinct_endpoints[edge_index] = edge.m_edge_index == edge_index &&
	    edge.m_vi[0] >= 0 && edge.m_vi[1] >= 0 &&
	    edge.m_vi[0] != edge.m_vi[1];
    }
    for (size_t match_index = 0; match_index < matches.size(); ++match_index) {
	if (!selected_matches[match_index])
	    continue;
	const edge_match &match = matches[match_index];
	if (!merge_edge_pair(stitched, match.first, match.second, match.reversed,
		&report.error))
	    return -1;
	++report.merged_edges;
    }

    int collapsed_edges = 0;
    if (report.merged_edges > 0) {
	collapsed_edges = collapse_stitch_self_edges(stitched,
	    distinct_endpoints, &report.error);
	if (collapsed_edges < 0)
	    return -1;
    }
    if (report.merged_edges > 0 && !stitched.Compact()) {
	report.error = BREP_ASSEMBLY_CULL_FAILED;
	return -1;
    }
    /* Vertex unification during an ordinary edge merge can expose the same
     * sub-ULP parameter-space endpoint discrepancy as collapsing a short
     * connector.  Normalize all affected loop gaps whenever stitching made
     * a topological change, not only when it also collapsed an edge. */
	if (report.merged_edges > 0 &&
		!close_stitch_trim_gaps(stitched, tolerance)) {
	report.error = BREP_ASSEMBLY_TRIM_GAP_FAILED;
	return -1;
    }
    if (report.merged_edges > 0)
	brep = stitched;
    report.remaining_naked_edges = naked_edge_count(brep);
    report.nonmanifold_edges = nonmanifold_edge_count(brep);
    return report.merged_edges;
}


bool
brep_orient_faces(ON_Brep &brep)
{
    const int face_count = brep.m_F.Count();
    if (face_count < 1)
	return false;
    ON_SimpleArray<int> flip;
    for (int i = 0; i < face_count; ++i)
	flip.Append(-1);

    for (int seed = 0; seed < face_count; ++seed) {
	if (flip[seed] >= 0)
	    continue;
	flip[seed] = 0;
	bool changed = true;
	while (changed) {
	    changed = false;
	    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
		const ON_BrepEdge &edge = brep.m_E[edge_index];
		if (edge.m_ti.Count() != 2)
		    return false;
		const ON_BrepTrim &first_trim = brep.m_T[edge.m_ti[0]];
		const ON_BrepTrim &second_trim = brep.m_T[edge.m_ti[1]];
		if (first_trim.m_li < 0 || first_trim.m_li >= brep.m_L.Count() ||
			second_trim.m_li < 0 || second_trim.m_li >= brep.m_L.Count())
		    return false;
		const int first_face = brep.m_L[first_trim.m_li].m_fi;
		const int second_face = brep.m_L[second_trim.m_li].m_fi;
		if (first_face < 0 || first_face >= face_count ||
			second_face < 0 || second_face >= face_count)
		    return false;
		const int first_sense = first_trim.m_bRev3d ^
		    brep.m_F[first_face].m_bRev;
		const int second_sense = second_trim.m_bRev3d ^
		    brep.m_F[second_face].m_bRev;
		const int relation = first_sense ^ second_sense ^ 1;
		if (first_face == second_face) {
		    if (relation != 0)
			return false;
		    continue;
		}
		if (flip[first_face] >= 0 && flip[second_face] < 0) {
		    flip[second_face] = flip[first_face] ^ relation;
		    changed = true;
		} else if (flip[second_face] >= 0 && flip[first_face] < 0) {
		    flip[first_face] = flip[second_face] ^ relation;
		    changed = true;
		} else if (flip[first_face] >= 0 &&
			flip[second_face] != (flip[first_face] ^ relation)) {
		    bu_log("B-Rep orientation conflict at edge %d: trim %d face "
			"%d sense %d flip %d; trim %d face %d sense %d "
			"flip %d; relation %d.\n", edge_index,
			first_trim.m_trim_index, first_face, first_sense,
			flip[first_face], second_trim.m_trim_index,
			second_face, second_sense, flip[second_face],
			relation);
		    return false;
		}
	    }
	}
    }

    for (int i = 0; i < face_count; ++i)
	if (flip[i] == 1)
	    brep.FlipFace(brep.m_F[i]);
    return true;
}


bool
brep_assemble(ON_Brep &brep, double tolerance,
	brep_assembly_result *result)
{
    brep_assembly_result local;
    brep_assembly_result &report = result ? *result : local;
    if (brep_stitch_naked_edges(brep, tolerance, &report) < 0)
	return false;

    if (report.remaining_naked_edges == 0 && report.nonmanifold_edges == 0) {
	report.oriented = brep_orient_faces(brep);
        if (!report.oriented)
	    report.error = BREP_ASSEMBLY_ORIENTATION_FAILED;
    }
    brep.SetTrimTolerances(false);
    brep.SetTrimIsoFlags();
    brep.SetTrimTypeFlags();
    brep.SetVertexTolerances(true);
    if (!brep_set_edge_endpoint_tolerances(brep, tolerance)) {
	report.error = BREP_ASSEMBLY_TOLERANCE_FAILED;
	return false;
    }
    brep.SetTrimBoundingBoxes(false);
    set_unset_tolerances(brep, tolerance);

    ON_wString messages;
    ON_TextLog log(messages);
    report.valid = brep.IsValid(&log);
    ON_String validation_text(messages);
    report.validation_log = validation_text.Array() ? validation_text.Array() : "";
    report.solid = report.valid && brep.IsSolid();
    if (!report.valid && report.error == BREP_ASSEMBLY_OK)
	report.error = BREP_ASSEMBLY_VALIDATION_FAILED;
    return report.valid;
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
