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
#include <vector>

#include "brep.h"


namespace {

/* A failed proof must leave edges separate, including when refinement is
 * too expensive.  Divide the error budget between both approximations
 * and the comparison of their polygonal paths. */
constexpr size_t MAX_CURVE_SUBDIVISIONS = 65536;
constexpr unsigned int MAX_CURVE_SUBDIVISION_DEPTH = 48;
constexpr double CURVE_APPROXIMATION_FRACTION = 0.125;
constexpr size_t MAX_POINT_CURVE_SUBDIVISIONS = 4096;
constexpr size_t MAX_PAIRED_CURVE_SUBDIVISIONS = 4096;

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
};


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
	    bool reversed = false;
	    if (!brep_curves_coincident(brep.m_E[first], brep.m_E[second],
		    tolerance, &reversed))
		continue;
	    ++match_counts[first];
	    ++match_counts[second];
	    matches.push_back({first, second, reversed});
	}
    }
    report.ambiguous_edges = std::count_if(match_counts.begin(),
	match_counts.end(), [](int count) { return count > 1; });
    for (const edge_match &match : matches) {
	if (match_counts[match.first] != 1 ||
		match_counts[match.second] != 1)
	    continue;
	if (!merge_edge_pair(brep, match.first, match.second, match.reversed,
		&report.error))
	    return -1;
	++report.merged_edges;
    }
    if (report.merged_edges > 0 &&
	    (!brep.CullUnusedEdges() || !brep.CullUnusedVertices() ||
	     !brep.CullUnused3dCurves())) {
	report.error = BREP_ASSEMBLY_CULL_FAILED;
	return -1;
    }
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
