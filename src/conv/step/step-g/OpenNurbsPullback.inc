/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/OpenNurbsPullback.inc
 *
 * Exact curve/surface association, pullback generation, periodic seam, and
 * bounded pcurve validation helpers.  Included only by OpenNurbsInterfaces.cpp.
 */

static double
maximum_verified_edge_tolerance(const ON_Curve *curve,
    double declared_tolerance, double item_scale = 0.0)
{
    if (!curve || !(declared_tolerance > 0.0)) return declared_tolerance;
    const ON_BoundingBox bounds = curve->BoundingBox();
    const double scale = bounds.IsValid() ? bounds.Diagonal().Length() : 0.0;
    const double relative_limit = std::max(
	scale * kMaximumRelativeEdgeMismatch,
	item_scale * kMaximumRelativeItemMismatch);
    return std::max(declared_tolerance *
	kMaximumDeclaredToleranceAdjustmentFactor, relative_limit);
}


static double
maximum_verified_edge_vertex_tolerance(const ON_Curve *curve,
    double declared_tolerance)
{
    double limit = maximum_verified_edge_tolerance(curve,
	declared_tolerance);
    if (!curve || !curve->IsValid()) return limit;
    const ON_Interval domain = curve->Domain();
    if (!domain.IsIncreasing()) return limit;
    /* A bounding-box diagonal understates the local scale of a curved edge.
     * Reuse the documented 1024-segment dense-validation resolution to obtain
     * a conservative chord-length estimate of this exact bounded interval.
     * This work is needed only when the source already violates its declared
     * endpoint tolerance. */
    ON_3dPoint previous = curve->PointAtStart();
    double chord_length = 0.0;
    for (int sample = 1; sample <= kDenseLiftValidationSegments; ++sample) {
	const ON_3dPoint point = curve->PointAt(domain.ParameterAt(
	    static_cast<double>(sample) / kDenseLiftValidationSegments));
	if (!previous.IsValid() || !point.IsValid()) return limit;
	chord_length += previous.DistanceTo(point);
	previous = point;
    }
    if (std::isfinite(chord_length))
	limit = std::max(limit,
	    chord_length * kMaximumRelativeEdgeVertexMismatch);
    return limit;
}


class CurveDistanceEvaluator {
public:
    explicit CurveDistanceEvaluator(const ON_Curve *curve)
    {
	/* Preserve native analytic curves instead of converting them to NURBS and
	 * repeatedly searching sampled chord brackets.  The helpers below compute
	 * the exact closest point on the bounded line/arc locus, so this is both
	 * faster and more accurate than the generic minimizer.  BREP edges are
	 * curve proxies whose domain can be a trimmed or reversed portion of the
	 * underlying curve.  Duplicate only analytic proxies: openNURBS then gives
	 * us a bounded native curve with exactly the proxy's locus and
	 * parameterization, rather than accidentally measuring the complete
	 * underlying curve. */
	std::unique_ptr<ON_Curve> analytic_proxy;
	const ON_CurveProxy *proxy = ON_CurveProxy::Cast(curve);
	if (proxy && (ON_LineCurve::Cast(proxy->ProxyCurve()) ||
		ON_ArcCurve::Cast(proxy->ProxyCurve()))) {
	    analytic_proxy.reset(proxy->DuplicateCurve());
	    curve = analytic_proxy.get();
	}
	const ON_LineCurve *line = ON_LineCurve::Cast(curve);
	if (line && line->IsValid()) {
	    m_line = *line;
	    m_analytic_kind = AnalyticKind::Line;
	    return;
	}
	const ON_ArcCurve *arc = ON_ArcCurve::Cast(curve);
	if (arc && arc->IsValid()) {
	    m_arc = *arc;
	    m_analytic_kind = AnalyticKind::Arc;
	    return;
	}
	if (!curve || !curve->GetNurbForm(m_nurbs)) return;
	const int span_count = m_nurbs.SpanCount();
	if (span_count <= 0) return;
	m_spans.resize(static_cast<size_t>(span_count) + 1);
	if (!m_nurbs.GetSpanVector(m_spans.data())) {
	    m_spans.clear();
	    return;
	}
	/* A positive-weight Bezier span lies inside the convex hull of its
	 * Euclidean control points.  Cache those conservative boxes so a
	 * tolerance query can rule out entire NURBS spans before examining their
	 * 64 chord brackets.  If conversion or weight validation is incomplete,
	 * leave the cache empty and retain the unrestricted search. */
	for (int nurbs_span = 0;
		nurbs_span <= m_nurbs.m_cv_count - m_nurbs.m_order;
		++nurbs_span) {
	    ON_BezierCurve bezier;
	    if (!m_nurbs.ConvertSpanToBezier(nurbs_span, bezier))
		continue;
	    bool positive_weights = true;
	    for (int cv = 0; cv < bezier.CVCount(); ++cv) {
		const double weight = bezier.Weight(cv);
		if (!std::isfinite(weight) || !(weight > 0.0)) {
		    positive_weights = false;
		    break;
		}
	    }
	    const ON_BoundingBox bounds = positive_weights ?
		bezier.BoundingBox() : ON_BoundingBox::EmptyBoundingBox;
	    if (!positive_weights || !bounds.IsValid()) {
		m_span_bounds.clear();
		break;
	    }
	    m_span_bounds.push_back(bounds);
	}
	if (m_span_bounds.size() != static_cast<size_t>(span_count))
	    m_span_bounds.clear();
	if (m_span_bounds.size() >= kCurveDistanceRTreeMinimumSpans &&
		m_span_bounds.size() <= static_cast<size_t>(INT_MAX)) {
	    std::unique_ptr<ON_RTree> span_tree(new ON_RTree(
		m_span_bounds.size()));
	    bool complete = true;
	    for (size_t span = 0; span < m_span_bounds.size(); ++span) {
		const ON_BoundingBox &bounds = m_span_bounds[span];
		m_curve_bounds.Union(bounds);
		if (!span_tree->Insert(&bounds.m_min.x, &bounds.m_max.x,
			static_cast<int>(span))) {
		    complete = false;
		    break;
		}
	    }
	    if (complete && m_curve_bounds.IsValid())
		m_span_tree = std::move(span_tree);
	}
	m_bracket_points.resize(static_cast<size_t>(span_count));
	for (int span = 0; span < span_count; ++span) {
	    const ON_Interval domain(m_spans[span], m_spans[span + 1]);
	    std::vector<ON_3dPoint> &points = m_bracket_points[span];
	    points.reserve(kCurveClosestBracketsPerSpan + 1);
	    for (int bracket = 0; bracket <= kCurveClosestBracketsPerSpan;
		 ++bracket) {
		points.push_back(m_nurbs.PointAt(domain.ParameterAt(
		    static_cast<double>(bracket) /
		    kCurveClosestBracketsPerSpan)));
	    }
	}
    }

    bool IsValid() const
    {
	return m_analytic_kind != AnalyticKind::None ||
	    (!m_spans.empty() && !m_bracket_points.empty());
    }

    bool ClosestParameter(const ON_3dPoint &point, double *parameter,
	    double *distance) const
    {
	if (parameter) *parameter = 0.0;
	if (distance) *distance = DBL_MAX;
	if (!parameter || !IsValid() || !point.IsValid()) return false;
	if (m_analytic_kind != AnalyticKind::None)
	    return AnalyticClosest(point, parameter, distance);
	double best_distance = DBL_MAX;
	double best_parameter = 0.0;
	for (size_t span = 0; span < m_bracket_points.size(); ++span) {
	    const std::vector<ON_3dPoint> &points = m_bracket_points[span];
	    int best_bracket = 0;
	    double best_chord_distance = DBL_MAX;
	    for (int bracket = 0; bracket < kCurveClosestBracketsPerSpan;
		    ++bracket) {
		ON_Line chord(points[bracket], points[bracket + 1]);
		double chord_parameter = 0.0;
		chord.ClosestPointTo(point, &chord_parameter);
		chord_parameter = std::max(0.0,
		    std::min(1.0, chord_parameter));
		const double chord_distance = point.DistanceTo(
		    chord.PointAt(chord_parameter));
		if (chord_distance < best_chord_distance) {
		    best_chord_distance = chord_distance;
		    best_bracket = bracket;
		}
	    }
	    double refined_parameter = 0.0;
	    const double refined_distance = RefinedDistance(point, span,
		best_bracket, &refined_parameter);
	    if (refined_distance < best_distance) {
		best_distance = refined_distance;
		best_parameter = refined_parameter;
	    }
	}
	*parameter = best_parameter;
	if (distance) *distance = best_distance;
	return std::isfinite(best_distance);
    }

    double DistanceTo(const ON_3dPoint &point,
	    double acceptable_distance = 0.0) const
    {
	if (!IsValid() || !point.IsValid()) return DBL_MAX;
	if (m_analytic_kind != AnalyticKind::None) {
	    double parameter = 0.0;
	    double distance = DBL_MAX;
	    return AnalyticClosest(point, &parameter, &distance) ?
		distance : DBL_MAX;
	}
	double minimum_distance = DBL_MAX;
	/* Dense pcurve samples are ordered along the same edge.  Try the previous
	 * exact knot-span neighborhood first.  Returning early remains rigorous:
	 * RefinedDistance evaluates the original NURBS curve and is used only when
	 * that evaluated point already satisfies the acceptance tolerance. */
	if (acceptable_distance > 0.0 && m_have_previous) {
	    const long bracket_count = static_cast<long>(
		m_bracket_points.size() * kCurveClosestBracketsPerSpan);
	    const long previous = static_cast<long>(m_previous_span *
		kCurveClosestBracketsPerSpan + m_previous_bracket);
	    for (long offset = -2; offset <= 2; ++offset) {
		const long candidate = previous + offset;
		if (candidate < 0 || candidate >= bracket_count) continue;
		const size_t span = static_cast<size_t>(candidate /
		    kCurveClosestBracketsPerSpan);
		const int bracket = static_cast<int>(candidate %
		    kCurveClosestBracketsPerSpan);
		const double distance = RefinedDistance(point, span, bracket,
		    NULL, acceptable_distance);
		if (distance <= acceptable_distance) {
		    m_previous_span = span;
		    m_previous_bracket = bracket;
		    return distance;
		}
	    }
	}
	struct SpanCandidate {
	    double chord_distance;
	    size_t span;
	    int bracket;
	};
	std::vector<size_t> spans_to_test;
	if (acceptable_distance > 0.0 && m_span_tree) {
	    const double query_min[3] = {
		point.x - acceptable_distance,
		point.y - acceptable_distance,
		point.z - acceptable_distance
	    };
	    const double query_max[3] = {
		point.x + acceptable_distance,
		point.y + acceptable_distance,
		point.z + acceptable_distance
	    };
	    ON_SimpleArray<int> hits;
	    if (m_span_tree->Search(query_min, query_max, hits)) {
		spans_to_test.reserve(static_cast<size_t>(hits.Count()));
		for (int hit = 0; hit < hits.Count(); ++hit) {
		    const int span = hits[hit];
		    if (span < 0 || static_cast<size_t>(span) >=
			    m_span_bounds.size())
			continue;
		    /* The search cube is a conservative superset of the Euclidean
		     * tolerance ball used by the old linear test. */
		    if (m_span_bounds[span].MinimumDistanceTo(point) <=
			    acceptable_distance)
			spans_to_test.push_back(static_cast<size_t>(span));
		}
		if (spans_to_test.empty())
		    return NearestSpanBound(point);
	    }
	}
	if (spans_to_test.empty()) {
	    spans_to_test.resize(m_bracket_points.size());
	    for (size_t span = 0; span < spans_to_test.size(); ++span)
		spans_to_test[span] = span;
	}
	std::vector<SpanCandidate> candidates;
	candidates.reserve(spans_to_test.size());
	double minimum_span_bound = DBL_MAX;
	for (std::vector<size_t>::const_iterator selected = spans_to_test.begin();
		selected != spans_to_test.end(); ++selected) {
	    const size_t span = *selected;
	    if (brlcad::PullbackWorkCancelled()) return DBL_MAX;
	    if (acceptable_distance > 0.0 &&
		    m_span_bounds.size() == m_bracket_points.size()) {
		const double span_bound =
		    m_span_bounds[span].MinimumDistanceTo(point);
		minimum_span_bound = std::min(minimum_span_bound, span_bound);
		if (span_bound > acceptable_distance)
		    continue;
	    }
	    const std::vector<ON_3dPoint> &points = m_bracket_points[span];
	    int best_bracket = 0;
	    double best_chord_distance = DBL_MAX;
	    for (int bracket = 0; bracket < kCurveClosestBracketsPerSpan;
		 ++bracket) {
		ON_Line chord(points[bracket], points[bracket + 1]);
		double chord_parameter = 0.0;
		chord.ClosestPointTo(point, &chord_parameter);
		chord_parameter = std::max(0.0,
		    std::min(1.0, chord_parameter));
		const double chord_distance = point.DistanceTo(
		    chord.PointAt(chord_parameter));
		if (chord_distance < best_chord_distance) {
		    best_chord_distance = chord_distance;
		    best_bracket = bracket;
		}
		minimum_distance = std::min(minimum_distance,
		    point.DistanceTo(points[bracket]));
	    }
	    minimum_distance = std::min(minimum_distance,
		point.DistanceTo(points.back()));
	    candidates.push_back({best_chord_distance, span, best_bracket});
	}
	if (candidates.empty() && minimum_span_bound < DBL_MAX)
	    return minimum_span_bound;
	std::sort(candidates.begin(), candidates.end(),
	    [](const SpanCandidate &left, const SpanCandidate &right) {
		return left.chord_distance < right.chord_distance;
	    });
	for (std::vector<SpanCandidate>::const_iterator candidate =
		candidates.begin(); candidate != candidates.end(); ++candidate) {
	    if (brlcad::PullbackWorkCancelled()) return DBL_MAX;
	    /* Every bracket point is an exact evaluation of the source NURBS.
	     * Once one is inside the caller's acceptance tolerance, that is already
	     * a complete membership proof and no local minimization is needed. */
	    if (acceptable_distance > 0.0 &&
		    minimum_distance <= acceptable_distance)
		return minimum_distance;
	    const size_t span = candidate->span;
	    const int best_bracket = candidate->bracket;
	    minimum_distance = std::min(minimum_distance,
		RefinedDistance(point, span, best_bracket, NULL,
		    acceptable_distance));
	    /* The caller only needs proof that some exact curve point is within
	     * its acceptance tolerance.  Stop as soon as one refined knot-span
	     * candidate supplies that proof; when it does not, all spans are still
	     * refined and the global measured distance is retained. */
	    if (acceptable_distance > 0.0 &&
		    minimum_distance <= acceptable_distance) {
		m_have_previous = true;
		m_previous_span = span;
		m_previous_bracket = best_bracket;
		return minimum_distance;
	    }
	}
	return minimum_distance;
    }

private:
    enum class AnalyticKind {
	None,
	Line,
	Arc
    };

    struct NearestSpanBoundContext {
	const CurveDistanceEvaluator *evaluator;
	const ON_3dPoint *point;
	ON_RTreeSphere *sphere;
	double nearest;
    };

    static bool ON_CALLBACK_CDECL NearestSpanBoundCallback(
	    void *opaque, ON__INT_PTR identifier)
    {
	NearestSpanBoundContext *context =
	    static_cast<NearestSpanBoundContext *>(opaque);
	if (!context || !context->evaluator || !context->point ||
		!context->sphere || identifier < 0 ||
		static_cast<size_t>(identifier) >=
		    context->evaluator->m_span_bounds.size())
	    return true;
	const double distance = context->evaluator->m_span_bounds[
	    static_cast<size_t>(identifier)].MinimumDistanceTo(*context->point);
	if (distance < context->nearest) {
	    context->nearest = distance;
	    /* ON_RTree explicitly permits a closest-point callback to shrink
	     * its search sphere.  Round outward so equal-distance boxes remain
	     * eligible despite floating-point comparison at a node boundary. */
	    context->sphere->m_radius = std::nextafter(distance, DBL_MAX);
	}
	return true;
    }

    double NearestSpanBound(const ON_3dPoint &point) const
    {
	if (!m_span_tree || !m_curve_bounds.IsValid()) return DBL_MAX;
	ON_RTreeSphere sphere;
	sphere.m_point[0] = point.x;
	sphere.m_point[1] = point.y;
	sphere.m_point[2] = point.z;
	sphere.m_radius = std::nextafter(
	    m_curve_bounds.MaximumDistanceTo(point), DBL_MAX);
	NearestSpanBoundContext context = {
	    this, &point, &sphere, DBL_MAX
	};
	if (!(sphere.m_radius >= 0.0) ||
		!m_span_tree->Search(&sphere, NearestSpanBoundCallback, &context))
	    return DBL_MAX;
	return context.nearest;
    }

    bool AnalyticClosest(const ON_3dPoint &point, double *parameter,
	    double *distance) const
    {
	if (!parameter || !distance)
	    return false;
	if (m_analytic_kind == AnalyticKind::Line) {
	    double line_parameter = 0.0;
	    if (!m_line.m_line.ClosestPointTo(point, &line_parameter))
		return false;
	    line_parameter = std::max(0.0, std::min(1.0, line_parameter));
	    *parameter = m_line.Domain().ParameterAt(line_parameter);
	    *distance = point.DistanceTo(m_line.m_line.PointAt(line_parameter));
	    return std::isfinite(*distance);
	}
	if (m_analytic_kind == AnalyticKind::Arc) {
	    double arc_parameter = 0.0;
	    if (!m_arc.m_arc.ClosestPointTo(point, &arc_parameter))
		return false;
	    const ON_Interval arc_domain = m_arc.m_arc.Domain();
	    if (!arc_domain.IsIncreasing())
		return false;
	    *parameter = m_arc.Domain().ParameterAt(
		arc_domain.NormalizedParameterAt(arc_parameter));
	    *distance = point.DistanceTo(m_arc.m_arc.PointAt(arc_parameter));
	    return std::isfinite(*distance);
	}
	return false;
    }

    double RefinedDistance(const ON_3dPoint &point, size_t span,
	    int bracket, double *parameter = NULL,
	    double acceptable_distance = 0.0) const
    {
	const double golden_ratio = 0.5 * (sqrt(5.0) - 1.0);
	const ON_Interval domain(m_spans[span], m_spans[span + 1]);
	double left = domain.ParameterAt(static_cast<double>(bracket) /
	    kCurveClosestBracketsPerSpan);
	double right = domain.ParameterAt(static_cast<double>(bracket + 1) /
	    kCurveClosestBracketsPerSpan);
	/* The bracket chord has already localized the closest curve region.  Use
	 * its projected fraction to seed a safeguarded Newton solve for the
	 * stationary point of squared distance.  Every acceptance test evaluates
	 * the original NURBS, and a failed or ill-conditioned solve falls through
	 * to the unchanged bounded refinement below. */
	if (acceptable_distance > 0.0) {
	    ON_Line chord(m_bracket_points[span][bracket],
		m_bracket_points[span][bracket + 1]);
	    double chord_parameter = 0.0;
	    if (chord.ClosestPointTo(point, &chord_parameter)) {
		chord_parameter = std::max(0.0,
		    std::min(1.0, chord_parameter));
		double seeded_parameter = left +
		    chord_parameter * (right - left);
		const double acceptable_distance_squared =
		    acceptable_distance * acceptable_distance;
		const double maximum_step = 0.5 * (right - left);
		for (int iteration = 0;
			iteration < kCurveClosestNewtonIterations; ++iteration) {
		    double derivatives[9] = {0.0, 0.0, 0.0, 0.0, 0.0,
			0.0, 0.0, 0.0, 0.0};
		    if (!m_nurbs.Evaluate(seeded_parameter, 2, 3,
			    derivatives))
			break;
		    const double dx = derivatives[0] - point.x;
		    const double dy = derivatives[1] - point.y;
		    const double dz = derivatives[2] - point.z;
		    const double distance_squared = dx * dx + dy * dy + dz * dz;
		    if (distance_squared <= acceptable_distance_squared) {
			if (parameter) *parameter = seeded_parameter;
			return sqrt(distance_squared);
		    }
		    const double gradient = dx * derivatives[3] +
			dy * derivatives[4] + dz * derivatives[5];
		    const double hessian = derivatives[3] * derivatives[3] +
			derivatives[4] * derivatives[4] +
			derivatives[5] * derivatives[5] +
			dx * derivatives[6] + dy * derivatives[7] +
			dz * derivatives[8];
		    if (!std::isfinite(gradient) || !std::isfinite(hessian) ||
			    fabs(hessian) <= DBL_EPSILON)
			break;
		    double step_size = gradient / hessian;
		    step_size = std::max(-maximum_step,
			std::min(maximum_step, step_size));
		    const double next_parameter = std::max(left,
			std::min(right, seeded_parameter - step_size));
		    const double progress_floor = DBL_EPSILON * std::max(1.0,
			fabs(seeded_parameter));
		    if (fabs(next_parameter - seeded_parameter) <= progress_floor)
			break;
		    seeded_parameter = next_parameter;
		}
	    }
	}
	double x1 = right - golden_ratio * (right - left);
	double x2 = left + golden_ratio * (right - left);
	double f1 = point.DistanceTo(m_nurbs.PointAt(x1));
	double f2 = point.DistanceTo(m_nurbs.PointAt(x2));
	/* DistanceTo() generally needs only a rigorous yes/no locus test.  An
	 * evaluated source-curve point inside its acceptance tolerance proves yes;
	 * continuing to machine-precision closest distance used to spend all 64
	 * iterations on every already-valid dense pcurve sample.  Calls which need
	 * an actual closest parameter pass zero and retain the full refinement. */
	if (acceptable_distance > 0.0 &&
		std::min(f1, f2) <= acceptable_distance) {
	    if (parameter) *parameter = f1 <= f2 ? x1 : x2;
	    return std::min(f1, f2);
	}
	for (int iteration = 0;
		iteration < kCurveClosestRefinementIterations; ++iteration) {
	    if (f1 > f2) {
		left = x1;
		x1 = x2;
		f1 = f2;
		x2 = left + golden_ratio * (right - left);
		f2 = point.DistanceTo(m_nurbs.PointAt(x2));
	    } else {
		right = x2;
		x2 = x1;
		f2 = f1;
		x1 = right - golden_ratio * (right - left);
		f1 = point.DistanceTo(m_nurbs.PointAt(x1));
	    }
	    if (acceptable_distance > 0.0 &&
		    std::min(f1, f2) <= acceptable_distance) {
		if (parameter) *parameter = f1 <= f2 ? x1 : x2;
		return std::min(f1, f2);
	    }
	}
	if (parameter) *parameter = f1 <= f2 ? x1 : x2;
	return std::min(f1, f2);
    }

    AnalyticKind m_analytic_kind = AnalyticKind::None;
    ON_LineCurve m_line;
    ON_ArcCurve m_arc;
    ON_NurbsCurve m_nurbs;
    std::vector<double> m_spans;
    std::vector<ON_BoundingBox> m_span_bounds;
    ON_BoundingBox m_curve_bounds = ON_BoundingBox::EmptyBoundingBox;
    std::unique_ptr<ON_RTree> m_span_tree;
    std::vector<std::vector<ON_3dPoint> > m_bracket_points;
    mutable bool m_have_previous = false;
    mutable size_t m_previous_span = 0;
    mutable int m_previous_bracket = 0;
};


bool
step_curve_locus_contains_points(const ON_Curve *curve,
    const ON_3dPoint *points, std::size_t point_count, double tolerance,
    std::size_t *rejected_index, double *rejected_distance)
{
    if (rejected_index) *rejected_index = 0;
    if (rejected_distance) *rejected_distance = DBL_MAX;
    if (!curve || (!points && point_count) || !(tolerance > 0.0))
	return false;

    CurveDistanceEvaluator evaluator(curve);
    if (!evaluator.IsValid())
	return false;
    for (std::size_t point = 0; point < point_count; ++point) {
	const double distance = evaluator.DistanceTo(points[point], tolerance);
	if (distance <= tolerance)
	    continue;
	if (rejected_index) *rejected_index = point;
	if (rejected_distance) *rejected_distance = distance;
	return false;
    }
    if (rejected_distance) *rejected_distance = 0.0;
    return true;
}


static bool
step_edges_have_same_locus(const ON_BrepEdge &first,
    const ON_BrepEdge &second, double tolerance)
{
    const ON_Curve *first_curve = first.EdgeCurveOf();
    const ON_Curve *second_curve = second.EdgeCurveOf();
    const ON_Interval first_domain = first.Domain();
    const ON_Interval second_domain = second.Domain();
    if (!first_curve || !second_curve || !first_domain.IsIncreasing() ||
	    !second_domain.IsIncreasing() || !(tolerance > 0.0))
	return false;

    constexpr int kStitchValidationSegments = 64;
    ON_3dPoint first_points[kStitchValidationSegments + 1];
    ON_3dPoint second_points[kStitchValidationSegments + 1];
    for (int sample = 0; sample <= kStitchValidationSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kStitchValidationSegments;
	first_points[sample] = first.PointAt(
	    first_domain.ParameterAt(fraction));
	second_points[sample] = second.PointAt(
	    second_domain.ParameterAt(fraction));
	if (!first_points[sample].IsValid() || !second_points[sample].IsValid())
	    return false;
    }
    /* Validate against each bounded edge proxy, not EdgeCurveOf()'s possibly
     * wider shared curve.  Otherwise a seam fragment and its complete source
     * edge can both appear to have the same underlying locus and be merged. */
    return step_curve_locus_contains_points(&second, first_points,
	kStitchValidationSegments + 1, tolerance) &&
	step_curve_locus_contains_points(&first, second_points,
	    kStitchValidationSegments + 1, tolerance);
}


static double
step_vertex_join_tolerance(const ON_BrepVertex &first,
    const ON_BrepVertex &second, double tolerance)
{
    double first_tolerance = tolerance;
    double second_tolerance = tolerance;
    if (first.m_tolerance > 0.0)
	first_tolerance = std::max(first_tolerance, first.m_tolerance);
    if (second.m_tolerance > 0.0)
	second_tolerance = std::max(second_tolerance, second.m_tolerance);
    return first_tolerance + second_tolerance;
}


static bool
step_vertices_can_stitch(const ON_BrepVertex &first,
    const ON_BrepVertex &second, double tolerance)
{
    const int first_source = first.m_vertex_user.i;
    const int second_source = second.m_vertex_user.i;
    /* Never infer that an importer-created split vertex is an authoritative
     * STEP endpoint.  Two internal vertices may be paired only after their
     * complete parent edge loci have independently matched. */
    if ((first_source > 0) != (second_source > 0) ||
	    (first_source > 0 && first_source != second_source))
	return false;
    return first.point.IsValid() && second.point.IsValid() &&
	first.point.DistanceTo(second.point) <=
	    step_vertex_join_tolerance(first, second, tolerance);
}


bool
step_stitch_face_breps(ON_Brep *brep, double tolerance,
    std::string *failure_reason)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || !(tolerance > 0.0)) {
	if (failure_reason) *failure_reason =
	    "face stitching requires a BREP and positive model tolerance";
	return false;
    }

    /* First collapse duplicated authoritative STEP vertices.  Independent
     * face jobs copy the same exact point, but retain measured local endpoint
     * tolerances when the source violates its declared uncertainty. */
    std::map<int, int> source_vertices;
    for (int vi = 0; vi < brep->m_V.Count(); ++vi) {
	if (brlcad::PullbackWorkCancelled())
	    return false;
	ON_BrepVertex &candidate = brep->m_V[vi];
	const int source_id = candidate.m_vertex_user.i;
	if (candidate.m_vertex_index < 0 || source_id <= 0)
	    continue;
	std::map<int, int>::iterator found = source_vertices.find(source_id);
	if (found == source_vertices.end()) {
	    source_vertices[source_id] = vi;
	    continue;
	}
	ON_BrepVertex &keep = brep->m_V[found->second];
	if (!step_vertices_can_stitch(keep, candidate, tolerance)) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "STEP vertex #" << source_id
		    << " produced face-local points separated by "
		    << keep.point.DistanceTo(candidate.point) << " mm";
		*failure_reason = reason.str();
	    }
	    return false;
	}
	if (!brep->CombineCoincidentVertices(keep, candidate)) {
	    if (failure_reason) *failure_reason =
		"OpenNURBS rejected an identity-proven STEP vertex merge";
	    return false;
	}
    }

    std::map<int, std::vector<int> > source_edges;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_index >= 0 && edge.m_edge_user.i > 0)
	    source_edges[edge.m_edge_user.i].push_back(ei);
    }
    for (std::map<int, std::vector<int> >::const_iterator source =
	    source_edges.begin(); source != source_edges.end(); ++source) {
	const std::vector<int> &edges = source->second;
	for (size_t first_index = 0; first_index < edges.size(); ++first_index) {
	    const int first_ei = edges[first_index];
	    if (first_ei < 0 || first_ei >= brep->m_E.Count() ||
		    brep->m_E[first_ei].m_edge_index < 0)
		continue;
	    for (size_t second_index = first_index + 1;
		    second_index < edges.size(); ++second_index) {
		if (brlcad::PullbackWorkCancelled())
		    return false;
		if (brep->m_E[first_ei].m_edge_index < 0)
		    break;
		const int second_ei = edges[second_index];
		if (second_ei < 0 || second_ei >= brep->m_E.Count() ||
			brep->m_E[second_ei].m_edge_index < 0)
		    continue;
		ON_BrepEdge &first = brep->m_E[first_ei];
		ON_BrepEdge &second = brep->m_E[second_ei];
		double edge_tolerance = tolerance;
		if (first.m_tolerance > 0.0)
		    edge_tolerance = std::max(edge_tolerance,
			first.m_tolerance);
		if (second.m_tolerance > 0.0)
		    edge_tolerance = std::max(edge_tolerance,
			second.m_tolerance);
		if (!step_edges_have_same_locus(first, second, edge_tolerance))
		    continue;

		const bool direct = step_vertices_can_stitch(
		    brep->m_V[first.m_vi[0]], brep->m_V[second.m_vi[0]],
		    tolerance) && step_vertices_can_stitch(
		    brep->m_V[first.m_vi[1]], brep->m_V[second.m_vi[1]],
		    tolerance);
		const bool reversed = !direct && step_vertices_can_stitch(
		    brep->m_V[first.m_vi[0]], brep->m_V[second.m_vi[1]],
		    tolerance) && step_vertices_can_stitch(
		    brep->m_V[first.m_vi[1]], brep->m_V[second.m_vi[0]],
		    tolerance);
		if (!direct && !reversed)
		    continue;
		if (reversed && !second.Reverse()) {
		    if (failure_reason) *failure_reason =
			"an exact STEP edge copy could not be reversed for stitching";
		    return false;
		}
		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    const int keep_vi = first.m_vi[endpoint];
		    const int remove_vi = second.m_vi[endpoint];
		    if (keep_vi == remove_vi)
			continue;
		    if (keep_vi < 0 || remove_vi < 0 ||
			keep_vi >= brep->m_V.Count() ||
			remove_vi >= brep->m_V.Count() ||
			!step_vertices_can_stitch(brep->m_V[keep_vi],
			    brep->m_V[remove_vi], tolerance) ||
			!brep->CombineCoincidentVertices(brep->m_V[keep_vi],
			    brep->m_V[remove_vi])) {
			if (failure_reason) *failure_reason =
			    "an exact STEP edge pair had incompatible endpoint topology";
			return false;
		    }
		}
		if (!brep->CombineCoincidentEdges(first, second)) {
		    if (failure_reason) *failure_reason =
			"OpenNURBS rejected a densely validated STEP edge merge";
		    return false;
		}
		/* CombineCoincidentEdges recomputes a geometric tolerance and may
		 * leave ON_UNSET_VALUE when a face-local singular neighborhood is not
		 * yet a valid whole solid.  The two inputs already carried validated
		 * source tolerances, so retain their conservative maximum until the
		 * ordinary whole-solid tolerance pass can tighten it. */
		ON_BrepEdge *merged = brep->m_E[first_ei].m_edge_index >= 0 ?
		    &brep->m_E[first_ei] :
		    (brep->m_E[second_ei].m_edge_index >= 0 ?
			&brep->m_E[second_ei] : NULL);
		if (!merged) {
		    if (failure_reason) *failure_reason =
			"an identity-proven edge merge deleted both source edges";
		    return false;
		}
		merged->m_tolerance = edge_tolerance;
	    }
	}
    }
    /* Face-local OpenNURBS operations can temporarily call
     * SetEdgeTolerance() before the reciprocal face exists.  OpenNURBS marks
     * that incomplete result ON_UNSET_VALUE.  Reconstruct only those unset
     * values from already accepted trim/vertex tolerances and the measured
     * edge endpoint gaps; whole-solid validation remains authoritative. */
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_index < 0 ||
		(std::isfinite(edge.m_tolerance) && edge.m_tolerance >= 0.0))
	    continue;
	double measured = tolerance;
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    const int vi = edge.m_vi[endpoint];
	    if (vi < 0 || vi >= brep->m_V.Count())
		continue;
	    const ON_BrepVertex &vertex = brep->m_V[vi];
	    if (vertex.m_tolerance > 0.0)
		measured = std::max(measured, vertex.m_tolerance);
	    const ON_3dPoint edge_point = endpoint ? edge.PointAtEnd() :
		edge.PointAtStart();
	    if (edge_point.IsValid() && vertex.point.IsValid())
		measured = std::max(measured,
		    edge_point.DistanceTo(vertex.point));
	}
	for (int use = 0; use < edge.m_ti.Count(); ++use) {
	    const ON_BrepTrim *trim = edge.Trim(use);
	    if (!trim)
		continue;
	    if (trim->m_tolerance[0] > 0.0)
		measured = std::max(measured, trim->m_tolerance[0]);
	    if (trim->m_tolerance[1] > 0.0)
		measured = std::max(measured, trim->m_tolerance[1]);
	}
	edge.m_tolerance = measured;
    }
    brep->Compact();
    return true;
}


static double
distance_to_curve(const ON_Curve *curve, const ON_3dPoint &point)
{
    return CurveDistanceEvaluator(curve).DistanceTo(point);
}


static void
destroy_pullback_data(PBCData *data)
{
    if (!data)
	return;
    if (data->segments) {
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
    }
    delete data;
}


static int
pullback_sample_count(const PBCData *data)
{
    int count = 0;
    if (!data || !data->segments)
	return count;
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (*segment)
	    count += (*segment)->Count();
    }
    return count;
}


static double
short_curve_pullback_resolution(const ON_Curve *curve, double model_tolerance)
{
    const double numerical_floor = ON_ZERO_TOLERANCE * kNumericalToleranceScale;
    if (!curve || !(model_tolerance > numerical_floor))
	return std::max(numerical_floor, model_tolerance * 0.1);

    ON_BoundingBox bbox;
    double feature_size = 0.0;
    if (curve->GetTightBoundingBox(bbox, false, NULL) && bbox.IsValid())
	feature_size = bbox.Diagonal().Length();
    const ON_Interval domain = curve->Domain();
    feature_size = std::max(feature_size,
	curve->PointAt(domain.Min()).DistanceTo(curve->PointAt(domain.Max())));
    if (!(feature_size > numerical_floor))
	return std::max(numerical_floor, model_tolerance * 0.1);

    /* The file uncertainty remains the validation and topology tolerance.
     * This smaller value only prevents the numerical closest-point solver
     * from treating every point of a sub-tolerance feature as the same UV. */
    return std::max(numerical_floor,
	std::min(model_tolerance * 0.1, feature_size * 0.01));
}


static bool
refine_surface_point_seeded(const ON_Surface *surface, const ON_3dPoint &target,
	double convergence_tolerance, ON_3dPoint &uv, double *final_distance,
	double acceptance_tolerance = 0.0,
	bool solve_closed_directions_in_native_domain = false)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() ||
	    !(convergence_tolerance > 0.0))
	return false;
    if (!(acceptance_tolerance > 0.0))
	acceptance_tolerance = convergence_tolerance;

    /* A closed OpenNURBS surface may evaluate parameters outside its native
     * domain, but that evaluation is not required to be the canonical
     * periodic image.  More importantly, Newton iteration outside the native
     * knot range can follow a succession of equivalent local solutions and
     * accumulate many whole periods along a geometrically short STEP edge.
     * Callers which are constructing a new continuous pullback can request a
     * native-domain solve and unwrap the returned solution coherently against
     * the preceding sample afterwards. */
    const auto normalize_closed_parameter = [surface](double parameter,
	    int direction) {
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!domain.IsIncreasing() || !(period > ON_ZERO_TOLERANCE) ||
		!ON_IsValid(parameter))
	    return parameter;
	if (parameter >= domain.Min() && parameter <= domain.Max())
	    return parameter;
	double normalized = domain.Min() + fmod(parameter - domain.Min(), period);
	if (normalized < domain.Min())
	    normalized += period;
	if (normalized > domain.Max())
	    normalized -= period;
	return normalized;
    };
    if (solve_closed_directions_in_native_domain) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (surface->IsClosed(direction))
		uv[direction] = normalize_closed_parameter(uv[direction], direction);
	}
    }

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint point;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, point, du, dv))
	    break;
	const ON_3dVector residual = target - point;
	best_distance = residual.Length();
	if (best_distance <= convergence_tolerance)
	    break;
	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = DBL_EPSILON * metric_scale * 64.0;
	const double aa = a + damping;
	const double cc = c + damping;
	const double determinant = aa * cc - b * b;
	if (fabs(determinant) <= DBL_EPSILON * metric_scale * metric_scale)
	    break;
	double delta[2] = {(cc * r0 - b * r1) / determinant,
	    (aa * r1 - b * r0) / determinant};
	double trust_scale = 1.0;
	for (int direction = 0; direction < 2; ++direction) {
	    const double maximum_step = 0.125 * surface->Domain(direction).Length();
	    if (maximum_step > ON_ZERO_TOLERANCE && fabs(delta[direction]) > maximum_step)
		trust_scale = std::min(trust_scale, maximum_step / fabs(delta[direction]));
	}

	bool improved = false;
	for (int reduction = 0; reduction < 12; ++reduction) {
	    const double scale = trust_scale * std::ldexp(1.0, -reduction);
	    ON_3dPoint candidate(uv.x + scale * delta[0],
		uv.y + scale * delta[1], 0.0);
	    for (int direction = 0; direction < 2; ++direction) {
		if (surface->IsClosed(direction)) {
		    if (solve_closed_directions_in_native_domain)
			candidate[direction] = normalize_closed_parameter(
			    candidate[direction], direction);
		    continue;
		}
		const ON_Interval domain = surface->Domain(direction);
		candidate[direction] = std::max(domain.Min(),
		    std::min(domain.Max(), candidate[direction]));
	    }
	    const ON_3dPoint candidate_lift = surface->PointAt(candidate.x, candidate.y);
	    if (!candidate_lift.IsValid())
		continue;
	    const double candidate_distance = candidate_lift.DistanceTo(target);
	    if (candidate_distance < best_distance) {
		uv = candidate;
		best_distance = candidate_distance;
		improved = true;
		break;
	    }
	}
	if (!improved)
	    break;
    }
    if (final_distance)
	*final_distance = best_distance;
    return best_distance <= acceptance_tolerance;
}


static bool
refine_adjusted_pullback_polyline(PBCData *data,
    const ON_2dPointArray &samples, const ON_3dPoint &failing_uv,
    double tolerance_limit,
    ON_Curve **result, double *measured_tolerance)
{
    if (result) *result = NULL;
    if (measured_tolerance) *measured_tolerance = 0.0;
    if (!data || !data->surf || !data->curve || samples.Count() < 2 ||
	!result || !(tolerance_limit > 0.0))
	return false;

    CurveDistanceEvaluator edge_distance(data->curve);
    if (!edge_distance.IsValid()) return false;
    int interval = 0;
    double interval_distance = DBL_MAX;
    const ON_3dPoint failure_point(failing_uv.x, failing_uv.y, 0.0);
    for (int candidate = 0; candidate < samples.Count() - 1; ++candidate) {
	const ON_3dPoint first(samples[candidate].x, samples[candidate].y, 0.0);
	const ON_3dPoint second(samples[candidate + 1].x,
	    samples[candidate + 1].y, 0.0);
	ON_Line chord(first, second);
	double chord_parameter = 0.0;
	chord.ClosestPointTo(failure_point, &chord_parameter);
	chord_parameter = std::max(0.0, std::min(1.0, chord_parameter));
	const double distance = failure_point.DistanceTo(
	    chord.PointAt(chord_parameter));
	if (distance < interval_distance) {
	    interval_distance = distance;
	    interval = candidate;
	}
    }
    const ON_3dPoint endpoint_lifts[2] = {
	data->surf->PointAt(samples[interval].x, samples[interval].y),
	data->surf->PointAt(samples[interval + 1].x, samples[interval + 1].y)
    };
    double parameters[2] = {0.0, 0.0};
    double accepted_tolerance = std::max(LocalUnits::tolerance,
	data->tolerance);
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	double distance = DBL_MAX;
	if (!edge_distance.ClosestParameter(endpoint_lifts[endpoint],
		&parameters[endpoint], &distance) ||
		distance > tolerance_limit)
	    return false;
	accepted_tolerance = std::max(accepted_tolerance,
	    distance * kMeasuredToleranceSafetyFactor);
    }
    if (accepted_tolerance > tolerance_limit) return false;

    ON_3dPointArray refined;
    refined.Reserve(std::min(static_cast<size_t>(samples.Count()) + 16,
	kMaximumAdjustedPolylineSamples));
    for (int sample = 0; sample <= interval; ++sample)
	refined.Append(ON_3dPoint(samples[sample].x, samples[sample].y, 0.0));
    bool valid = true;
    std::function<void(const ON_2dPoint &, double, const ON_2dPoint &,
	double, int)> refine_interval;
    refine_interval = [data, tolerance_limit, &edge_distance, &refined,
	&accepted_tolerance, &valid, &refine_interval](const ON_2dPoint &uv0,
	double parameter0, const ON_2dPoint &uv1, double parameter1, int depth) {
	if (!valid) return;
	if (brlcad::PullbackWorkCancelled() ||
		static_cast<size_t>(refined.Count()) >=
		    kMaximumAdjustedPolylineSamples) {
	    valid = false;
	    return;
	}
	const ON_2dPoint chord_midpoint(0.5 * (uv0.x + uv1.x),
	    0.5 * (uv0.y + uv1.y));
	const ON_3dPoint chord_lift = data->surf->PointAt(
	    chord_midpoint.x, chord_midpoint.y);
	const double chord_error = edge_distance.DistanceTo(chord_lift,
	    accepted_tolerance);
	if (chord_error <= accepted_tolerance) {
	    refined.Append(ON_3dPoint(uv1.x, uv1.y, 0.0));
	    return;
	}
	if (depth >= kMaximumAdjustedPolylineRefinementDepth) {
	    valid = false;
	    return;
	}

	/* The UV and 3-D curves have independent parameterizations.  Bracket the
	 * source parameters from the two exact endpoint loci, then project their
	 * midpoint onto the surface.  Acceptance remains a curve-locus test. */
	const double midpoint_parameter = 0.5 * (parameter0 + parameter1);
	const ON_3dPoint target = data->curve->PointAt(midpoint_parameter);
	ON_3dPoint midpoint_uv(chord_midpoint.x, chord_midpoint.y, 0.0);
	double projection_distance = DBL_MAX;
	bool projected = refine_surface_point_seeded(data->surf, target,
	    tolerance_limit, midpoint_uv, &projection_distance);
	if (!projected) {
	    if (!data->context)
		data->context = std::make_shared<brlcad::PullbackContext>();
	    ON_2dPoint projected_uv(midpoint_uv.x, midpoint_uv.y);
	    ON_3dPoint projected_lift;
	    projected = data->context->SurfaceClosestPoint(data->surf, target,
		projected_uv, projected_lift, projection_distance, 0,
		tolerance_limit, tolerance_limit);
	    if (projected)
		midpoint_uv.Set(projected_uv.x, projected_uv.y, 0.0);
	}
	if (!projected || projection_distance > tolerance_limit) {
	    valid = false;
	    return;
	}
	for (int direction = 0; direction < 2; ++direction) {
	    if (!data->surf->IsClosed(direction)) continue;
	    const double period = data->surf->Domain(direction).Length();
	    if (period > ON_ZERO_TOLERANCE)
		midpoint_uv[direction] += round((chord_midpoint[direction] -
		    midpoint_uv[direction]) / period) * period;
	}
	const ON_3dPoint midpoint_lift = data->surf->PointAt(
	    midpoint_uv.x, midpoint_uv.y);
	const double midpoint_locus_error = edge_distance.DistanceTo(
	    midpoint_lift, tolerance_limit);
	if (!midpoint_lift.IsValid() || midpoint_locus_error > tolerance_limit) {
	    valid = false;
	    return;
	}
	accepted_tolerance = std::max(accepted_tolerance,
	    midpoint_locus_error * kMeasuredToleranceSafetyFactor);
	if (accepted_tolerance > tolerance_limit) {
	    valid = false;
	    return;
	}
	refine_interval(uv0, parameter0,
	    ON_2dPoint(midpoint_uv.x, midpoint_uv.y), midpoint_parameter,
	    depth + 1);
	refine_interval(ON_2dPoint(midpoint_uv.x, midpoint_uv.y),
	    midpoint_parameter, uv1, parameter1, depth + 1);
    };

    refine_interval(samples[interval], parameters[0], samples[interval + 1],
	parameters[1], 0);
    for (int sample = interval + 2; valid && sample < samples.Count(); ++sample)
	refined.Append(ON_3dPoint(samples[sample].x, samples[sample].y, 0.0));
    ON_PolylineCurve *polyline = valid && refined.Count() >= 2 ?
	new ON_PolylineCurve(refined) : NULL;
    if (!polyline || !polyline->ChangeDimension(2) || !polyline->IsValid()) {
	delete polyline;
	return false;
    }
    *result = polyline;
    if (measured_tolerance) *measured_tolerance = accepted_tolerance;
    return true;
}


static bool
seam_boundary_score(PBCData *data, int direction, double value,
    double tolerance, double *score, std::string *failure = NULL)
{
    if (failure)
	failure->clear();
    if (!data || !data->surf || !data->curve || !data->segments || !score || tolerance <= 0.0)
	return false;

    const ON_Interval domain = data->surf->Domain(direction);
    if (!domain.IsIncreasing())
	return false;
    const CurveDistanceEvaluator edge_distance(data->curve);
    if (!edge_distance.IsValid()) return false;
    ON_3dPoint candidate_min = ON_3dPoint::UnsetPoint;
    ON_3dPoint candidate_max = ON_3dPoint::UnsetPoint;
    bool have_candidate_bounds = false;

    *score = 0.0;
    for (std::list<ON_2dPointArray *>::const_iterator segment = data->segments->begin();
	 segment != data->segments->end(); ++segment) {
	if (!*segment)
	    continue;
	for (int i = 0; i < (*segment)->Count(); ++i) {
	    const ON_2dPoint original = (**segment)[i];
	    ON_2dPoint snapped = original;
	    snapped[direction] = value;
	    const ON_3dPoint lifted = data->surf->PointAt(snapped.x, snapped.y);
	    const double edge_error = lifted.IsValid() ?
		edge_distance.DistanceTo(lifted, tolerance) : DBL_MAX;
	    if (!lifted.IsValid() || edge_error > tolerance) {
		if (failure) {
		    std::ostringstream detail;
		    detail << "sample " << i << " at boundary " << value
			<< " missed the exact edge by " << edge_error;
		    *failure = detail.str();
		}
		return false;
	    }
	    if (!have_candidate_bounds) {
		candidate_min = lifted;
		candidate_max = lifted;
		have_candidate_bounds = true;
	    } else {
		for (int coordinate = 0; coordinate < 3; ++coordinate) {
		    candidate_min[coordinate] = std::min(candidate_min[coordinate],
			lifted[coordinate]);
		    candidate_max[coordinate] = std::max(candidate_max[coordinate],
			lifted[coordinate]);
		}
	    }
	    *score += fabs(original[direction] - value) / domain.Length();
	}
    }
    const ON_BoundingBox source_bounds = data->curve->BoundingBox();
    const double source_scale = source_bounds.IsValid() ?
	source_bounds.Diagonal().Length() : 0.0;
    const double candidate_scale = have_candidate_bounds ?
	candidate_min.DistanceTo(candidate_max) : 0.0;
    /* Preserve explicitly modeled sub-tolerance edges as well.  Model
     * uncertainty permits endpoint snapping; it does not authorize erasing a
     * nonzero topological edge. */
    if (source_scale > ON_ZERO_TOLERANCE && candidate_scale <
	    source_scale * kMinimumSeamCoverageFraction) {
	if (failure) {
	    ON_2dPoint uv_min = ON_2dPoint::UnsetPoint;
	    ON_2dPoint uv_max = ON_2dPoint::UnsetPoint;
	    bool have_uv = false;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    data->segments->begin(); segment != data->segments->end();
		    ++segment) {
		if (!*segment) continue;
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    const ON_2dPoint uv = (**segment)[point];
		    if (!have_uv) {
			uv_min = uv_max = uv;
			have_uv = true;
		    } else {
			uv_min.x = std::min(uv_min.x, uv.x);
			uv_min.y = std::min(uv_min.y, uv.y);
			uv_max.x = std::max(uv_max.x, uv.x);
			uv_max.y = std::max(uv_max.y, uv.y);
		    }
		}
	    }
	    std::ostringstream detail;
	    detail << "boundary coverage " << candidate_scale
		<< " was below the exact edge coverage " << source_scale
		<< " (source UV span " << (have_uv ? uv_max.x - uv_min.x : 0.0)
		<< ':' << (have_uv ? uv_max.y - uv_min.y : 0.0) << ')';
	    *failure = detail.str();
	}
	return false;
    }
    return true;
}


static void
pullback_loop_metrics(const std::list<PBCData *> &pullbacks, PBCData *first, double first_value,
	PBCData *second, double second_value, int direction, double period, double *area, double *gap)
{
    ON_2dPoint initial;
    ON_2dPoint previous;
    bool have_previous = false;
    double twice_area = 0.0;
    *gap = 0.0;
    for (std::list<PBCData *>::const_iterator data = pullbacks.begin(); data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	for (std::list<ON_2dPointArray *>::const_iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (!*segment)
		continue;
	    for (int i = 0; i < (*segment)->Count(); ++i) {
		ON_2dPoint point = (**segment)[i];
		if (*data == first)
		    point[direction] = first_value;
		else if (*data == second)
		    point[direction] = second_value;
		else if (have_previous)
		    point[direction] += round((previous[direction] - point[direction]) / period) * period;
		if (!have_previous) {
		    initial = point;
		    have_previous = true;
		} else {
		    if (i == 0) {
			const double join_gap = previous.DistanceTo(point);
			if (join_gap > *gap)
			    *gap = join_gap;
		    }
		    twice_area += previous.x * point.y - point.x * previous.y;
		}
		previous = point;
	    }
	}
    }
    if (have_previous) {
	twice_area += previous.x * initial.y - initial.x * previous.y;
	const double closure_gap = previous.DistanceTo(initial);
	if (closure_gap > *gap)
	    *gap = closure_gap;
    }
    *area = 0.5 * twice_area;
}


static bool
align_nurbs_surface_seam(std::list<PBCData *> &pullbacks, const ON_Surface *surface,
	double tolerance)
{
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(const_cast<ON_Surface *>(surface));
    if ((!nurbs && !revolution) || !(tolerance > 0.0))
	return false;

    for (std::list<PBCData *>::iterator first = pullbacks.begin(); first != pullbacks.end(); ++first) {
	if (!*first || !(*first)->edge || !(*first)->segments)
	    continue;
	std::list<PBCData *>::iterator second = first;
	++second;
	for (; second != pullbacks.end(); ++second) {
	    if (!*second || (*second)->edge != (*first)->edge || !(*second)->segments)
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		if (!(period > 0.0))
		    continue;
		double values[2] = {0.0, 0.0};
		bool constant[2] = {true, true};
		PBCData *pair[2] = {*first, *second};
		for (int member = 0; member < 2; ++member) {
		    bool have_value = false;
		    for (std::list<ON_2dPointArray *>::const_iterator segment =
			    pair[member]->segments->begin();
			 segment != pair[member]->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point) {
			    const double value = (**segment)[point][direction];
			    if (!have_value) {
				values[member] = value;
				have_value = true;
			    } else if (fabs(value - values[member]) >
				    1.0e-8 * std::max(1.0, period)) {
				constant[member] = false;
			    }
			}
		    }
		    if (!have_value)
			constant[member] = false;
		}
		if (!constant[0] || !constant[1] ||
			fabs(fabs(values[0] - values[1]) - period) >
			    1.0e-7 * std::max(1.0, period))
		    continue;

		const double lower = std::min(values[0], values[1]);
		double seam = std::max(values[0], values[1]);
		while (seam < domain.Min()) seam += period;
		while (seam > domain.Max()) seam -= period;
		if (seam <= domain.Min() + ON_ZERO_TOLERANCE ||
			seam >= domain.Max() - ON_ZERO_TOLERANCE)
		    continue;
		const double shift = seam - lower;
		ON_NurbsSurface nurbs_candidate;
		ON_RevSurface revolution_candidate;
		const ON_Surface *candidate = NULL;
		if (nurbs) {
		    nurbs_candidate = *nurbs;
		    if (!nurbs_candidate.ChangeSurfaceSeam(direction, seam))
			continue;
		    candidate = &nurbs_candidate;
		} else {
		    const int angle_direction = revolution->m_bTransposed ? 1 : 0;
		    if (direction != angle_direction ||
			fabs(revolution->m_angle.Length() - ON_2PI) > ON_ZERO_TOLERANCE)
			continue;
		    revolution_candidate = *revolution;
		    const double angle = revolution->m_angle.ParameterAt(
			domain.NormalizedParameterAt(seam));
		    revolution_candidate.m_angle.Set(angle, angle + ON_2PI);
		    revolution_candidate.m_t.Set(seam, seam + period);
		    candidate = &revolution_candidate;
		}

		bool valid = true;
		for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		     valid && data != pullbacks.end(); ++data) {
		    if (!*data || !(*data)->segments)
			continue;
		    for (std::list<ON_2dPointArray *>::const_iterator segment =
			    (*data)->segments->begin();
			 valid && segment != (*data)->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point) {
			    const ON_2dPoint original = (**segment)[point];
			    ON_2dPoint shifted = original;
			    shifted[direction] += shift;
			    const ON_3dPoint original_lift = surface->PointAt(
				original.x, original.y);
			    const ON_3dPoint shifted_lift = candidate->PointAt(
				shifted.x, shifted.y);
			    if (!original_lift.IsValid() || !shifted_lift.IsValid() ||
				    original_lift.DistanceTo(shifted_lift) > tolerance) {
				valid = false;
				break;
			    }
			}
		    }
		}
		if (!valid)
		    continue;

		if (nurbs)
		    *nurbs = nurbs_candidate;
		else
		    *revolution = revolution_candidate;
		for (std::list<PBCData *>::iterator data = pullbacks.begin();
		     data != pullbacks.end(); ++data) {
		    if (!*data || !(*data)->segments)
			continue;
		    for (std::list<ON_2dPointArray *>::iterator segment =
			    (*data)->segments->begin();
			 segment != (*data)->segments->end(); ++segment) {
			if (!*segment)
			    continue;
			for (int point = 0; point < (*segment)->Count(); ++point)
			    (**segment)[point][direction] += shift;
		    }
		}
		return true;
	    }
	}
    }
    return false;
}


/* A loop that winds around a surface pole cannot be made into a closed
 * Euclidean UV polygon by moving the seam away from its boundary: every seam
 * connecting the two poles must cross the loop.  Put the surface seam exactly
 * at the already proven full-period topology join.  Trim construction can
 * then encode the required zero-area pole cut with two uses of one seam edge
 * and a singular trim. */
static bool
align_surface_seam_with_periodic_loop_cut(
    std::list<PBCData *> &pullbacks, ON_Brep *brep, ON_BrepFace *face,
    const ON_Surface *&surface, double tolerance, STEPWrapper *step = NULL,
    int loop_id = 0, std::string *failure = NULL)
{
    if (failure)
	failure->clear();
    if (!brep || !face || !surface || !(tolerance > 0.0) || pullbacks.empty())
	return false;

    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(
	const_cast<ON_Surface *>(surface));
    if (!nurbs && !revolution)
	return false;

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;
	const int singular_direction = 1 - direction;
	const bool has_singular_side = singular_direction == 0 ?
	    (surface->IsSingular(3) || surface->IsSingular(1)) :
	    (surface->IsSingular(0) || surface->IsSingular(2));
	if (!has_singular_side)
	    continue;

	/* Only a nonzero net winding requires a pole cut.  Measure directed travel
	 * over the validated sample chain rather than subtracting a rounded period
	 * from each edge's two endpoints.  Endpoint-only normalization reverses an
	 * exact half-period edge (round(-0.5) is -1), and it erases the intentional
	 * full-period travel of a closed circular edge.  Consecutive dense samples
	 * are locally unambiguous: normalize only their individual seam jumps, then
	 * accumulate them.  This distinguishes a periodic band from a pole cap
	 * without relying on the curve's parameterization or edge partitioning. */
	double unwrapped_travel = 0.0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments || (*data)->segments->empty())
		continue;
	    double edge_travel = 0.0;
	    double previous_parameter = ON_UNSET_VALUE;
	    bool have_previous = false;
	    const double half_period_guard = kPeriodicParameterSnapFraction *
		std::max(1.0, period);
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		    ++segment) {
		if (!*segment)
		    continue;
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    const double parameter = (**segment)[point][direction];
		    if (have_previous) {
			double delta = parameter - previous_parameter;
			/* A sparse exact semicircle can have only its endpoints.  Its
			 * signed half-period travel is already authoritative; choosing
			 * either periodic image would reverse that direction. */
			if (fabs(fabs(delta) - 0.5 * period) > half_period_guard)
			    delta -= round(delta / period) * period;
			edge_travel += delta;
		    }
		    previous_parameter = parameter;
		    have_previous = true;
		}
	    }
	    const bool topologically_closed_edge = (*data)->edge &&
		(*data)->edge->m_vi[0] >= 0 &&
		(*data)->edge->m_vi[0] == (*data)->edge->m_vi[1];
	    if (step && step->Verbose() && topologically_closed_edge &&
		fabs(edge_travel / period) >= 0.5)
		std::cerr << "EDGE_LOOP #" << loop_id
		    << ": closed STEP edge #" << (*data)->edge->m_edge_user.i
		    << " contributes " << edge_travel / period
		    << " directed periodic turns"
		    << std::endl;
	    unwrapped_travel += edge_travel;
	}
	const double winding = unwrapped_travel / period;
	if (step && step->Verbose() && fabs(winding) >= 0.1)
	    std::cerr << "EDGE_LOOP #" << loop_id
		<< ": measured periodic loop winding " << winding << std::endl;
	if (fabs(winding) < 0.5 ||
		fabs(winding - round(winding)) > kPeriodicParameterSnapFraction) {
	    if (failure && fabs(winding) > 0.1) {
		std::ostringstream reason;
		reason << "the measured periodic loop winding was " << winding
		    << " rather than a nonzero integer";
		*failure = reason.str();
	    }
	    continue;
	}
	if (failure) {
	    std::ostringstream reason;
	    reason << "the loop winds " << round(winding)
		<< " times but no exact full-period topology join was found";
	    *failure = reason.str();
	}

	PBCData *previous = pullbacks.back();
	for (std::list<PBCData *>::iterator current = pullbacks.begin();
		current != pullbacks.end(); ++current) {
	    if (!previous || !previous->segments || previous->segments->empty() ||
		    !previous->segments->back() ||
		    previous->segments->back()->Count() == 0 || !*current ||
		    !(*current)->segments || (*current)->segments->empty() ||
		    !(*current)->segments->front() ||
		    (*current)->segments->front()->Count() == 0) {
		previous = *current;
		continue;
	    }
	    const ON_2dPointArray *previous_segment = previous->segments->back();
	    const ON_2dPoint previous_end =
		(*previous_segment)[previous_segment->Count() - 1];
	    const ON_2dPoint current_start = (*(*current)->segments->front())[0];
	    const double gap = fabs(current_start[direction] -
		previous_end[direction]);
	    if (fabs(gap - period) > kPeriodicParameterSnapFraction *
		    std::max(1.0, period)) {
		previous = *current;
		continue;
	    }
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_end.x, previous_end.y);
	    const ON_3dPoint current_lift = surface->PointAt(
		current_start.x, current_start.y);
	    if (!previous_lift.IsValid() || !current_lift.IsValid() ||
		    previous_lift.DistanceTo(current_lift) > tolerance) {
		previous = *current;
		continue;
	    }

	    const double lower = std::min(previous_end[direction],
		current_start[direction]);
	    double seam = std::max(previous_end[direction],
		current_start[direction]);
	    while (seam < domain.Min()) seam += period;
	    while (seam > domain.Max()) seam -= period;
	    if (!domain.Includes(seam)) {
		previous = *current;
		continue;
	    }
	    /* Reparameterization shifts must be exact integral periods.  Source
	     * pcurve endpoints can disagree by a few measured microradians even
	     * when their 3-D lifts and topology vertex are coincident; snap those
	     * endpoints below, but never bake that source discrepancy into the
	     * surface parameterization itself. */
	    const double shift = round((seam - lower) / period) * period;
	    std::unique_ptr<ON_Surface> candidate;
	    if (nurbs) {
		std::unique_ptr<ON_NurbsSurface> nurbs_candidate(
		    new ON_NurbsSurface(*nurbs));
		if (!nurbs_candidate->ChangeSurfaceSeam(direction, seam) ||
			!nurbs_candidate->IsValid()) {
		    if (failure)
			*failure = "openNURBS rejected the NURBS pole-cut seam";
		    previous = *current;
		    continue;
		}
		candidate.reset(nurbs_candidate.release());
	    } else {
		const int angle_direction = revolution->m_bTransposed ? 1 : 0;
		if (direction != angle_direction ||
			fabs(revolution->m_angle.Length() - ON_2PI) >
			ON_ZERO_TOLERANCE) {
		    if (failure)
			*failure = "the revolution surface angle direction or span was incompatible";
		    previous = *current;
		    continue;
		}
		std::unique_ptr<ON_RevSurface> revolution_candidate(
		    new ON_RevSurface(*revolution));
		const double angle = revolution->m_angle.ParameterAt(
		    domain.NormalizedParameterAt(seam));
		revolution_candidate->m_angle.Set(angle, angle + ON_2PI);
		revolution_candidate->m_t.Set(seam, seam + period);
		if (!revolution_candidate->IsValid()) {
		    if (failure)
			*failure = "openNURBS rejected the revolution pole-cut seam";
		    previous = *current;
		    continue;
		}
		candidate.reset(revolution_candidate.release());
	    }

	    bool valid = true;
	    for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		    valid && data != pullbacks.end(); ++data) {
		if (!*data || !(*data)->segments)
		    continue;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			(*data)->segments->begin(); valid && segment !=
			(*data)->segments->end(); ++segment) {
		    if (!*segment)
			continue;
		    for (int point = 0; point < (*segment)->Count(); ++point) {
			const ON_2dPoint original = (**segment)[point];
			ON_2dPoint normalized = original;
			while (normalized[direction] < domain.Min())
			    normalized[direction] += period;
			while (normalized[direction] > domain.Max())
			    normalized[direction] -= period;
			ON_2dPoint shifted = original;
			shifted[direction] += shift;
			const ON_3dPoint old_lift = surface->PointAt(
			    normalized.x, normalized.y);
			const ON_3dPoint new_lift = candidate->PointAt(
			    shifted.x, shifted.y);
			const double lift_error = old_lift.IsValid() &&
			    new_lift.IsValid() ? old_lift.DistanceTo(new_lift) :
			    DBL_MAX;
			if (lift_error > tolerance) {
			    if (failure) {
				std::ostringstream reason;
				reason << "the pole-cut seam reparameterization changed a sampled lift by "
				    << lift_error << " at " << original.x << ':'
				    << original.y << " (normalized " << normalized.x
				    << ':' << normalized.y << ", shifted " << shifted.x
				    << ':' << shifted.y << ", seam " << seam
				    << ", shift " << shift << ')';
				*failure = reason.str();
			    }
			    valid = false;
			    break;
			}
		    }
		}
	    }
	    if (!valid) {
		previous = *current;
		continue;
	    }

	    ON_Surface *replacement = candidate.release();
	    const int replacement_index = brep->AddSurface(replacement);
	    if (replacement_index < 0) {
		delete replacement;
		if (failure)
		    *failure = "the BREP rejected the pole-cut surface reparameterization";
		return false;
	    }
	    face->m_si = replacement_index;
	    face->SetProxySurface(replacement);
	    surface = replacement;
	    for (std::list<PBCData *>::iterator data = pullbacks.begin();
		    data != pullbacks.end(); ++data) {
		if (!*data || !(*data)->segments)
		    continue;
		(*data)->surf = replacement;
		(*data)->surftree = NULL;
		for (std::list<ON_2dPointArray *>::iterator segment =
			(*data)->segments->begin(); segment !=
			(*data)->segments->end(); ++segment) {
		    if (!*segment)
			continue;
		    const ON_Interval replacement_domain =
			replacement->Domain(direction);
		    const double endpoint_tolerance = kPeriodicParameterSnapFraction *
			std::max(1.0, replacement_domain.Length());
		    for (int point = 0; point < (*segment)->Count(); ++point) {
			(**segment)[point][direction] += shift;
			if (fabs((**segment)[point][direction] -
				replacement_domain.Min()) <= endpoint_tolerance)
			    (**segment)[point][direction] = replacement_domain.Min();
			else if (fabs((**segment)[point][direction] -
				replacement_domain.Max()) <= endpoint_tolerance)
			    (**segment)[point][direction] = replacement_domain.Max();
		    }
		}
	    }

	    /* The new seam makes the selected previous/current topology join the
	     * sole full-period cut.  Rebranch the remaining cyclic chain from that
	     * current edge forward so every other adjacent pcurve is continuous.
	     * Applying one uniform shift above preserves geometry but can merely
	     * move the discontinuity to another edge, causing trim repair to
	     * oscillate forever between two equivalent branches.  Every adjustment
	     * here is an exact integral period on the private closed surface. */
	    std::list<PBCData *>::iterator chain = current;
	    PBCData *chain_previous = NULL;
	    for (size_t position = 0; position < pullbacks.size(); ++position) {
		PBCData *chain_data = *chain;
		if (chain_previous && chain_previous->segments &&
			!chain_previous->segments->empty() &&
			chain_previous->segments->back() &&
			chain_previous->segments->back()->Count() > 0 && chain_data &&
			chain_data->segments && !chain_data->segments->empty() &&
			chain_data->segments->front() &&
			chain_data->segments->front()->Count() > 0) {
		    const ON_2dPointArray *chain_previous_segment =
			chain_previous->segments->back();
		    const double chain_previous_end =
			(*chain_previous_segment)[chain_previous_segment->Count() - 1][direction];
		    const double chain_start =
			(*chain_data->segments->front())[0][direction];
		    const double branch_shift = round(
			(chain_previous_end - chain_start) / period) * period;
		    if (fabs(branch_shift) > ON_ZERO_TOLERANCE) {
			for (std::list<ON_2dPointArray *>::iterator segment =
				chain_data->segments->begin(); segment !=
				chain_data->segments->end(); ++segment) {
			    if (!*segment)
				continue;
			    for (int point = 0; point < (*segment)->Count(); ++point)
				(**segment)[point][direction] += branch_shift;
			}
		    }
		}
		chain_previous = chain_data;
		++chain;
		if (chain == pullbacks.end())
		    chain = pullbacks.begin();
	    }
	    previous->periodic_pole_cut_after = true;
	    (*current)->periodic_pole_cut_before = true;
	    return true;
	}
    }
    return false;
}


/* Move a closed NURBS or full-revolution seam to an exact full-period STEP
 * topology cut.  This
 * face-local reparameterization is deliberately limited to a closed STEP edge
 * whose densely validated pullback winds exactly one surface period.  Moving
 * the seam to an arbitrary empty angular interval can turn ordinary paired
 * edges into invalid OpenNURBS seam trims once the complete solid topology is
 * known.  No sample is accepted unless its 3-D lift remains unchanged within
 * model uncertainty. */
static bool
relocate_surface_seam_away_from_boundary(
    std::list<PBCData *> &pullbacks, ON_Brep *brep, ON_BrepFace *face,
    const ON_Surface *&surface, double tolerance, std::string *failure = NULL)
{
    if (failure)
	failure->clear();

    const ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
    const ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    if (!brep || !face || !surface || (!nurbs && !revolution) ||
	    !(tolerance > 0.0) || pullbacks.empty()) {
	if (failure)
	    *failure = !brep || !face || !surface ?
		"the BREP face or surface is unavailable" :
		((!nurbs && !revolution) ?
		"ordinary seam relocation requires a NURBS or revolution surface" :
		"the pullback set or tolerance is empty");
	return false;
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	/* A full-revolution cylinder or cone has one periodic direction and its
	 * compact two-boundary STEP bands must be cut into explicit OpenNURBS seam
	 * edges after the complete face topology is available.  Relocating that
	 * private surface here makes one boundary look native while hiding the
	 * required paired cut.  The revolution-specific relocation is needed for
	 * a torus, where both directions are closed and an adjacent edge can
	 * otherwise select the antipodal image.  NURBS surfaces retain their
	 * separately validated seam path. */
	if (revolution && !nurbs && !surface->IsClosed(1 - direction)) {
	    if (failure)
		*failure = "singly-periodic revolutions retain their topology-driven seam cut";
	    continue;
	}
	const ON_Interval old_domain = surface->Domain(direction);
	const double period = old_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;
	/* A closed STEP edge may itself wind once around this parameter
	 * direction.  Such a boundary has no empty angular interval: the exact
	 * Euclidean UV representation is obtained by putting the private surface
	 * seam at that edge's declared topology vertex.  Prove the winding from
	 * the ordered, already lift-validated pullback samples before preferring
	 * this cut over the ordinary largest-gap choice below. */
	bool have_topology_cut = false;
	double topology_cut = 0.0;
	std::ostringstream topology_cut_detail;
	const double winding_guard = std::max(ON_ZERO_TOLERANCE,
	    kPeriodicParameterSnapFraction * std::max(1.0, period));
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		!have_topology_cut && data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->edge || !(*data)->segments ||
		    (*data)->edge->m_vi[0] != (*data)->edge->m_vi[1])
		continue;
	    bool have_parameter = false;
	    double first_parameter = 0.0;
	    double previous_parameter = 0.0;
	    double final_parameter = 0.0;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		    ++segment) {
		if (!*segment) continue;
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    double parameter = (**segment)[point][direction];
		    if (!std::isfinite(parameter)) {
			have_parameter = false;
			break;
		    }
		    if (!have_parameter) {
			first_parameter = parameter;
			have_parameter = true;
		    } else {
			parameter += round((previous_parameter - parameter) /
			    period) * period;
		    }
		    previous_parameter = parameter;
		    final_parameter = parameter;
		}
		if (!have_parameter) break;
	    }
	    double travel = final_parameter - first_parameter;
	    /* Seam resolution may have duplicated an already fragmented closed
	     * pcurve, producing a spurious multi-period stored walk.  Establish the
	     * authoritative winding independently from one ordered traversal of the
	     * exact 3-D edge before rejecting the topology-vertex cut. */
	    if (!have_parameter || fabs(fabs(travel) - period) > winding_guard) {
		const ON_Interval curve_domain = (*data)->curve ?
		    (*data)->curve->Domain() : ON_Interval::EmptyInterval;
		const double edge_tolerance = std::max(tolerance,
		    (*data)->tolerance);
		brlcad::PullbackContext context;
		bool dense_valid = curve_domain.IsIncreasing();
		double dense_first = 0.0;
		double dense_previous = 0.0;
		for (int sample = 0; dense_valid &&
			sample <= kDenseLiftValidationSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseLiftValidationSegments;
		    const ON_3dPoint target = (*data)->curve->PointAt(
			curve_domain.ParameterAt(fraction));
		    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint lift;
		    double distance = DBL_MAX;
		    dense_valid = target.IsValid() &&
			context.SurfaceClosestPoint(surface, target, uv, lift,
			    distance, 0, std::max(ON_ZERO_TOLERANCE,
				edge_tolerance * 0.1), edge_tolerance) &&
			distance <= edge_tolerance;
		    if (!dense_valid) break;
		    double parameter = uv[direction];
		    if (sample == 0)
			dense_first = parameter;
		    else
			parameter += round((dense_previous - parameter) /
			    period) * period;
		    dense_previous = parameter;
		}
		if (dense_valid) {
		    first_parameter = dense_first;
		    final_parameter = dense_previous;
		    travel = final_parameter - first_parameter;
		    have_parameter = true;
		}
	    }
	    topology_cut_detail << " closed edge #" << (*data)->edge->m_edge_user.i
		<< " travel=" << travel;
	    if (!have_parameter || fabs(fabs(travel) - period) > winding_guard)
		continue;
	    const int vertex_index = (*data)->edge->m_vi[0];
	    if (vertex_index < 0 || vertex_index >= brep->m_V.Count())
		continue;
	    ON_2dPoint vertex_uv = ON_2dPoint::UnsetPoint;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		    ++segment) {
		if (*segment && (*segment)->Count() > 0) {
		    vertex_uv = (**segment)[0];
		    break;
		}
	    }
	    const ON_3dPoint vertex_lift = vertex_uv.IsValid() ?
		surface->PointAt(vertex_uv.x, vertex_uv.y) :
		ON_3dPoint::UnsetPoint;
	    if (!vertex_lift.IsValid() || vertex_lift.DistanceTo(
		    brep->m_V[vertex_index].point) >
		    std::max(tolerance, (*data)->tolerance))
		continue;
	    double phase = std::fmod(first_parameter - old_domain.Min(), period);
	    if (phase < 0.0) phase += period;
	    topology_cut = old_domain.Min() + phase;
	    have_topology_cut = true;
	}
	std::vector<double> parameters;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		 ++segment) {
		if (!*segment)
		    continue;
		for (int point = 0; point < (*segment)->Count(); ++point) {
		    const double value = (**segment)[point][direction];
		    if (!std::isfinite(value)) {
			parameters.clear();
			break;
		    }
		    double phase = std::fmod(value - old_domain.Min(), period);
		    if (phase < 0.0) phase += period;
		    parameters.push_back(phase);
		}
		if (parameters.empty()) break;
	    }
	    if (parameters.empty()) break;
	}
	/* Relocation is needed only when adjacent topological edges select
	 * opposite copies of the same periodic point.  A jump between samples
	 * inside one edge is an ordinary seam crossing and is handled by the
	 * bounded split/rejoin path without changing the face surface. */
	bool crosses_current_seam = false;
	bool have_first_start = false;
	double first_start = 0.0;
	bool have_previous_end = false;
	double previous_end = 0.0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments || (*data)->segments->empty() ||
		    !(*data)->segments->front() || !(*data)->segments->back() ||
		    (*data)->segments->front()->Count() == 0 ||
		    (*data)->segments->back()->Count() == 0)
		continue;
	    const double start = (*(*data)->segments->front())[0][direction];
	    const ON_2dPointArray *last_segment = (*data)->segments->back();
	    const double end = (*last_segment)[last_segment->Count() - 1][direction];
	    if (!have_first_start) {
		first_start = start;
		have_first_start = true;
	    }
	    if (have_previous_end && fabs(start - previous_end) > 0.5 * period)
		crosses_current_seam = true;
	    previous_end = end;
	    have_previous_end = true;
	}
	if (have_previous_end && have_first_start &&
		fabs(first_start - previous_end) > 0.5 * period)
	    crosses_current_seam = true;
	if (!crosses_current_seam || parameters.size() < 2) {
	    if (failure && failure->empty())
		*failure = "no sampled boundary crosses the current surface seam";
	    continue;
	}

	double seam = topology_cut;
	if (!have_topology_cut) {
	    /* A doubly-closed NURBS face can cross its current seam between two
	     * ordinary STEP edges without containing a closed full-period edge.
	     * In that case the topology-vertex rule above has no candidate.  A
	     * face-private seam in a sampled boundary-free interval is still exact,
	     * provided every source edge is unique in this loop and the complete
	     * remapping below preserves every 3-D lift.  Repeated edges remain the
	     * authoritative indication of an explicit topological seam and are not
	     * eligible for this fallback.  Revolution surfaces retain the stronger
	     * topology-cut requirement used by periodic bands. */
	    if (!nurbs || !surface->IsClosed(1 - direction)) {
		if (failure)
		    *failure = "no closed STEP edge established a full-period topology cut";
		continue;
	    }
	    std::set<int> source_edges;
	    bool unique_edges = true;
	    for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		    data != pullbacks.end(); ++data) {
		if (!*data || !(*data)->edge ||
			!source_edges.insert((*data)->edge->m_edge_index).second) {
		    unique_edges = false;
		    break;
		}
	    }
	    if (!unique_edges) {
		if (failure)
		    *failure = "the loop's repeated STEP edge requires a topology-driven seam";
		continue;
	    }
	    std::sort(parameters.begin(), parameters.end());
	    double largest_gap = -1.0;
	    double gap_start = 0.0;
	    for (size_t index = 0; index < parameters.size(); ++index) {
		const double next = index + 1 < parameters.size() ?
		    parameters[index + 1] : parameters[0] + period;
		const double gap = next - parameters[index];
		if (gap > largest_gap) {
		    largest_gap = gap;
		    gap_start = parameters[index];
		}
	    }
	    if (largest_gap < kMinimumSafeSeamGapFraction * period) {
		if (failure)
		    *failure = "the boundary has no safely empty periodic interval";
		continue;
	    }
	    double seam_phase = std::fmod(gap_start + 0.5 * largest_gap,
		period);
	    if (seam_phase < 0.0)
		seam_phase += period;
	    seam = old_domain.Min() + seam_phase;
	}
	const double endpoint_guard = std::max(ON_ZERO_TOLERANCE,
	    1.0e-10 * period);
	if (seam <= old_domain.Min() + endpoint_guard ||
		seam >= old_domain.Max() - endpoint_guard) {
	    if (failure)
		*failure = "the only empty interval selected an existing domain endpoint";
	    continue;
	}

	ON_NurbsSurface nurbs_candidate;
	ON_RevSurface revolution_candidate;
	const ON_Surface *candidate = NULL;
	double revolution_parameter_shift = 0.0;
	if (nurbs) {
	    nurbs_candidate = *nurbs;
	    if (!nurbs_candidate.Domain(direction).Includes(seam) ||
		    !nurbs_candidate.ChangeSurfaceSeam(direction, seam) ||
		    !nurbs_candidate.IsValid()) {
		if (failure)
		    *failure = "openNURBS rejected the candidate NURBS seam change";
		continue;
	    }
	    candidate = &nurbs_candidate;
	} else {
	    const int angle_direction = revolution->m_bTransposed ? 1 : 0;
	    if (direction != angle_direction ||
		    fabs(revolution->m_angle.Length() - ON_2PI) >
			ON_ZERO_TOLERANCE) {
		if (failure)
		    *failure = "the closed direction was not a full revolution angle";
		continue;
	    }
	    revolution_candidate = *revolution;
	    const double angle = revolution->m_angle.ParameterAt(
		old_domain.NormalizedParameterAt(seam));
	    revolution_candidate.m_angle.Set(angle, angle + ON_2PI);
	    revolution_candidate.m_t.Set(old_domain.Min(), old_domain.Max());
	    if (!revolution_candidate.IsValid()) {
		if (failure)
		    *failure = "openNURBS rejected the candidate revolution seam change";
		continue;
	    }
	    revolution_parameter_shift = old_domain.Min() - seam;
	    candidate = &revolution_candidate;
	}
	const ON_Interval new_domain = candidate->Domain(direction);
	const double new_period = new_domain.Length();
	if (!(new_period > ON_ZERO_TOLERANCE)) {
	    if (failure)
		*failure = "the relocated rational surface has no periodic domain";
	    continue;
	}
	std::vector<ON_2dPointArray> remapped;
	std::list<PBCData *> regenerated_pullbacks;
	bool use_regenerated_pullbacks = false;
	bool valid = true;
	std::string invalid_detail;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     valid && data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    (*data)->segments->begin(); valid && segment !=
		    (*data)->segments->end(); ++segment) {
		if (!*segment) {
		    remapped.push_back(ON_2dPointArray());
		    continue;
		}
		ON_2dPointArray points(**segment);
		std::vector<ON_2dPoint> source_parameters;
		source_parameters.reserve(points.Count());
		for (int point = 0; valid && point < points.Count(); ++point) {
		    const ON_2dPoint original = points[point];
		    ON_2dPoint surface_parameter = original;
		    for (int closed_direction = 0; closed_direction < 2;
			    ++closed_direction) {
			if (!surface->IsClosed(closed_direction))
			    continue;
			const ON_Interval domain = surface->Domain(closed_direction);
			const double domain_period = domain.Length();
			while (surface_parameter[closed_direction] < domain.Min())
			    surface_parameter[closed_direction] += domain_period;
			while (surface_parameter[closed_direction] > domain.Max())
			    surface_parameter[closed_direction] -= domain_period;
		    }
		    ON_2dPoint mapped = surface_parameter;
		    if (revolution)
			mapped[direction] += revolution_parameter_shift;
		    /* Map the ordered path to one continuous periodic branch first.
		     * Wrapping each sample independently can put adjacent points on
		     * opposite copies of the new seam even though that seam lies in a
		     * boundary-free interval. */
		    if (point > 0)
			mapped[direction] += round((points[point - 1][direction] -
			    mapped[direction]) / new_period) * new_period;
		    points[point] = mapped;
		    source_parameters.push_back(surface_parameter);
		}
		if (valid && points.Count() > 0) {
		    double minimum = points[0][direction];
		    double maximum = minimum;
		    for (int point = 1; point < points.Count(); ++point) {
			minimum = std::min(minimum, points[point][direction]);
			maximum = std::max(maximum, points[point][direction]);
		    }
		    const double center = 0.5 * (minimum + maximum);
		    const double branch_shift = round((new_domain.Mid() - center) /
			new_period) * new_period;
		    minimum += branch_shift;
		    maximum += branch_shift;
		    if (minimum < new_domain.Min() - endpoint_guard ||
			    maximum > new_domain.Max() + endpoint_guard) {
			valid = false;
			std::ostringstream detail;
			detail << " (STEP edge #"
			    << ((*data)->edge ? (*data)->edge->m_edge_user.i : -1)
			    << " continuous mapped span " << minimum << ':'
			    << maximum << " did not fit relocated domain "
			    << new_domain.Min() << ':' << new_domain.Max()
			    << " from " << points.Count() << " samples)";
			invalid_detail = detail.str();
		    } else {
			for (int point = 0; point < points.Count(); ++point)
			    points[point][direction] += branch_shift;
		    }
		}
		for (int point = 0; valid && point < points.Count(); ++point) {
		    const ON_3dPoint old_lift = surface->PointAt(
			source_parameters[point].x, source_parameters[point].y);
		    const ON_3dPoint new_lift = candidate->PointAt(
			points[point].x, points[point].y);
		    const double lift_error = old_lift.IsValid() && new_lift.IsValid() ?
			old_lift.DistanceTo(new_lift) : DBL_MAX;
		    valid = lift_error <= tolerance;
		    if (!valid) {
			std::ostringstream detail;
			detail << " (lift error " << lift_error << " at "
			    << (**segment)[point].x << ':' << (**segment)[point].y
			    << " -> " << points[point].x << ':'
			    << points[point].y << ')';
			invalid_detail = detail.str();
		    }
		}
		remapped.push_back(points);
	    }
	}
	if (!valid) {
	    /* Some exporter pcurves oscillate between equivalent copies of the old
	     * seam.  Their raw UV span cannot be translated into one relocated
	     * domain even though the exact 3-D boundary does not cross the new
	     * seam.  Re-pullback those same source curves on the candidate surface,
	     * preserving loop order and orientation, and accept the replacement
	     * only after the standard seam resolver and a complete sample-locus
	     * validation both succeed. */
	    bool regenerated = true;
	    std::string regeneration_failure;
	    for (std::list<PBCData *>::const_iterator original = pullbacks.begin();
		    regenerated && original != pullbacks.end(); ++original) {
		if (!*original || !(*original)->curve) {
		    regeneration_failure = "a source edge was unavailable";
		    regenerated = false;
		    break;
		}
		const double effective_tolerance = std::max(tolerance,
		    (*original)->tolerance);
		PBCData *replacement_data = pullback_samples(candidate,
		    (*original)->curve, effective_tolerance,
		    std::max(effective_tolerance, (*original)->flatness),
		    effective_tolerance, effective_tolerance);
		if (!replacement_data || pullback_sample_count(replacement_data) < 2 ||
			replacement_data->rejected_projection_samples > 0) {
		    std::ostringstream detail;
		    detail << "STEP edge #" << ((*original)->edge ?
			(*original)->edge->m_edge_user.i : -1)
			<< " produced " << pullback_sample_count(replacement_data)
			<< " samples and " << (replacement_data ?
			replacement_data->rejected_projection_samples : 0)
			<< " rejected projections";
		    regeneration_failure = detail.str();
		    destroy_pullback_data(replacement_data);
		    regenerated = false;
		    break;
		}
		replacement_data->edge = (*original)->edge;
		replacement_data->order_reversed = (*original)->order_reversed;
		replacement_data->tolerance = effective_tolerance;
		replacement_data->declared_tolerance =
		    (*original)->declared_tolerance;
		replacement_data->tolerance_adjusted =
		    (*original)->tolerance_adjusted;
		replacement_data->periodic_pole_cut_before =
		    (*original)->periodic_pole_cut_before;
		replacement_data->periodic_pole_cut_after =
		    (*original)->periodic_pole_cut_after;
		if (replacement_data->order_reversed &&
			replacement_data->segments) {
		    std::list<ON_2dPointArray *> reversed_segments;
		    while (!replacement_data->segments->empty()) {
			ON_2dPointArray *samples =
			    replacement_data->segments->front();
			replacement_data->segments->pop_front();
			if (samples) samples->Reverse();
			reversed_segments.push_front(samples);
		    }
		    replacement_data->segments->swap(reversed_segments);
		}
		regenerated_pullbacks.push_back(replacement_data);
	    }
	    if (regenerated && regenerated_pullbacks.size() != pullbacks.size()) {
		regenerated = false;
		regeneration_failure = "the regenerated loop did not retain every STEP edge";
	    }
	for (std::list<PBCData *>::const_iterator data =
		    regenerated_pullbacks.begin(); regenerated &&
		    data != regenerated_pullbacks.end(); ++data) {
		if (!*data || !(*data)->segments || !(*data)->curve) {
		    regeneration_failure = "regenerated pullback data was incomplete";
		    regenerated = false;
		    break;
		}
		/* Curve-locus membership alone does not prove traversal coverage: a
		 * locally stalled closest-point continuation can remain near some
		 * earlier part of the same source curve while omitting its final
		 * interval.  Before committing a private surface replacement, require
		 * the regenerated pcurve to retain both authoritative source-curve
		 * endpoints in STEP traversal order. */
		if ((*data)->segments->empty() ||
			!(*data)->segments->front() || !(*data)->segments->back() ||
			(*data)->segments->front()->Count() < 1 ||
			(*data)->segments->back()->Count() < 1) {
		    regeneration_failure = "a regenerated source edge had no endpoint samples";
		    regenerated = false;
		    break;
		}
		const ON_Interval source_domain = (*data)->curve->Domain();
		const ON_2dPoint &first_uv = (*(*data)->segments->front())[0];
		const ON_2dPointArray *last_segment = (*data)->segments->back();
		const ON_2dPoint &last_uv =
		    (*last_segment)[last_segment->Count() - 1];
		const ON_3dPoint first_lift = candidate->PointAt(first_uv.x,
		    first_uv.y);
		const ON_3dPoint last_lift = candidate->PointAt(last_uv.x,
		    last_uv.y);
		const ON_3dPoint expected_first = (*data)->curve->PointAt(
		    source_domain[(*data)->order_reversed ? 1 : 0]);
		const ON_3dPoint expected_last = (*data)->curve->PointAt(
		    source_domain[(*data)->order_reversed ? 0 : 1]);
		const double endpoint_tolerance = std::max(tolerance,
		    (*data)->tolerance);
		const double first_error = first_lift.IsValid() &&
		    expected_first.IsValid() ? first_lift.DistanceTo(expected_first) :
		    DBL_MAX;
		const double last_error = last_lift.IsValid() &&
		    expected_last.IsValid() ? last_lift.DistanceTo(expected_last) :
		    DBL_MAX;
		if (!source_domain.IsIncreasing() || first_error > endpoint_tolerance ||
			last_error > endpoint_tolerance) {
		    std::ostringstream detail;
		    detail << "regenerated STEP edge #" << ((*data)->edge ?
			(*data)->edge->m_edge_user.i : -1)
			<< " did not retain its source interval endpoints ("
			<< first_error << '/' << last_error << " > "
			<< endpoint_tolerance << ')';
		    regeneration_failure = detail.str();
		    regenerated = false;
		    break;
		}
		const CurveDistanceEvaluator edge_distance((*data)->curve);
		if (!edge_distance.IsValid()) {
		    regeneration_failure = "a regenerated source edge could not be evaluated";
		    regenerated = false;
		    break;
		}
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			(*data)->segments->begin(); regenerated && segment !=
			(*data)->segments->end(); ++segment) {
		    if (!*segment) continue;
		    for (int point = 0; point < (*segment)->Count(); ++point) {
			const ON_3dPoint lift = candidate->PointAt(
			    (**segment)[point].x, (**segment)[point].y);
			if (!lift.IsValid() || edge_distance.DistanceTo(lift,
				    (*data)->tolerance) > (*data)->tolerance) {
			    std::ostringstream detail;
			    detail << "regenerated STEP edge #"
				<< ((*data)->edge ? (*data)->edge->m_edge_user.i : -1)
				<< " failed sample-locus validation at sample "
				<< point;
			    regeneration_failure = detail.str();
			    regenerated = false;
			    break;
			}
		    }
		}
	    }
	    if (!regenerated) {
		while (!regenerated_pullbacks.empty()) {
		    destroy_pullback_data(regenerated_pullbacks.front());
		    regenerated_pullbacks.pop_front();
		}
		if (failure)
		    *failure = "the candidate seam did not preserve every sampled 3-D lift" +
			invalid_detail + "; exact pullback on the relocated surface also failed;" +
			topology_cut_detail.str() + "; " + regeneration_failure;
		continue;
	    }
	    use_regenerated_pullbacks = true;
	}

	/* Give this face a private surface.  Mutating the original in place would
	 * invalidate trims on any previously completed face sharing that surface. */
	ON_Surface *replacement = nurbs ?
	    static_cast<ON_Surface *>(new ON_NurbsSurface(nurbs_candidate)) :
	    static_cast<ON_Surface *>(new ON_RevSurface(revolution_candidate));
	const int replacement_index = brep->AddSurface(replacement);
	if (replacement_index < 0) {
	    delete replacement;
	    while (!regenerated_pullbacks.empty()) {
		destroy_pullback_data(regenerated_pullbacks.front());
		regenerated_pullbacks.pop_front();
	    }
	    if (failure)
		*failure = "the BREP rejected the relocated rational surface";
	    continue;
	}
	face->m_si = replacement_index;
	face->SetProxySurface(replacement);
	surface = replacement;
	if (use_regenerated_pullbacks) {
	    std::list<PBCData *>::iterator original = pullbacks.begin();
	    std::list<PBCData *>::iterator regenerated =
		regenerated_pullbacks.begin();
	    for (; original != pullbacks.end() && regenerated !=
		    regenerated_pullbacks.end(); ++original, ++regenerated) {
		destroy_pullback_data(*original);
		*original = *regenerated;
	    }
	    regenerated_pullbacks.clear();
	}
	size_t remapped_index = 0;
	for (std::list<PBCData *>::iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments)
		continue;
	    (*data)->surf = replacement;
	    (*data)->surftree = NULL;
	    for (std::list<ON_2dPointArray *>::iterator segment =
		    (*data)->segments->begin(); segment != (*data)->segments->end();
		    ++segment, ++remapped_index) {
		if (!use_regenerated_pullbacks && *segment &&
			remapped_index < remapped.size())
		    **segment = remapped[remapped_index];
	    }
	}
	return true;
    }
    if (failure && failure->empty())
	*failure = "the surface has no closed parameter direction";
    return false;
}


static bool
pullback_requires_singular_topology_split(const PBCData *data,
	double tolerance)
{
    if (!data || !data->curve || !data->surf || !data->segments ||
	    data->segments->size() < 2 || !(tolerance > 0.0))
	return false;

    const CurveDistanceEvaluator source_distance(data->curve);
    if (!source_distance.IsValid())
	return false;

    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (!*segment || (*segment)->Count() < 1)
	    continue;
	/* Ordinary periodic seam fragmentation must retain the original STEP
	 * edge topology.  Only a fragment ending on an actual degenerate
	 * surface boundary is allowed to select the exact subedge construction
	 * below.  The fragment list need not be stored in source-curve order, so
	 * inspect its endpoints independently rather than assuming adjacent list
	 * entries meet at the singularity. */
	const ON_2dPoint endpoints[2] = {
	    (**segment)[0], (**segment)[(*segment)->Count() - 1]
	};
	for (int endpoint = 0; endpoint < 2; ++endpoint) {
	    if (IsAtSingularity(data->surf, endpoints[endpoint],
		    PBC_SEAM_TOL) < 0)
		continue;
	    const ON_3dPoint lift = data->surf->PointAt(
		endpoints[endpoint].x, endpoints[endpoint].y);
	    /* The singular endpoint must also lie on the source STEP edge.  The
	     * subsequent split routine densely proves each complete fragment. */
	    if (lift.IsValid() &&
		    source_distance.DistanceTo(lift, tolerance) <= tolerance)
		return true;
	}
    }
    return false;
}


/* A native-periodic split can land within the asserted uncertainty of an
 * authoritative STEP edge endpoint.  In that case the split may create a
 * tiny first or last UV fragment whose complete 3-D lift is indistinguishable
 * from that endpoint.  It is not a separate topological edge: retaining it
 * asks the subedge builder to manufacture a zero-length interval, while
 * rejecting it loses an otherwise exact solid.
 *
 * Remove only outer fragments, and prove the complete piece densely against
 * the original STEP vertex.  An interior fragment, a closed excursion, or a
 * fragment whose surface lift leaves the vertex tolerance is never eligible.
 * The caller enables this only for non-exact safe repair. */
static size_t
remove_subtolerance_outer_split_fragments(PBCData *data,
	const ON_Brep *brep, double tolerance)
{
    if (!data || !data->segments || data->segments->size() < 2 ||
	!data->surf || !data->curve || !data->edge || !brep ||
	!(tolerance > 0.0))
	return 0;

    const CurveDistanceEvaluator source_distance(data->curve);
    if (!source_distance.IsValid())
	return 0;

    const auto fragment_collapses_to_vertex = [&](const ON_2dPointArray *samples,
	int vertex_index) {
	if (!samples || samples->Count() < 2 || vertex_index < 0 ||
		vertex_index >= brep->m_V.Count())
	    return false;
	const ON_3dPoint vertex = brep->m_V[vertex_index].point;
	if (!vertex.IsValid() ||
		source_distance.DistanceTo(vertex, tolerance) > tolerance)
	    return false;

	ON_3dPointArray uv_points;
	uv_points.Reserve(samples->Count());
	for (int point = 0; point < samples->Count(); ++point)
	    uv_points.Append(ON_3dPoint((*samples)[point].x,
		(*samples)[point].y, 0.0));
	ON_PolylineCurve fragment(uv_points);
	if (!fragment.ChangeDimension(2) || !fragment.IsValid() ||
		!fragment.Domain().IsIncreasing())
	    return false;
	const ON_Interval domain = fragment.Domain();
	for (int sample = 0; sample <= kDenseLiftValidationSegments; ++sample) {
	    const ON_3dPoint uv = fragment.PointAt(domain.ParameterAt(
		static_cast<double>(sample) / kDenseLiftValidationSegments));
	    const ON_3dPoint lift = uv.IsValid() ?
		data->surf->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
	    if (!lift.IsValid() || lift.DistanceTo(vertex) > tolerance ||
		    source_distance.DistanceTo(lift, tolerance) > tolerance)
		return false;
	}
	return true;
    };

    size_t removed = 0;
    const int traversal_start_vertex = data->edge->m_vi[
	data->order_reversed ? 1 : 0];
    const int traversal_end_vertex = data->edge->m_vi[
	data->order_reversed ? 0 : 1];
    while (data->segments->size() > 1 &&
	fragment_collapses_to_vertex(data->segments->front(),
	    traversal_start_vertex)) {
	delete data->segments->front();
	data->segments->pop_front();
	++removed;
    }
    while (data->segments->size() > 1 &&
	fragment_collapses_to_vertex(data->segments->back(),
	    traversal_end_vertex)) {
	delete data->segments->back();
	data->segments->pop_back();
	++removed;
    }
    return removed;
}


static bool
refined_fragment_polyline(PBCData *data, const ON_2dPointArray &samples,
	double tolerance, ON_Curve **result, std::string *failure,
	bool complete_edge = false, double *measured_error = NULL)
{
    if (result) *result = NULL;
    if (failure) failure->clear();
	if (measured_error) *measured_error = 0.0;
    if (!data || !data->curve || !data->surf || samples.Count() < 2 ||
	    !(tolerance > 0.0) || !result)
	return false;

    const CurveDistanceEvaluator source_distance(data->curve);
    const ON_3dPoint start_lift = data->surf->PointAt(samples[0].x,
	samples[0].y);
    const ON_3dPoint end_lift = data->surf->PointAt(
	samples[samples.Count() - 1].x, samples[samples.Count() - 1].y);
    double start_parameter = 0.0;
    double end_parameter = 0.0;
    double start_distance = DBL_MAX;
    double end_distance = DBL_MAX;
    if (complete_edge) {
	const ON_Interval source_domain = data->curve->Domain();
	start_parameter = source_domain[data->order_reversed ? 1 : 0];
	end_parameter = source_domain[data->order_reversed ? 0 : 1];
    } else if (!source_distance.ClosestParameter(start_lift, &start_parameter,
		&start_distance) ||
	    !source_distance.ClosestParameter(end_lift, &end_parameter,
		&end_distance) || start_distance > tolerance ||
	    end_distance > tolerance) {
	if (failure) *failure = "fragment endpoints did not locate an exact source interval";
	return false;
    }

    if (!data->context)
	data->context = std::make_shared<brlcad::PullbackContext>();
    /* Geometric acceptance remains the model-derived tolerance, but using
     * that same value as the closest-point convergence threshold collapses a
     * real edge whenever the entire feature is shorter than the file
     * uncertainty.  Resolve the UV path at a feature-relative scale and then
     * validate its lift independently against the unchanged acceptance
     * tolerance. */
    const double projection_tolerance = short_curve_pullback_resolution(
	data->curve, tolerance);
    ON_3dPointArray points;
    points.Reserve(kDenseLiftValidationSegments + 1);
	std::vector<double> point_parameters;
	point_parameters.reserve(kDenseLiftValidationSegments + 1);
	double maximum_error = 0.0;
    ON_3dPoint previous(samples[0].x, samples[0].y, 0.0);
    for (int sample = 0; sample <= kDenseLiftValidationSegments; ++sample) {
	if (brlcad::PullbackWorkCancelled()) {
	    if (failure) *failure = "fragment refinement was cancelled";
	    return false;
	}
	const double fraction = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const ON_3dPoint target = data->curve->PointAt((1.0 - fraction) *
	    start_parameter + fraction * end_parameter);
	ON_3dPoint continuity_seed = previous;
	ON_3dPoint uv = continuity_seed;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	bool projected = false;
	/* A fragment already has validated STEP-edge endpoints.  Prefer those
	 * supplied endpoint images only when their nearest periodic branch still
	 * lifts to the authoritative curve endpoint.  Otherwise solve that endpoint
	 * again in the native surface domain; blindly translating an exact but
	 * distant extrapolated image can turn the last UV chord into several false
	 * surface revolutions. */
	if (sample == 0 || sample == kDenseLiftValidationSegments) {
	    const ON_2dPoint supplied = sample == 0 ? samples[0] :
		samples[samples.Count() - 1];
	    uv.Set(supplied.x, supplied.y, 0.0);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!data->surf->IsClosed(direction))
		    continue;
		const double period = data->surf->Domain(direction).Length();
		if (period > ON_ZERO_TOLERANCE)
		    uv[direction] += round((previous[direction] -
			uv[direction]) / period) * period;
	    }
	    lift = data->surf->PointAt(uv.x, uv.y);
	    distance = lift.IsValid() && target.IsValid() ?
		lift.DistanceTo(target) : DBL_MAX;
	    projected = distance <= tolerance;
	}
	if (!projected) {
	    if (sample == 0)
		uv.Set(samples[0].x, samples[0].y, 0.0);
	    else
		uv = continuity_seed;
	    projected = refine_surface_point_seeded(data->surf, target,
		projection_tolerance, uv, &distance, tolerance, true);
	    /* Independently supplied closed spline boundaries can differ at their
	     * nominally equivalent native seam images by slightly more than the
	     * declared uncertainty.  Prefer the native solve so ordinary edges
	     * cannot accumulate spurious whole periods, but if that image does not
	     * represent the source point, retain an exact continuation from the
	     * preceding already validated unwrapped sample.  Its completed locus is
	     * checked below, so an extrapolated but non-equivalent branch cannot be
	     * accepted merely because Newton converged. */
	    if (!projected) {
		ON_3dPoint continued = previous;
		double continued_distance = DBL_MAX;
		if (refine_surface_point_seeded(data->surf, target,
			projection_tolerance, continued, &continued_distance,
			tolerance)) {
		    uv = continued;
		    distance = continued_distance;
		    projected = true;
		}
	    }
	    if (!projected) {
		ON_2dPoint projected_uv(uv.x, uv.y);
		projected = data->context->SurfaceClosestPoint(data->surf, target,
		    projected_uv, lift, distance, 0, projection_tolerance,
		    tolerance);
		if (projected)
		    uv.Set(projected_uv.x, projected_uv.y, 0.0);
	    }
	}
	if (!projected) {
	    maximum_error = std::max(maximum_error, distance);
	    if (measured_error) *measured_error = maximum_error;
	    if (failure) *failure = "a dense exact subedge sample did not project to the surface";
	    return false;
	}
	for (int direction = 0; direction < 2; ++direction) {
	    if (!data->surf->IsClosed(direction)) continue;
	    const double period = data->surf->Domain(direction).Length();
	    if (period > ON_ZERO_TOLERANCE)
		uv[direction] += round((previous[direction] - uv[direction]) /
		    period) * period;
	}
	lift = data->surf->PointAt(uv.x, uv.y);
	const double lift_error = target.IsValid() && lift.IsValid() ?
	    lift.DistanceTo(target) : DBL_MAX;
	maximum_error = std::max(maximum_error, lift_error);
	if (!target.IsValid() || !lift.IsValid() || lift_error > tolerance) {
	    if (measured_error) *measured_error = maximum_error;
	    if (failure) {
		std::ostringstream details;
		details << "dense subedge projection missed the source by "
		    << lift.DistanceTo(target) << " at sample " << sample
		    << " (uv " << uv.x << ':' << uv.y << ", previous "
		    << previous.x << ':' << previous.y << ", supplied "
		    << (sample == 0 ? samples[0].x :
			(sample == kDenseLiftValidationSegments ?
			 samples[samples.Count() - 1].x : ON_UNSET_VALUE))
		    << ':' << (sample == 0 ? samples[0].y :
			(sample == kDenseLiftValidationSegments ?
			 samples[samples.Count() - 1].y : ON_UNSET_VALUE)) << ')';
		*failure = details.str();
	    }
	    return false;
	}
	if (points.Count() == 0 || uv.DistanceTo(points[points.Count() - 1]) >
		ON_ZERO_TOLERANCE) {
	    points.Append(uv);
	    point_parameters.push_back((1.0 - fraction) * start_parameter +
		fraction * end_parameter);
	}
	previous = uv;
    }
	/* A polyline with exactly 1024 intervals would otherwise be sampled only
	 * at its 1025 vertices by the equally sized final validation budget.  Prove
	 * every represented UV chord at its midpoint before the proposal can reach
	 * BREP mutation.  The points were generated at known source parameters, so
	 * this is a direct edge/surface consistency check rather than a closest-
	 * locus approximation. */
	for (int interval = 0; interval + 1 < points.Count(); ++interval) {
	    const ON_3dPoint midpoint_uv = 0.5 * (points[interval] +
		points[interval + 1]);
	    const double midpoint_parameter = 0.5 * (
		point_parameters[static_cast<size_t>(interval)] +
		point_parameters[static_cast<size_t>(interval + 1)]);
	    const ON_3dPoint midpoint_lift = data->surf->PointAt(
		midpoint_uv.x, midpoint_uv.y);
	    const ON_3dPoint midpoint_target = data->curve->PointAt(
		midpoint_parameter);
	    const double midpoint_error = midpoint_lift.IsValid() &&
		midpoint_target.IsValid() ?
		midpoint_lift.DistanceTo(midpoint_target) : DBL_MAX;
	    maximum_error = std::max(maximum_error, midpoint_error);
	    if (midpoint_error > tolerance) {
		if (failure) {
		    std::ostringstream details;
		    details << "dense subedge UV chord missed the source by "
			<< midpoint_error << " in interval " << interval;
		    *failure = details.str();
		}
		if (measured_error) *measured_error = maximum_error;
		return false;
	    }
	}
    ON_PolylineCurve *polyline = points.Count() >= 2 ?
	new ON_PolylineCurve(points) : NULL;
    if (!polyline || !polyline->ChangeDimension(2) || !polyline->IsValid()) {
	delete polyline;
	if (failure) *failure = "dense fragment polyline construction failed";
	return false;
    }
	if (measured_error) *measured_error = maximum_error;
    *result = polyline;
    return true;
}


static bool
step_fragment_boundary_parameter(PBCData *data,
	const ON_2dPointArray &left, const ON_2dPointArray &right,
	double tolerance, double *parameter)
{
    if (parameter) *parameter = ON_UNSET_VALUE;
    if (!data || !data->curve || !data->surf || !parameter ||
	    !(tolerance > 0.0) || left.Count() < 2 || right.Count() < 2)
	return false;

    const ON_Interval source_domain = data->curve->Domain();
    const CurveDistanceEvaluator source_distance(data->curve);
    if (!source_domain.IsIncreasing() || !source_distance.IsValid())
	return false;

    const ON_2dPointArray *interior = &right;
    int first_index = 1;
    int second_index = 2;
    if (right.Count() < 3 && left.Count() >= 3) {
	interior = &left;
	first_index = left.Count() - 2;
	second_index = left.Count() - 3;
    }
    if (second_index < 0 || second_index >= interior->Count())
	return false;

    double first_parameter = 0.0;
    double second_parameter = 0.0;
    double first_distance = DBL_MAX;
    double second_distance = DBL_MAX;
    const ON_3dPoint first_lift = data->surf->PointAt(
	(*interior)[first_index].x, (*interior)[first_index].y);
    const ON_3dPoint second_lift = data->surf->PointAt(
	(*interior)[second_index].x, (*interior)[second_index].y);
    if (!first_lift.IsValid() || !second_lift.IsValid() ||
	    !source_distance.ClosestParameter(first_lift, &first_parameter,
		&first_distance) ||
	    !source_distance.ClosestParameter(second_lift, &second_parameter,
		&second_distance) || first_distance > tolerance ||
	    second_distance > tolerance)
	return false;

    const double predicted = 2.0 * first_parameter - second_parameter;
    const double parameter_step = fabs(first_parameter - second_parameter);
    const double window = std::max(parameter_step * 8.0,
	source_domain.Length() * 1.0e-8);
    const ON_Interval local_domain(
	std::max(source_domain.Min(), predicted - window),
	std::min(source_domain.Max(), predicted + window));
    if (!local_domain.IsIncreasing())
	return false;

    std::unique_ptr<ON_Curve> local_curve(data->curve->DuplicateCurve());
    if (!local_curve || !local_curve->Trim(local_domain))
	return false;
    const CurveDistanceEvaluator local_distance(local_curve.get());
    const ON_2dPoint boundary_uv = right[0];
    const ON_3dPoint boundary_lift = data->surf->PointAt(
	boundary_uv.x, boundary_uv.y);
    double boundary_distance = DBL_MAX;
    if (!boundary_lift.IsValid() ||
	    !local_distance.ClosestParameter(boundary_lift, parameter,
		&boundary_distance) || boundary_distance > tolerance) {
	*parameter = ON_UNSET_VALUE;
	return false;
    }
    return true;
}


static bool
split_pullback_segment_edge(ON_Brep *brep, PBCData *data,
	const ON_Curve *pcurve, const ON_2dPointArray &samples,
	double *tolerance, bool allow_tolerance_adjustment,
	double tolerance_adjustment_limit, bool first_fragment,
	bool last_fragment, double start_parameter_hint,
	double end_parameter_hint, ON_BrepEdge **segment_edge,
	bool *trim_reversed, std::string *failure)
{
    if (failure)
	failure->clear();
    if (segment_edge)
	*segment_edge = NULL;
    if (trim_reversed)
	*trim_reversed = false;
    if (!brep || !data || !data->edge || !data->curve || !data->surf ||
	    !pcurve || samples.Count() < 2 || !tolerance || !(*tolerance > 0.0) ||
	    !segment_edge || !trim_reversed)
	return false;

    double effective_tolerance = *tolerance;

    ON_NurbsCurve source_nurbs;
    const CurveDistanceEvaluator source_distance(data->curve);
    if (!data->curve->GetNurbForm(source_nurbs) ||
	    !source_distance.IsValid()) {
	if (failure) *failure = "source edge NURBS conversion failed";
	return false;
    }
    const ON_Interval pcurve_domain = pcurve->Domain();
    double maximum_source_distance = 0.0;
    bool complete_source_locus = pcurve_domain.IsIncreasing();
    for (int sample = 0; complete_source_locus &&
	    sample <= kDenseLiftValidationSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const ON_3dPoint uv = pcurve->PointAt(
	    pcurve_domain.ParameterAt(fraction));
	const ON_3dPoint lift = uv.IsValid() ?
	    data->surf->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
	const double distance = lift.IsValid() ?
	    source_distance.DistanceTo(lift, effective_tolerance) : DBL_MAX;
	maximum_source_distance = std::max(maximum_source_distance, distance);
	complete_source_locus = std::isfinite(distance);
    }
    if (complete_source_locus &&
	    maximum_source_distance > effective_tolerance) {
	const double measured_tolerance = maximum_source_distance *
	    kMeasuredToleranceSafetyFactor;
	if (allow_tolerance_adjustment &&
		measured_tolerance <= tolerance_adjustment_limit)
	    effective_tolerance = measured_tolerance;
    }
    const ON_3dPoint loop_start = data->surf->PointAt(samples[0].x,
	samples[0].y);
    const ON_3dPoint loop_end = data->surf->PointAt(
	samples[samples.Count() - 1].x, samples[samples.Count() - 1].y);
    double start_parameter = 0.0;
    double end_parameter = 0.0;
    double start_distance = DBL_MAX;
    double end_distance = DBL_MAX;
    if (!loop_start.IsValid() || !loop_end.IsValid() ||
	    !source_distance.ClosestParameter(loop_start, &start_parameter,
		&start_distance) ||
	    !source_distance.ClosestParameter(loop_end, &end_parameter,
		&end_distance) ||
	    start_distance > effective_tolerance ||
	    end_distance > effective_tolerance) {
	if (failure) {
	    std::ostringstream details;
	    details << "fragment endpoints did not project onto the source edge: "
		<< start_distance << '/' << end_distance << " (uv "
		<< samples[0].x << ':' << samples[0].y << " -> "
		<< samples[samples.Count() - 1].x << ':'
		<< samples[samples.Count() - 1].y << ", singular "
		<< IsAtSingularity(data->surf, samples[0], PBC_SEAM_TOL) << '/'
		<< IsAtSingularity(data->surf,
		    samples[samples.Count() - 1], PBC_SEAM_TOL)
		<< ", fragments " << data->segments->size() << ')';
	    *failure = details.str();
	}
	return false;
    }
    /* Surface-seam fragments retain their order along the original STEP edge.
	 * On an almost-closed open curve, an endpoint close to both ends can make a
	 * global closest-point solve select parameter 0 for the final endpoint at
	 * parameter 1 (or vice versa).  Use the immutable edge traversal to resolve
	 * only the outer fragment endpoints, and require their actual 3-D points to
	 * satisfy the same model-space tolerance before overriding the ambiguous
	 * parameter. */
    const ON_Interval source_domain = data->curve->Domain();
    if (source_domain.IsIncreasing() && first_fragment) {
	const double expected_parameter = data->order_reversed ?
	    source_domain.Max() : source_domain.Min();
	const ON_3dPoint expected = data->curve->PointAt(expected_parameter);
	if (expected.IsValid() &&
		loop_start.DistanceTo(expected) <= effective_tolerance) {
	    start_parameter = expected_parameter;
	    start_distance = loop_start.DistanceTo(expected);
	}
    }
    if (source_domain.IsIncreasing() && last_fragment) {
	const double expected_parameter = data->order_reversed ?
	    source_domain.Min() : source_domain.Max();
	const ON_3dPoint expected = data->curve->PointAt(expected_parameter);
	if (expected.IsValid() &&
		loop_end.DistanceTo(expected) <= effective_tolerance) {
	    end_parameter = expected_parameter;
	    end_distance = loop_end.DistanceTo(expected);
	}
    }
    if (source_domain.Includes(start_parameter_hint)) {
	const ON_3dPoint hinted = data->curve->PointAt(start_parameter_hint);
	const double hinted_distance = hinted.IsValid() ?
	    loop_start.DistanceTo(hinted) : DBL_MAX;
	if (hinted_distance <= effective_tolerance) {
	    start_parameter = start_parameter_hint;
	    start_distance = hinted_distance;
	}
    }
    if (source_domain.Includes(end_parameter_hint)) {
	const ON_3dPoint hinted = data->curve->PointAt(end_parameter_hint);
	const double hinted_distance = hinted.IsValid() ?
	    loop_end.DistanceTo(hinted) : DBL_MAX;
	if (hinted_distance <= effective_tolerance) {
	    end_parameter = end_parameter_hint;
	    end_distance = hinted_distance;
	}
    }
    /* A surface-seam split can also put an internal fragment endpoint at the
     * source curve's own nearly-closed join.  In that case both source-domain
     * ends may satisfy a scale-bounded closest-point search.  Inspect one
     * dense interior pcurve sample and select a domain end only when that
     * sample independently lies in the adjacent 4/1024 of the same source
     * end.  This recovers traversal identity without assuming that the open
     * source curve is topologically closed. */
    const auto align_ambiguous_domain_endpoint = [&](bool at_start,
	    double *parameter, double *distance) {
	if (!source_domain.IsIncreasing() || !parameter || !distance ||
		!pcurve_domain.IsIncreasing())
	    return;
	const double interior_fraction = at_start ?
	    1.0 / kDenseLiftValidationSegments :
	    1.0 - 1.0 / kDenseLiftValidationSegments;
	const ON_3dPoint interior_uv = pcurve->PointAt(
	    pcurve_domain.ParameterAt(interior_fraction));
	const ON_3dPoint interior_lift = interior_uv.IsValid() ?
	    data->surf->PointAt(interior_uv.x, interior_uv.y) :
	    ON_3dPoint::UnsetPoint;
	double interior_parameter = ON_UNSET_VALUE;
	double interior_distance = DBL_MAX;
	if (!interior_lift.IsValid() ||
		!source_distance.ClosestParameter(interior_lift,
		    &interior_parameter, &interior_distance) ||
		interior_distance > effective_tolerance)
	    return;
	const double source_length = source_domain.Length();
	const double boundary_parameter_window = source_length * 4.0 /
	    kDenseLiftValidationSegments;
	double candidate_parameter = ON_UNSET_VALUE;
	if (fabs(interior_parameter - source_domain.Min()) <=
		boundary_parameter_window)
	    candidate_parameter = source_domain.Min();
	else if (fabs(interior_parameter - source_domain.Max()) <=
		boundary_parameter_window)
	    candidate_parameter = source_domain.Max();
	if (!ON_IsValid(candidate_parameter) ||
		fabs(candidate_parameter - *parameter) <=
		    source_length * 1.0e-12)
	    return;
	const ON_3dPoint endpoint = at_start ? loop_start : loop_end;
	const ON_3dPoint candidate = data->curve->PointAt(candidate_parameter);
	const double candidate_distance = endpoint.IsValid() && candidate.IsValid() ?
	    endpoint.DistanceTo(candidate) : DBL_MAX;
	if (candidate_distance <= effective_tolerance) {
	    *parameter = candidate_parameter;
	    *distance = candidate_distance;
	}
    };
    align_ambiguous_domain_endpoint(true, &start_parameter, &start_distance);
    align_ambiguous_domain_endpoint(false, &end_parameter, &end_distance);
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	data->curve->Domain().Length() * 1.0e-12);
    if (fabs(start_parameter - end_parameter) <= parameter_guard) {
	if (failure) {
	    std::ostringstream details;
	    details << "fragment endpoints selected the same source parameter "
		<< start_parameter << '/' << end_parameter << " in domain "
		<< data->curve->Domain().Min() << ':'
		<< data->curve->Domain().Max() << "; endpoint lift separation "
		<< loop_start.DistanceTo(loop_end) << "; projection errors "
		<< start_distance << '/' << end_distance;
	    *failure = details.str();
	}
	return false;
    }

    int rejected_sample = -1;
    double rejected_distance = DBL_MAX;
    double rejected_source_distance = DBL_MAX;
    double rejected_source_parameter = ON_UNSET_VALUE;
    std::string interval_rejection;
    /* Prove that the complete emitted pcurve fragment belongs to a proposed
	 * exact 3-D subedge.  A closed source curve has two parameter intervals
	 * between the same endpoints.  The numerically increasing interval is not
	 * necessarily the one traversed by a pcurve crossing the source curve's
	 * own seam, so validate both loci before choosing topology. */
    const auto exact_fragment_interval = [&](const ON_Curve *candidate,
	    bool *reversed) {
	if (!candidate || !reversed || !candidate->IsValid() ||
		!pcurve_domain.IsIncreasing()) {
	    interval_rejection = "candidate source interval was invalid";
	    return false;
	}
	const ON_3dPoint candidate_start = candidate->PointAtStart();
	const ON_3dPoint candidate_end = candidate->PointAtEnd();
	*reversed = loop_start.DistanceTo(candidate_end) <
	    loop_start.DistanceTo(candidate_start);
	const ON_3dPoint expected_start = *reversed ? candidate_end :
	    candidate_start;
	const ON_3dPoint expected_end = *reversed ? candidate_start :
	    candidate_end;
	if (!expected_start.IsValid() || !expected_end.IsValid() ||
		loop_start.DistanceTo(expected_start) > effective_tolerance ||
		loop_end.DistanceTo(expected_end) > effective_tolerance) {
	    interval_rejection =
		"trim and candidate subedge endpoint orientations did not agree";
	    return false;
	}
	const CurveDistanceEvaluator subedge_distance(candidate);
	if (!subedge_distance.IsValid()) {
	    interval_rejection = "candidate subedge locus evaluator was invalid";
	    return false;
	}
	for (int sample = 0; sample <= kDenseLiftValidationSegments;
		++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseLiftValidationSegments;
	    const ON_3dPoint uv = pcurve->PointAt(
		pcurve_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = uv.IsValid() ?
		data->surf->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
	    rejected_distance = lift.IsValid() ?
		subedge_distance.DistanceTo(lift, effective_tolerance) : DBL_MAX;
	    if (rejected_distance <= effective_tolerance)
		continue;
	    rejected_source_distance = DBL_MAX;
	    rejected_source_parameter = ON_UNSET_VALUE;
	    if (lift.IsValid())
		source_distance.ClosestParameter(lift,
		    &rejected_source_parameter, &rejected_source_distance);
	    rejected_sample = sample;
	    interval_rejection = "fragment pcurve left the candidate subedge";
	    return false;
	}
	return true;
    };

    const ON_Interval segment_domain(std::min(start_parameter, end_parameter),
	std::max(start_parameter, end_parameter));
    std::unique_ptr<ON_Curve> selected_subcurve;
    std::unique_ptr<ON_Curve> direct_subcurve(data->curve->DuplicateCurve());
    bool selected_reversed = false;
    const bool direct_exact = direct_subcurve &&
	direct_subcurve->Trim(segment_domain) &&
	exact_fragment_interval(direct_subcurve.get(), &selected_reversed);
    std::ostringstream direct_rejection;
    if (direct_exact) {
	selected_subcurve = std::move(direct_subcurve);
    } else if (data->curve->IsClosed()) {
	direct_rejection << interval_rejection;
	if (rejected_sample >= 0)
	    direct_rejection << " at sample " << rejected_sample << " by "
		<< rejected_distance << " (full source "
		<< rejected_source_distance << " at "
		<< rejected_source_parameter << ')';
	/* The complementary closed-curve interval begins at the larger source
	 * parameter and ends at the smaller parameter in the next period.  Moving
	 * only this duplicate's seam makes that exact interval contiguous; the
	 * authoritative source curve and all previously built edges are untouched. */
	std::unique_ptr<ON_Curve> wrapped_subcurve(data->curve->DuplicateCurve());
	const double wrapped_start = segment_domain.Max();
	const double wrapped_end = segment_domain.Min() + source_domain.Length();
	const ON_Interval wrapped_domain(wrapped_start, wrapped_end);
	bool wrapped_reversed = false;
	rejected_sample = -1;
	rejected_distance = DBL_MAX;
	rejected_source_distance = DBL_MAX;
	rejected_source_parameter = ON_UNSET_VALUE;
	interval_rejection.clear();
	const bool wrapped_exact = wrapped_subcurve &&
	    source_domain.IsIncreasing() && wrapped_domain.IsIncreasing() &&
	    wrapped_subcurve->ChangeClosedCurveSeam(wrapped_start) &&
	    wrapped_subcurve->Trim(wrapped_domain) &&
	    exact_fragment_interval(wrapped_subcurve.get(), &wrapped_reversed);
	if (wrapped_exact) {
	    selected_reversed = wrapped_reversed;
	    selected_subcurve = std::move(wrapped_subcurve);
	} else if (interval_rejection.empty()) {
	    interval_rejection =
		"complementary source interval could not be constructed";
	}
    }
    if (!selected_subcurve) {
	if (failure) {
	    std::ostringstream details;
	    if (!direct_rejection.str().empty())
		details << "direct interval " << direct_rejection.str()
		    << "; complementary interval ";
	    details << interval_rejection;
	    if (rejected_sample >= 0)
		details << " at sample " << rejected_sample << " by "
		    << rejected_distance << " (full source "
		    << rejected_source_distance << " at "
		    << rejected_source_parameter << ')';
	    details << "; endpoint parameters " << start_parameter << ':'
		<< end_parameter << " source domain " << data->curve->Domain().Min()
		<< ':' << data->curve->Domain().Max() << " closed/periodic "
		<< data->curve->IsClosed() << '/' << data->curve->IsPeriodic()
		<< " endpoint separation " << data->curve->PointAtStart().DistanceTo(
		    data->curve->PointAtEnd()) << " topology vertices "
		<< data->edge->m_vi[0] << ':' << data->edge->m_vi[1];
	    *failure = details.str();
	}
	return false;
    }
    ON_Curve *subcurve = selected_subcurve.release();
    *trim_reversed = selected_reversed;

    const ON_3dPoint edge_start = subcurve->PointAtStart();
    const ON_3dPoint edge_end = subcurve->PointAtEnd();

    /* Preserve parameter identity at the ends of the authoritative STEP
     * edge.  A native-surface seam can pass within the model tolerance of an
     * outer edge vertex (and small intentional features can be smaller than
     * that tolerance).  Choosing a subedge vertex by geometric proximity in
     * that situation collapses an internal split onto the wrong STEP vertex.
     *
     * selected_reversed describes the emitted trim's traversal of this
     * naturally parameterized subcurve.  Map the first/last traversal points
     * back to the original edge vertices before looking for reusable internal
     * split vertices. */
    int authoritative_subcurve_vertex[2] = {-1, -1};
    const int traversal_start_vertex = data->edge->m_vi[
	data->order_reversed ? 1 : 0];
    const int traversal_end_vertex = data->edge->m_vi[
	data->order_reversed ? 0 : 1];
    if (first_fragment)
	authoritative_subcurve_vertex[selected_reversed ? 1 : 0] =
	    traversal_start_vertex;
    if (last_fragment)
	authoritative_subcurve_vertex[selected_reversed ? 0 : 1] =
	    traversal_end_vertex;
    double maximum_outer_vertex_distance = 0.0;
    for (int end = 0; end < 2; ++end) {
	const int vi = authoritative_subcurve_vertex[end];
	const ON_3dPoint &point = end == 0 ? edge_start : edge_end;
	if (vi >= 0 && vi < brep->m_V.Count())
	    maximum_outer_vertex_distance = std::max(
		maximum_outer_vertex_distance,
		brep->m_V[vi].point.DistanceTo(point));
    }
    if (maximum_outer_vertex_distance > effective_tolerance &&
	    allow_tolerance_adjustment) {
	const double measured_tolerance = maximum_outer_vertex_distance *
	    kMeasuredToleranceSafetyFactor;
	if (measured_tolerance <= tolerance_adjustment_limit)
	    effective_tolerance = measured_tolerance;
    }
    for (int end = 0; end < 2; ++end) {
	const int vi = authoritative_subcurve_vertex[end];
	const ON_3dPoint &point = end == 0 ? edge_start : edge_end;
	if (vi < 0)
	    continue;
	if (vi >= brep->m_V.Count() ||
		brep->m_V[vi].point.DistanceTo(point) > effective_tolerance) {
	    if (failure) {
		std::ostringstream details;
		details << "outer fragment endpoint did not retain its STEP vertex "
		    << vi << ": distance "
		    << (vi >= 0 && vi < brep->m_V.Count() ?
			brep->m_V[vi].point.DistanceTo(point) : DBL_MAX)
		    << " exceeds tolerance " << effective_tolerance
		    << "; subcurve end " << end << " parameter "
		    << subcurve->Domain()[end] << "; selected-reversed "
		    << selected_reversed << "; traversal vertices "
		    << traversal_start_vertex << ':' << traversal_end_vertex
		    << "; source edge vertices " << data->edge->m_vi[0] << ':'
		    << data->edge->m_vi[1] << "; order-reversed "
		    << data->order_reversed << "; source domain "
		    << data->curve->Domain().Min() << ':'
		    << data->curve->Domain().Max();
		if (data->edge->m_vi[0] >= 0 && data->edge->m_vi[0] <
			brep->m_V.Count() && data->edge->m_vi[1] >= 0 &&
			data->edge->m_vi[1] < brep->m_V.Count()) {
		    const ON_3dPoint source_start = data->curve->PointAtStart();
		    const ON_3dPoint source_end = data->curve->PointAtEnd();
		    details << "; source endpoint distances ["
			<< source_start.DistanceTo(
			    brep->m_V[data->edge->m_vi[0]].point) << ','
			<< source_start.DistanceTo(
			    brep->m_V[data->edge->m_vi[1]].point) << "; "
			<< source_end.DistanceTo(
			    brep->m_V[data->edge->m_vi[0]].point) << ','
			<< source_end.DistanceTo(
			    brep->m_V[data->edge->m_vi[1]].point) << ']';
		}
		*failure = details.str();
	    }
	    delete subcurve;
	    return false;
	}
    }

    const int source_edge_id = data->edge->m_edge_user.i;
    *tolerance = effective_tolerance;
    const ON_3dPoint subcurve_midpoint = subcurve->PointAt(
	subcurve->Domain().Mid());
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	ON_BrepEdge &candidate = brep->m_E[ei];
	if (&candidate == data->edge || candidate.m_edge_user.i != source_edge_id ||
		candidate.m_vi[0] < 0 || candidate.m_vi[1] < 0 ||
		candidate.m_vi[0] >= brep->m_V.Count() ||
		candidate.m_vi[1] >= brep->m_V.Count())
	    continue;
	const ON_3dPoint candidate_start = brep->m_V[candidate.m_vi[0]].point;
	const ON_3dPoint candidate_end = brep->m_V[candidate.m_vi[1]].point;
	const bool candidate_start_identity =
	    (authoritative_subcurve_vertex[0] >= 0 ?
		candidate.m_vi[0] == authoritative_subcurve_vertex[0] :
		candidate.m_vi[0] != data->edge->m_vi[0] &&
		candidate.m_vi[0] != data->edge->m_vi[1]);
	const bool candidate_end_identity =
	    (authoritative_subcurve_vertex[1] >= 0 ?
		candidate.m_vi[1] == authoritative_subcurve_vertex[1] :
		candidate.m_vi[1] != data->edge->m_vi[0] &&
		candidate.m_vi[1] != data->edge->m_vi[1]);
	const bool candidate_opposite_start_identity =
	    (authoritative_subcurve_vertex[0] >= 0 ?
		candidate.m_vi[1] == authoritative_subcurve_vertex[0] :
		candidate.m_vi[1] != data->edge->m_vi[0] &&
		candidate.m_vi[1] != data->edge->m_vi[1]);
	const bool candidate_opposite_end_identity =
	    (authoritative_subcurve_vertex[1] >= 0 ?
		candidate.m_vi[0] == authoritative_subcurve_vertex[1] :
		candidate.m_vi[0] != data->edge->m_vi[0] &&
		candidate.m_vi[0] != data->edge->m_vi[1]);
	const bool same_direction = candidate_start.DistanceTo(edge_start) <= effective_tolerance &&
	    candidate_end.DistanceTo(edge_end) <= effective_tolerance &&
	    candidate_start_identity && candidate_end_identity;
	const bool opposite_direction = candidate_start.DistanceTo(edge_end) <= effective_tolerance &&
	    candidate_end.DistanceTo(edge_start) <= effective_tolerance &&
	    candidate_opposite_start_identity && candidate_opposite_end_identity;
	if (!same_direction && !opposite_direction)
	    continue;
	const CurveDistanceEvaluator candidate_distance(candidate.EdgeCurveOf());
	if (!candidate_distance.IsValid() ||
		candidate_distance.DistanceTo(subcurve_midpoint,
		    effective_tolerance) > effective_tolerance)
	    continue;
	delete subcurve;
	*segment_edge = &candidate;
	*trim_reversed = loop_start.DistanceTo(candidate_end) <
	    loop_start.DistanceTo(candidate_start);
	return true;
    }

    const auto matching_vertex = [brep, data, source_edge_id,
	effective_tolerance](
	const ON_3dPoint &point, int authoritative_vertex) -> int {
	if (authoritative_vertex >= 0)
	    return authoritative_vertex;
	int closest_vertex = -1;
	double closest_distance = DBL_MAX;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_edge_user.i != source_edge_id)
		continue;
	    for (int end = 0; end < 2; ++end) {
		const int vi = edge.m_vi[end];
		/* An internal surface-seam vertex is distinct from both
		 * authoritative STEP edge endpoints even when its point lies
		 * within the file uncertainty of one of them. */
		if (vi < 0 || vi >= brep->m_V.Count() ||
			vi == data->edge->m_vi[0] ||
			vi == data->edge->m_vi[1])
		    continue;
		const double distance = brep->m_V[vi].point.DistanceTo(point);
		if (distance <= effective_tolerance && distance < closest_distance) {
		    closest_distance = distance;
		    closest_vertex = vi;
		}
	    }
	}
	return closest_vertex;
    };
    int start_vertex = matching_vertex(edge_start,
	authoritative_subcurve_vertex[0]);
    int end_vertex = matching_vertex(edge_end,
	authoritative_subcurve_vertex[1]);
    if (start_vertex < 0)
	start_vertex = brep->NewVertex(edge_start,
	    effective_tolerance).m_vertex_index;
    if (end_vertex < 0)
	end_vertex = brep->NewVertex(edge_end,
	    effective_tolerance).m_vertex_index;
    if (start_vertex < 0 || end_vertex < 0) {
	delete subcurve;
	return false;
    }
    const int curve_index = brep->AddEdgeCurve(subcurve);
    if (curve_index < 0)
	return false;
    ON_BrepEdge &created = brep->NewEdge(brep->m_V[start_vertex],
	brep->m_V[end_vertex], curve_index, NULL, effective_tolerance);
    created.m_edge_user.i = source_edge_id;
    created.m_tolerance = effective_tolerance;
    *segment_edge = &created;
    return true;
}


static bool
normalize_periodic_pullback_segments(std::list<PBCData *> &pullbacks,
	const ON_Surface *surface, double tolerance, double item_scale,
	size_t *normalized_segments, STEPWrapper *step, int loop_id)
{
    if (normalized_segments)
	*normalized_segments = 0;
    if (!surface || !(tolerance > 0.0))
	return false;

    /* OpenNURBS closed surfaces do not promise periodic evaluation outside
     * their declared domain.  In particular, an ON_RevSurface with a closed
     * NURBS profile extrapolates that profile at v+n*period.  Normalize every
     * segment which fits wholly into the native domain, including segments on
     * doubly-periodic surfaces.  Adjacent branch choices are reconciled by the
     * seam and loop-chain passes below; acceptance here is based on the exact
     * STEP edge locus, not on an out-of-domain source lift. */
    if (!surface->IsClosed(0) && !surface->IsClosed(1))
	return false;

    const bool doubly_periodic = surface->IsClosed(0) &&
	surface->IsClosed(1);
    bool changed = false;
    for (std::list<PBCData *>::iterator data = pullbacks.begin();
	    data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	const CurveDistanceEvaluator source_distance((*data)->curve);
	if (!source_distance.IsValid())
	    continue;
	double locus_tolerance = std::max(tolerance, (*data)->tolerance);
	/* A source edge which has already been proven inconsistent with the
	 * declared file tolerance is validated more precisely after its pcurve is
	 * constructed.  Do not use that known-too-small tolerance to reject the
	 * native periodic image here: doing so leaves the seam resolver's extended
	 * coordinates outside the OpenNURBS evaluation domain, where PointAt()
	 * extrapolates instead of wrapping.  This is only a branch-selection bound;
	 * the dense curve-locus pass below still measures the complete edge and
	 * rejects it if it exceeds the same scale-bounded limit. */
	if ((*data)->tolerance_adjusted)
	    locus_tolerance = std::max(locus_tolerance,
		maximum_verified_edge_tolerance((*data)->curve,
		    (*data)->declared_tolerance, item_scale));
	for (std::list<ON_2dPointArray *>::iterator segment =
		(*data)->segments->begin(); segment != (*data)->segments->end();
		++segment) {
	    if (!*segment || (*segment)->Count() < 2)
		continue;
	    if (doubly_periodic) {
		/* Do not choose independent branches for ordinary valid torus
		 * segments.  This special case repairs only the OpenNURBS
		 * out-of-domain evaluation failure: the supplied UVs no longer lift
		 * to their STEP edge, while one whole-period translation of the
		 * complete segment does.  A seam-straddling path cannot fit wholly
		 * into one native copy and is deliberately left to the coherent
		 * seam resolver. */
		bool original_exact = true;
		for (int point = 0; original_exact && point < (*segment)->Count();
			++point) {
		    const ON_2dPoint uv = (**segment)[point];
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    original_exact = lift.IsValid() &&
			source_distance.DistanceTo(lift, locus_tolerance) <=
			locus_tolerance;
		}
		if (original_exact)
		    continue;

		ON_2dPointArray candidate(**segment);
		bool candidate_changed = false;
		bool candidate_in_domain = true;
		for (int direction = 0; direction < 2; ++direction) {
		    const ON_Interval domain = surface->Domain(direction);
		    const double period = domain.Length();
		    if (!(period > ON_ZERO_TOLERANCE)) {
			candidate_in_domain = false;
			break;
		    }
		    /* Loop endpoint closure can select an equivalent periodic image
		     * while the interior samples retain their original continuous
		     * branch.  Unwrap the ordered samples before measuring whether the
		     * complete curve fits in one native copy.  This is especially
		     * important for doubly-closed rational surfaces: treating the isolated
		     * endpoint as part of the bounding span leaves a full-period UV chord
		     * even though every sample lifts to the exact STEP edge. */
		    for (int point = 1; point < candidate.Count(); ++point) {
			const double unwrap_shift = round(
			    (candidate[point - 1][direction] -
			     candidate[point][direction]) / period) * period;
			if (fabs(unwrap_shift) > ON_ZERO_TOLERANCE) {
			    candidate[point][direction] += unwrap_shift;
			    candidate_changed = true;
			}
		    }
		    double minimum = candidate[0][direction];
		    double maximum = minimum;
		    for (int point = 1; point < candidate.Count(); ++point) {
			minimum = std::min(minimum, candidate[point][direction]);
			maximum = std::max(maximum, candidate[point][direction]);
		    }
		    const double guard = std::max(ON_ZERO_TOLERANCE,
			period * 1.0e-10);
		    if (maximum - minimum > period + guard) {
			candidate_in_domain = false;
			break;
		    }
		    const double shift = round((domain.Mid() -
			0.5 * (minimum + maximum)) / period) * period;
		    if (fabs(shift) > ON_ZERO_TOLERANCE) {
			for (int point = 0; point < candidate.Count(); ++point)
			    candidate[point][direction] += shift;
			candidate_changed = true;
		    }
		    for (int point = 0; point < candidate.Count(); ++point) {
			if (candidate[point][direction] < domain.Min() - guard ||
				candidate[point][direction] > domain.Max() + guard) {
			    candidate_in_domain = false;
			    break;
			}
		    }
		    if (!candidate_in_domain)
			break;
		}
		if (!candidate_in_domain && step && step->Verbose() &&
			candidate_changed)
		    std::cerr << "EDGE_LOOP #" << loop_id << ": STEP edge #"
			<< ((*data)->edge ? (*data)->edge->m_edge_user.i : -1)
			<< " doubly-periodic native normalization could not fit "
			   "the continuous samples inside the surface domains"
			<< std::endl;
		bool candidate_exact = candidate_changed && candidate_in_domain;
		double rejected_distance = 0.0;
		int rejected_point = -1;
		for (int point = 0; candidate_exact && point < candidate.Count();
			++point) {
		    const ON_3dPoint lift = surface->PointAt(candidate[point].x,
			candidate[point].y);
		    rejected_distance = lift.IsValid() ?
			source_distance.DistanceTo(lift, locus_tolerance) : DBL_MAX;
		    candidate_exact = lift.IsValid() &&
			rejected_distance <= locus_tolerance;
		    if (!candidate_exact)
			rejected_point = point;
		}
		if (!candidate_exact) {
		    if (step && step->Verbose() && candidate_changed &&
			    candidate_in_domain)
			std::cerr << "EDGE_LOOP #" << loop_id << ": STEP edge #"
			    << ((*data)->edge ? (*data)->edge->m_edge_user.i : -1)
			    << " doubly-periodic native normalization rejected at "
			    << rejected_point << " distance=" << rejected_distance
			    << " tolerance=" << locus_tolerance << std::endl;
		    continue;
		}
		**segment = candidate;
		changed = true;
		if (normalized_segments)
		    ++*normalized_segments;
		continue;
	    }
	    ON_2dPointArray candidate(**segment);
	    bool segment_changed = false;
	    bool candidate_in_domain = true;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		if (!(period > ON_ZERO_TOLERANCE))
		    continue;
		/* Closest-point projection may return equivalent samples on opposite
		 * copies of a periodic seam.  Unwrap the complete ordered path before
		 * choosing its native-domain translation.  Adjusting only the two
		 * endpoints leaves an interior 2*pi jump which a UV interpolant crosses
		 * through the middle of the surface, despite every raw sample lifting
		 * exactly onto the 3-D edge. */
		for (int point = 1; point < candidate.Count(); ++point) {
		    const double unwrap_shift = round((candidate[point - 1][direction] -
			candidate[point][direction]) / period) * period;
		    if (fabs(unwrap_shift) > ON_ZERO_TOLERANCE) {
			candidate[point][direction] += unwrap_shift;
			segment_changed = true;
		    }
		}
		double minimum = candidate[0][direction];
		double maximum = minimum;
		for (int point = 1; point < candidate.Count(); ++point) {
		    minimum = std::min(minimum, candidate[point][direction]);
		    maximum = std::max(maximum, candidate[point][direction]);
		}
		/* Preserve a constant closed-boundary curve which is already on the
		 * native min/max side.  Constant curves on a distant periodic copy
		 * still need whole-segment translation; leaving one at u+n*period is
		 * how two exact adjacent trims can remain precisely one period apart
		 * and fail OpenNURBS loop closure. */
		const double native_guard = std::max(ON_ZERO_TOLERANCE,
		    period * 1.0e-10);
		if (maximum - minimum <= ON_ZERO_TOLERANCE &&
			minimum >= domain.Min() - native_guard &&
			maximum <= domain.Max() + native_guard)
		    continue;
		const double center = 0.5 * (minimum + maximum);
		const double shift = round((domain.Mid() - center) / period) * period;
		if (fabs(shift) > ON_ZERO_TOLERANCE) {
		    for (int point = 0; point < candidate.Count(); ++point)
			candidate[point][direction] += shift;
		    segment_changed = true;
		}
		/* A closed 3-D edge may project both coincident endpoints onto the
		 * same periodic copy.  Select each endpoint from its adjacent
		 * interior sample rather than introducing a one-period spike. */
		ON_2dPoint *start = candidate.At(0);
		ON_2dPoint *next = candidate.At(1);
		ON_2dPoint *end = candidate.At(candidate.Count() - 1);
		ON_2dPoint *preceding = candidate.At(candidate.Count() - 2);
		const double start_shift = round(((*next)[direction] -
		    (*start)[direction]) / period) * period;
		const double end_shift = round(((*preceding)[direction] -
		    (*end)[direction]) / period) * period;
		(*start)[direction] += start_shift;
		(*end)[direction] += end_shift;
		segment_changed = segment_changed ||
		    fabs(start_shift) > ON_ZERO_TOLERANCE ||
		    fabs(end_shift) > ON_ZERO_TOLERANCE;
		const double guard = std::max(ON_ZERO_TOLERANCE,
		    period * 1.0e-10);
		for (int point = 0; point < candidate.Count(); ++point) {
		    if (candidate[point][direction] < domain.Min() - guard ||
			    candidate[point][direction] > domain.Max() + guard) {
			candidate_in_domain = false;
			break;
		    }
		}
	    }
	    if (!segment_changed || !candidate_in_domain)
		continue;

	    bool exact = true;
	    for (int point = 0; point < candidate.Count(); ++point) {
		const ON_3dPoint candidate_lift = surface->PointAt(candidate[point].x,
		    candidate[point].y);
		/* The original sample may be outside an OpenNURBS evaluation domain,
		 * where evaluating it is not a valid periodic-equivalence proof.  The
		 * native candidate must instead retain membership in the exact source
		 * edge locus within the already established local tolerance. */
		if (!candidate_lift.IsValid() ||
			source_distance.DistanceTo(candidate_lift,
			    locus_tolerance) > locus_tolerance) {
		    exact = false;
		    break;
		}
	    }
	    if (!exact)
		continue;
	    **segment = candidate;
	    changed = true;
	    if (normalized_segments)
		++*normalized_segments;
	}
    }
    return changed;
}


/* Convert an extended continuous UV path which necessarily crosses a native
 * periodic boundary into native-domain fragments.  Every crossing point is
 * found on the original 3-D STEP curve and represented on both equivalent
 * sides of the surface seam.  This preserves the source edge and geometry;
 * it only supplies the multiple trim fragments required by openNURBS. */
static bool
split_periodic_pullback_at_native_seams(PBCData *data, double tolerance,
    size_t *split_count)
{
    if (split_count) *split_count = 0;
    if (!data || !data->segments || !data->surf || !data->curve ||
	!(tolerance > 0.0) ||
	(!data->surf->IsClosed(0) && !data->surf->IsClosed(1)))
	return false;

    const CurveDistanceEvaluator source_distance(data->curve);
    if (!source_distance.IsValid()) return false;
    const double native_lift_tolerance = std::max(tolerance,
	data->tolerance);
    const double native_projection_tolerance = short_curve_pullback_resolution(
	data->curve, native_lift_tolerance);
    const auto native_image_preserving_lift = [&](const ON_2dPoint &raw,
	ON_2dPoint &native, const ON_3dPoint *authoritative_source = NULL) {
	const ON_3dPoint raw_lift = data->surf->PointAt(raw.x, raw.y);
	if ((!authoritative_source || !authoritative_source->IsValid()) &&
		(!raw_lift.IsValid() || source_distance.DistanceTo(raw_lift,
		    native_lift_tolerance) > native_lift_tolerance))
	    return false;
	const ON_3dPoint target = authoritative_source &&
	    authoritative_source->IsValid() ? *authoritative_source : raw_lift;
	native = UnwrapUVPoint(data->surf, raw, tolerance);
	ON_3dPoint native_lift = data->surf->PointAt(native.x, native.y);
	if (native_lift.IsValid() &&
		native_lift.DistanceTo(target) <= native_lift_tolerance &&
		source_distance.DistanceTo(native_lift,
		    native_lift_tolerance) <= native_lift_tolerance)
	    return true;

	/* Closed OpenNURBS surfaces are not required to evaluate a parameter
	 * outside their native domain as an exactly periodic image.  In
	 * particular, the closed profile of an ON_RevSurface can extrapolate at
	 * v+n*period; reducing that value modulo the domain then changes its 3-D
	 * lift.  Locate the target's actual native surface image instead of
	 * accepting parameter arithmetic as geometric equivalence.  At the two
	 * outer samples the target is the authoritative STEP curve endpoint;
	 * otherwise it is the already source-locus-validated raw lift. */
	ON_3dPoint uv(native.x, native.y, 0.0);
	double distance = DBL_MAX;
	bool projected = refine_surface_point_seeded(data->surf, target,
	    native_projection_tolerance, uv, &distance, native_lift_tolerance,
	    true);
	if (!projected) {
	    if (!data->context)
		data->context = std::make_shared<brlcad::PullbackContext>();
	    ON_2dPoint projected_uv(native);
	    projected = data->context->SurfaceClosestPoint(data->surf,
		target, projected_uv, native_lift, distance, 0,
		native_projection_tolerance, native_lift_tolerance);
	    if (projected)
		uv.Set(projected_uv.x, projected_uv.y, 0.0);
	}
	if (!projected)
	    return false;
	native.Set(uv.x, uv.y);
	native_lift = data->surf->PointAt(native.x, native.y);
	return native_lift.IsValid() &&
	    native_lift.DistanceTo(target) <= native_lift_tolerance &&
	    source_distance.DistanceTo(native_lift,
		native_lift_tolerance) <= native_lift_tolerance;
    };
    std::list<ON_2dPointArray *> replacement;
    size_t splits = 0;
    bool native_remapped = false;
    bool valid = true;

    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); valid && segment != data->segments->end();
	    ++segment) {
	if (!*segment || (*segment)->Count() < 2) {
	    valid = false;
	    break;
	}
	ON_2dPointArray *current = new ON_2dPointArray();
	ON_2dPoint previous;
	/* The outer samples are authoritative traversal endpoints even when the
	 * source geometry is closed.  A closed STEP edge still names a topology
	 * vertex, and its imported 3-D curve has been seamed at that vertex.  If an
	 * extended UV endpoint lies outside the OpenNURBS surface domain, evaluating
	 * that raw parameter can extrapolate to an unrelated point; using the raw
	 * lift as the remapping target then discards part of the exact closed edge.
	 * Preserve the curve endpoint here and let the dense fragment checks below
	 * prove every interior sample independently. */
	const bool first_source_sample = segment == data->segments->begin();
	std::list<ON_2dPointArray *>::const_iterator following_segment = segment;
	++following_segment;
	const bool last_source_segment =
	    following_segment == data->segments->end();
	const ON_3dPoint source_start = first_source_sample ?
	    (data->order_reversed ? data->curve->PointAtEnd() :
	    data->curve->PointAtStart()) : ON_3dPoint::UnsetPoint;
	if (!native_image_preserving_lift((**segment)[0], previous,
		first_source_sample ? &source_start : NULL)) {
	    delete current;
	    valid = false;
	    break;
	}
	native_remapped = native_remapped ||
	    previous.DistanceTo((**segment)[0]) > ON_ZERO_TOLERANCE;
	current->Append(previous);
	for (int point = 1; valid && point < (*segment)->Count(); ++point) {
	    if (brlcad::PullbackWorkCancelled()) {
		valid = false;
		break;
	    }
	    ON_2dPoint native;
	    const bool last_source_sample = last_source_segment &&
		point == (*segment)->Count() - 1;
	    const ON_3dPoint source_end = last_source_sample ?
		(data->order_reversed ? data->curve->PointAtStart() :
		data->curve->PointAtEnd()) : ON_3dPoint::UnsetPoint;
	    if (!native_image_preserving_lift((**segment)[point], native,
		    last_source_sample ? &source_end : NULL)) {
		valid = false;
		break;
	    }
	    /* A surface seam has two native boundary coordinates for the same
	     * 3-D point.  Closest-point iteration may alternate between them on
	     * successive samples, particularly near a spindle-torus pole.  Keep a
	     * boundary-following path on the preceding side whenever that literal
	     * substitution preserves the already proved raw lift.  Otherwise the
	     * seam detector would manufacture one topology split per sample. */
	    const ON_2dPoint raw = (**segment)[point];
	    const ON_3dPoint raw_lift = data->surf->PointAt(raw.x, raw.y);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!data->surf->IsClosed(direction)) continue;
		const ON_Interval domain = data->surf->Domain(direction);
		const double period = domain.Length();
		const double guard = std::max(ON_ZERO_TOLERANCE,
		    period * 1.0e-10);
		const bool native_at_min = fabs(native[direction] -
		    domain.Min()) <= guard;
		const bool native_at_max = fabs(native[direction] -
		    domain.Max()) <= guard;
		const bool previous_at_min = fabs(previous[direction] -
		    domain.Min()) <= guard;
		const bool previous_at_max = fabs(previous[direction] -
		    domain.Max()) <= guard;
		if (!((native_at_min && previous_at_max) ||
			(native_at_max && previous_at_min)))
		    continue;
		ON_2dPoint continued(native);
		continued[direction] = previous[direction];
		const ON_3dPoint continued_lift = data->surf->PointAt(
		    continued.x, continued.y);
		if (raw_lift.IsValid() && continued_lift.IsValid() &&
			continued_lift.DistanceTo(raw_lift) <=
			    native_lift_tolerance)
		    native = continued;
	    }
	    native_remapped = native_remapped ||
		native.DistanceTo(raw) > ON_ZERO_TOLERANCE;
	    int u_direction = 0;
	    int v_direction = 0;
	    bool crosses_seam = ConsecutivePointsCrossClosedSeam(data->surf,
		native, previous, u_direction, v_direction, tolerance);
	    /* ConsecutivePointsCrossClosedSeam deliberately suppresses a crossing
	     * when either sample is already on the seam.  That is useful to avoid
	     * duplicate splits in generic sampling, but here it can leave one
	     * native-domain jump in an otherwise continuous STEP pullback when the
	     * sampler happens to land exactly on the boundary.  Recover only the
	     * unambiguous greater-than-half-period jump; both seam images are still
	     * source-locus validated below before the split is accepted. */
	    if (!crosses_seam) {
		if (data->surf->IsClosed(0) && fabs(native.x - previous.x) >
			0.5 * data->surf->Domain(0).Length()) {
		    u_direction = native.x < previous.x ? 1 : 2;
		    crosses_seam = true;
		}
		if (data->surf->IsClosed(1) && fabs(native.y - previous.y) >
			0.5 * data->surf->Domain(1).Length()) {
		    v_direction = native.y < previous.y ? 2 : 1;
		    crosses_seam = true;
		}
	    }
	    if (!crosses_seam) {
		current->Append(native);
		previous = native;
		continue;
	    }

	    const ON_3dPoint previous_lift = data->surf->PointAt(
		previous.x, previous.y);
	    const ON_3dPoint native_lift = data->surf->PointAt(native.x, native.y);
	    double previous_parameter = 0.0;
	    double native_parameter = 0.0;
	    double previous_distance = DBL_MAX;
	    double native_distance = DBL_MAX;
	    if (!source_distance.ClosestParameter(previous_lift,
		    &previous_parameter, &previous_distance) ||
		!source_distance.ClosestParameter(native_lift,
		    &native_parameter, &native_distance) ||
		previous_distance > data->tolerance ||
		native_distance > data->tolerance) {
		valid = false;
		break;
	    }
	    double seam_parameter = 0.0;
	    ON_2dPoint from = ON_2dPoint::UnsetPoint;
	    ON_2dPoint to = ON_2dPoint::UnsetPoint;
	    const int previous_seam = IsAtSeam(data->surf, previous,
		tolerance);
	    const int native_seam = IsAtSeam(data->surf, native, tolerance);
	    const auto is_crossed_seam = [u_direction, v_direction](int seam) {
		return (u_direction && (seam == 1 || seam == 3)) ||
		    (v_direction && (seam == 2 || seam == 3));
	    };
	    const int seam_hint = u_direction && v_direction ? 3 :
		(u_direction ? 1 : 2);
	    bool found_crossing = false;
	    if (is_crossed_seam(previous_seam)) {
		from = to = previous;
		SwapUVSeamPoint(data->surf, to, seam_hint);
		seam_parameter = previous_parameter;
		found_crossing = true;
	    } else if (is_crossed_seam(native_seam)) {
		from = to = native;
		SwapUVSeamPoint(data->surf, from, seam_hint);
		seam_parameter = native_parameter;
		found_crossing = true;
	    } else {
		found_crossing = Find3DCurveSeamCrossing(*data,
		    previous_parameter, native_parameter, 0.0, seam_parameter,
		    from, to, tolerance, data->tolerance, data->tolerance);
	    }
	    if (!found_crossing) {
		valid = false;
		break;
	    }
	    ForceToClosestSeam(data->surf, from, tolerance);
	    ForceToClosestSeam(data->surf, to, tolerance);
	    const ON_3dPoint from_lift = data->surf->PointAt(from.x, from.y);
	    const ON_3dPoint to_lift = data->surf->PointAt(to.x, to.y);
	    if (!from_lift.IsValid() || !to_lift.IsValid() ||
		    source_distance.DistanceTo(from_lift, data->tolerance) >
			data->tolerance ||
		    source_distance.DistanceTo(to_lift, data->tolerance) >
			data->tolerance) {
		valid = false;
		break;
	    }
	    if (current->Count() == 0 || current->Last()->DistanceTo(from) >
		    ON_ZERO_TOLERANCE)
		current->Append(from);
	    if (current->Count() < 2) {
		valid = false;
		break;
	    }
	    replacement.push_back(current);
	    current = new ON_2dPointArray();
	    current->Append(to);
	    if (to.DistanceTo(native) > ON_ZERO_TOLERANCE)
		current->Append(native);
	    ++splits;
	    previous = native;
	}
	if (!valid) {
	    delete current;
	    break;
	}
	if (current->Count() < 2) {
	    delete current;
	    valid = false;
	    break;
	}
	replacement.push_back(current);
    }

	if (!valid || (splits == 0 && !native_remapped)) {
	for (std::list<ON_2dPointArray *>::iterator segment = replacement.begin();
	     segment != replacement.end(); ++segment)
	    delete *segment;
	return false;
    }
    while (!data->segments->empty()) {
	delete data->segments->front();
	data->segments->pop_front();
    }
    data->segments->swap(replacement);
    if (split_count) *split_count = splits;
    return true;
}


/* Native-domain seam fragments of one ordinary STEP edge are not separate
 * topology uses.  Rejoin them on one unwrapped periodic branch so openNURBS
 * receives one trim with the original edge's two vertices.  Only a proven
 * surface-singularity split may create real subedges; representing an
 * ordinary seam crossing as multiple trims on the complete source edge gives
 * every fragment the same endpoint vertices and makes loop topology invalid. */
static bool
merge_ordinary_periodic_pullback_fragments(PBCData *data, double tolerance,
	std::string *failure = NULL, bool preserve_singular_boundaries = false,
	size_t *retained_groups = NULL)
{
    if (failure) failure->clear();
    if (retained_groups) *retained_groups = 0;
    if (!data || !data->segments || data->segments->size() < 2 ||
	    !data->surf || !(tolerance > 0.0)) {
	if (failure) *failure = "incomplete periodic fragment data";
	return false;
	}

    std::unique_ptr<ON_2dPointArray> merged(new ON_2dPointArray());
    std::vector<std::unique_ptr<ON_2dPointArray> > groups;
    const double lift_tolerance = std::max(ON_ZERO_TOLERANCE *
	kNumericalToleranceScale,
	tolerance * kPeriodicLiftEquivalenceToleranceFraction);
    const double join_tolerance = std::max(tolerance, data->tolerance);
    const CurveDistanceEvaluator source_distance(data->curve);
    if (!source_distance.IsValid()) {
	if (failure) *failure = "the source STEP curve could not be evaluated";
	return false;
    }

    std::set<const ON_2dPointArray *> redundant_source_fragments;
    std::string complete_candidate_failure;
    if (preserve_singular_boundaries) {
	const ON_Interval source_domain = data->curve->Domain();
	const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	    source_domain.Length() * 1.0e-10);
	const auto fragment_lies_on_source = [&](const ON_2dPointArray *fragment,
		double validation_tolerance) {
	    if (!fragment || fragment->Count() < 1)
		return false;
	    const int interval_count = std::max(1, fragment->Count() - 1);
	    for (int interval = 0; interval < interval_count; ++interval) {
		const ON_2dPoint start = (*fragment)[std::min(interval,
		    fragment->Count() - 1)];
		const ON_2dPoint end = (*fragment)[std::min(interval + 1,
		    fragment->Count() - 1)];
		for (int sample = 0;
			sample <= kFragmentIntervalValidationSubdivisions; ++sample) {
		    if (brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) /
			kFragmentIntervalValidationSubdivisions;
		    const ON_2dPoint uv = (1.0 - fraction) * start +
			fraction * end;
		    const ON_3dPoint lift = data->surf->PointAt(uv.x, uv.y);
		    if (!lift.IsValid() || source_distance.DistanceTo(lift,
			    validation_tolerance) > validation_tolerance)
			return false;
		}
	    }
	    return true;
	};
	const ON_2dPointArray *complete_source_fragment = NULL;
	for (std::list<ON_2dPointArray *>::const_iterator segment =
		data->segments->begin(); segment != data->segments->end(); ++segment) {
	    if (!*segment || (*segment)->Count() < 2)
		continue;
	    const ON_2dPoint start = (**segment)[0];
	    const ON_2dPoint end = (**segment)[(*segment)->Count() - 1];
	    const ON_3dPoint start_lift = data->surf->PointAt(start.x, start.y);
	    const ON_3dPoint end_lift = data->surf->PointAt(end.x, end.y);
	    double start_parameter = 0.0;
	    double end_parameter = 0.0;
	    double start_distance = DBL_MAX;
	    double end_distance = DBL_MAX;
	    if (!source_distance.ClosestParameter(start_lift, &start_parameter,
		    &start_distance) ||
		!source_distance.ClosestParameter(end_lift, &end_parameter,
		    &end_distance) || start_distance > join_tolerance ||
		end_distance > join_tolerance)
		continue;
	    const ON_3dPoint source_minimum = data->curve->PointAt(
		source_domain.Min());
	    const ON_3dPoint source_maximum = data->curve->PointAt(
		source_domain.Max());
	    /* ClosestParameter need not return the literal endpoint parameter when
	     * a high-order curve approaches that endpoint tangentially.  Accept an
	     * endpoint selection either parametrically or by a direct geometric
	     * match to the authoritative source endpoint. */
	    const bool start_at_minimum =
		fabs(start_parameter - source_domain.Min()) <= parameter_guard ||
		(start_lift.IsValid() && source_minimum.IsValid() &&
		 start_lift.DistanceTo(source_minimum) <= join_tolerance);
	    const bool start_at_maximum =
		fabs(start_parameter - source_domain.Max()) <= parameter_guard ||
		(start_lift.IsValid() && source_maximum.IsValid() &&
		 start_lift.DistanceTo(source_maximum) <= join_tolerance);
	    const bool end_at_minimum =
		fabs(end_parameter - source_domain.Min()) <= parameter_guard ||
		(end_lift.IsValid() && source_minimum.IsValid() &&
		 end_lift.DistanceTo(source_minimum) <= join_tolerance);
	    const bool end_at_maximum =
		fabs(end_parameter - source_domain.Max()) <= parameter_guard ||
		(end_lift.IsValid() && source_maximum.IsValid() &&
		 end_lift.DistanceTo(source_maximum) <= join_tolerance);
	    const bool covers_forward = start_at_minimum && end_at_maximum;
	    const bool covers_reverse = start_at_maximum && end_at_minimum;
	    if (!covers_forward && !covers_reverse) {
		std::ostringstream details;
		details.precision(17);
		details << "candidate source interval " << start_parameter << ':'
		    << end_parameter << " did not cover domain "
		    << source_domain.Min() << ':' << source_domain.Max()
		    << " within parameter guard " << parameter_guard;
		if (complete_candidate_failure.empty())
		    complete_candidate_failure = details.str();
		continue;
	    }
	    bool exact_complete_fragment = fragment_lies_on_source(*segment,
		join_tolerance);
	    if (!exact_complete_fragment) {
		/* Adaptive pullback samples can be sparse enough that their straight
		 * UV chords miss a strongly curved source between samples.  The
		 * endpoint proof above identifies a candidate only.  Rebuild it by
		 * projecting the complete authoritative source curve, then prove the
		 * represented dense polyline before allowing it to replace the
		 * fragmented path. */
		ON_Curve *refined_curve = NULL;
		std::string refinement_failure;
		if (refined_fragment_polyline(data, **segment, join_tolerance,
			&refined_curve, &refinement_failure, true)) {
		    const ON_PolylineCurve *polyline =
			ON_PolylineCurve::Cast(refined_curve);
		    ON_2dPointArray refined_samples;
		    if (polyline)
			refined_samples.Reserve(polyline->m_pline.Count());
		    for (int point = 0; polyline &&
			    point < polyline->m_pline.Count(); ++point)
			refined_samples.Append(ON_2dPoint(
			    polyline->m_pline[point].x,
			    polyline->m_pline[point].y));
		    exact_complete_fragment = refined_samples.Count() >= 2 &&
			fragment_lies_on_source(&refined_samples, join_tolerance);
		    if (exact_complete_fragment)
			**segment = refined_samples;
		    else
			complete_candidate_failure =
			    "dense complete-edge polyline left the source locus";
		}
		else
		    complete_candidate_failure = refinement_failure;
		delete refined_curve;
	    }
	    if (exact_complete_fragment) {
		complete_source_fragment = *segment;
		break;
	    }
	}
	if (complete_source_fragment) {
	    /* A closest-point seam split infinitesimally beside a surface pole can
	     * leave an overlapping UV fragment although another fragment already
	     * spans the complete authoritative STEP curve.  The STEP topology has
	     * one edge use, so any additional fragment whose complete represented
	     * locus is proven to lie on that same source edge is duplicate geometry,
	     * not a new singular subedge.  Retaining it would repeat a source-curve
	     * interval and create a false singular bridge with a different vertex. */
	    for (std::list<ON_2dPointArray *>::const_iterator segment =
		    data->segments->begin(); segment != data->segments->end(); ++segment) {
		if (!*segment || *segment == complete_source_fragment)
		    continue;
		if (fragment_lies_on_source(*segment, join_tolerance))
		    redundant_source_fragments.insert(*segment);
	    }
	}
    }
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (redundant_source_fragments.find(*segment) !=
		redundant_source_fragments.end())
	    continue;
	/* A parameter-zero split can leave a one-sample fragment when zero is
	 * only a few floating-point parameter units from an edge endpoint.  That
	 * point still identifies the original STEP edge endpoint and must take
	 * part in the periodic-branch merge.  The completed merged path is
	 * required to contain at least two points below. */
	if (!*segment || (*segment)->Count() < 1) {
	    if (failure) *failure = "a periodic fragment contained no samples";
	    return false;
	}
	ON_2dPointArray shifted(**segment);
	if (merged->Count() > 0) {
	    const ON_2dPoint previous = (*merged)[merged->Count() - 1];
	    const ON_2dPoint first = shifted[0];
	    if (preserve_singular_boundaries) {
		const ON_3dPoint previous_lift = data->surf->PointAt(
		    previous.x, previous.y);
		const ON_3dPoint first_lift = data->surf->PointAt(first.x, first.y);
		const bool singular_endpoint =
		    IsAtSingularity(data->surf, previous, PBC_SEAM_TOL) >= 0 ||
		    IsAtSingularity(data->surf, first, PBC_SEAM_TOL) >= 0;
		double previous_parameter = 0.0;
		double first_parameter = 0.0;
		double previous_distance = DBL_MAX;
		double first_distance = DBL_MAX;
		const ON_Interval source_domain = data->curve->Domain();
		const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
		    source_domain.Length() * 1.0e-10);
		/* A pullback split beside a pole can be only a numerical fragment at
		 * the original STEP edge endpoint.  Creating a subedge for it gives a
		 * zero source-curve interval.  Preserve a singular topology boundary
		 * only when both images of the pole prove a strictly interior source
		 * parameter; endpoint-adjacent fragments remain one exact edge use. */
		const bool interior_source_parameter = singular_endpoint &&
		    source_domain.IsIncreasing() &&
		    source_distance.ClosestParameter(previous_lift,
			&previous_parameter, &previous_distance) &&
		    source_distance.ClosestParameter(first_lift,
			&first_parameter, &first_distance) &&
		    previous_distance <= join_tolerance &&
		    first_distance <= join_tolerance &&
		    previous_parameter > source_domain.Min() + parameter_guard &&
		    previous_parameter < source_domain.Max() - parameter_guard &&
		    first_parameter > source_domain.Min() + parameter_guard &&
		    first_parameter < source_domain.Max() - parameter_guard;
		const bool exact_singular_boundary = interior_source_parameter &&
		    previous_lift.IsValid() && first_lift.IsValid() &&
		    previous_lift.DistanceTo(first_lift) <= join_tolerance &&
		    source_distance.DistanceTo(previous_lift, join_tolerance) <=
			join_tolerance &&
		    source_distance.DistanceTo(first_lift, join_tolerance) <=
			join_tolerance;
		if (exact_singular_boundary) {
		    if (merged->Count() < 2) {
			if (failure)
			    *failure = "a singular pullback group had fewer than two samples";
			return false;
		    }
		    groups.push_back(std::move(merged));
		    merged.reset(new ON_2dPointArray());
		}
	    }
	    if (merged->Count() > 0) {
	    double translation[2] = {0.0, 0.0};
	    for (int direction = 0; direction < 2; ++direction) {
		if (!data->surf->IsClosed(direction))
		    continue;
		const double period = data->surf->Domain(direction).Length();
		if (period > ON_ZERO_TOLERANCE)
		    translation[direction] = round((previous[direction] -
			first[direction]) / period) * period;
	    }
	    for (int point = 0; point < shifted.Count(); ++point) {
		const ON_2dPoint original = shifted[point];
		shifted[point].x += translation[0];
		shifted[point].y += translation[1];
		const ON_3dPoint original_lift = data->surf->PointAt(
		    original.x, original.y);
		const ON_3dPoint shifted_lift = data->surf->PointAt(
		    shifted[point].x, shifted[point].y);
		const double lift_change = original_lift.IsValid() &&
		    shifted_lift.IsValid() ? original_lift.DistanceTo(shifted_lift) :
		    DBL_MAX;
		if (lift_change > lift_tolerance) {
		    if (failure) {
			std::ostringstream reason;
			reason << "integer-period translation changed a fragment's "
			    "3-D lift by " << lift_change << " (limit "
			    << lift_tolerance << ", uv " << original.x << ':'
			    << original.y << " -> " << shifted[point].x << ':'
			    << shifted[point].y << ')';
			*failure = reason.str();
		    }
		    return false;
		}
	    }
	    const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE,
		std::max(data->surf->Domain(0).Length(),
		    data->surf->Domain(1).Length()) * 1.0e-10);
	    if (previous.DistanceTo(shifted[0]) > parameter_tolerance) {
		const ON_3dPoint previous_lift = data->surf->PointAt(
		    previous.x, previous.y);
		const ON_3dPoint first_lift = data->surf->PointAt(
		    shifted[0].x, shifted[0].y);
		const bool same_edge_point = previous_lift.IsValid() &&
		    first_lift.IsValid() &&
		    previous_lift.DistanceTo(first_lift) <= join_tolerance &&
		    source_distance.DistanceTo(previous_lift, join_tolerance) <=
			join_tolerance &&
		    source_distance.DistanceTo(first_lift, join_tolerance) <=
			join_tolerance;
		if (!same_edge_point) {
		    if (failure) {
			std::ostringstream reason;
			reason << "adjacent periodic fragments did not share a "
			    "validated edge endpoint (" << previous.x << ':'
			    << previous.y << " versus " << shifted[0].x << ':'
			    << shifted[0].y << ')';
			if (!complete_candidate_failure.empty())
			    reason << "; complete-fragment proof rejected: "
				<< complete_candidate_failure;
			*failure = reason.str();
		    }
		    return false;
		}
		/* This is an internal representation split, not a STEP topology
		 * vertex.  Select the already retained endpoint so the rejoined UV
		 * path is continuous. */
		shifted[0] = previous;
	    }
	    }
	}
	const int first_point = merged->Count() > 0 ? 1 : 0;
	for (int point = first_point; point < shifted.Count(); ++point) {
	    if (merged->Count() == 0 || shifted[point].DistanceTo(
		    (*merged)[merged->Count() - 1]) > ON_ZERO_TOLERANCE)
		merged->Append(shifted[point]);
	}
    }
    if (merged->Count() < 2) {
	if (failure) *failure = "the merged periodic path had fewer than two samples";
	return false;
    }
	groups.push_back(std::move(merged));

    while (!data->segments->empty()) {
	delete data->segments->front();
	data->segments->pop_front();
    }
	for (std::vector<std::unique_ptr<ON_2dPointArray> >::iterator group =
		groups.begin(); group != groups.end(); ++group)
	    data->segments->push_back(group->release());
	if (retained_groups)
	    *retained_groups = data->segments->size();
    return true;
}


/* Two adjacent, oppositely directed STEP edge uses with the same exact 3-D
 * locus are a zero-area keyhole excursion, not part of the face boundary.
 * Removing the pair before pcurve construction lets a surrounding closed
 * boundary reach the normal exact trim path.  Topology endpoints and the
 * complete directed curve locus are both proven; merely coincident endpoints
 * are not sufficient. */
static bool
pullback_edges_cancel(const PBCData *first, const PBCData *second,
	double tolerance, std::string *failure = NULL)
{
    if (failure) failure->clear();
    if (!first || !second || !first->edge || !second->edge ||
	    !first->curve || !second->curve || !(tolerance > 0.0))
	return false;
    const int first_start = first->edge->m_vi[first->order_reversed ? 1 : 0];
    const int first_end = first->edge->m_vi[first->order_reversed ? 0 : 1];
    const int second_start = second->edge->m_vi[
	second->order_reversed ? 1 : 0];
    const int second_end = second->edge->m_vi[
	second->order_reversed ? 0 : 1];
    if (first_start != second_end || first_end != second_start)
	return false;

    const ON_Interval first_domain = first->curve->Domain();
    const ON_Interval second_domain = second->curve->Domain();
    if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing())
	return false;
    const double locus_tolerance = std::max(tolerance,
	std::max(first->tolerance, second->tolerance));
    const CurveDistanceEvaluator first_distance(first->curve);
    const CurveDistanceEvaluator second_distance(second->curve);
    if (!first_distance.IsValid() || !second_distance.IsValid())
	return false;
    for (int sample = 0; sample <= kDenseLiftValidationSegments; ++sample) {
	if (brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const double first_fraction = first->order_reversed ?
	    1.0 - fraction : fraction;
	const double second_fraction = second->order_reversed ?
	    1.0 - fraction : fraction;
	const ON_3dPoint first_point = first->curve->PointAt(
	    first_domain.ParameterAt(first_fraction));
	const ON_3dPoint second_point = second->curve->PointAt(
	    second_domain.ParameterAt(second_fraction));
	const double first_to_second = first_point.IsValid() ?
	    second_distance.DistanceTo(first_point, locus_tolerance) : DBL_MAX;
	const double second_to_first = second_point.IsValid() ?
	    first_distance.DistanceTo(second_point, locus_tolerance) : DBL_MAX;
	if (!first_point.IsValid() || !second_point.IsValid() ||
		first_to_second > locus_tolerance ||
		second_to_first > locus_tolerance) {
	    if (failure) {
		std::ostringstream detail;
		detail << "opposite STEP edges #" << first->edge->m_edge_user.i
		    << "/#" << second->edge->m_edge_user.i
		    << " had symmetric locus errors " << first_to_second << '/'
		    << second_to_first
		    << " at sample " << sample;
		*failure = detail.str();
	    }
	    return false;
	}
    }
    return true;
}


static bool
snap_seam_pullback_pair(std::list<PBCData *> &pullbacks, PBCData *first, PBCData *second,
	ON_BrepLoop::TYPE expected_loop_type, double tolerance,
	std::string *failure = NULL)
{
    if (failure)
	failure->clear();
    if (!first || !second || first->surf != second->surf)
	return false;

    struct PairCandidate {
	int direction;
	double first_value;
	double second_value;
	double score;
	double area;
	double gap;
    };
    std::vector<PairCandidate> candidates;
    std::ostringstream candidate_rejections;
    for (int direction = 0; direction < 2; ++direction) {
	if (!first->surf->IsClosed(direction))
	    continue;
	const ON_Interval domain = first->surf->Domain(direction);
	if (!domain.IsIncreasing())
	    continue;
	for (int reverse = 0; reverse < 2; ++reverse) {
	    const double first_value = reverse ? domain.Max() : domain.Min();
	    const double second_value = reverse ? domain.Min() : domain.Max();
	    double first_score = 0.0;
	    double second_score = 0.0;
	    std::string first_failure;
	    std::string second_failure;
	    const bool first_exact = seam_boundary_score(first, direction,
		first_value, tolerance, &first_score, &first_failure);
	    const bool second_exact = seam_boundary_score(second, direction,
		second_value, tolerance, &second_score, &second_failure);
	    if (!first_exact || !second_exact) {
		if (candidate_rejections.tellp() > 0)
		    candidate_rejections << "; ";
		candidate_rejections << "direction " << direction << " sides "
		    << first_value << '/' << second_value << ": "
		    << (!first_exact ? "first " + first_failure :
			"second " + second_failure);
		continue;
	    }
	    double area = 0.0;
	    double gap = 0.0;
	    pullback_loop_metrics(pullbacks, first, first_value, second, second_value,
		direction, domain.Length(), &area, &gap);
	    candidates.push_back({direction, first_value, second_value,
		first_score + second_score, area, gap});
	}
    }

    if (candidates.empty()) {
	if (failure)
	    *failure = "neither closed parameter direction supplied an exact "
		"opposite-boundary pair: " + candidate_rejections.str();
	return false;
	}

    PairCandidate *best = NULL;
    for (std::vector<PairCandidate>::iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate) {
	const bool expected_orientation =
	    (expected_loop_type == ON_BrepLoop::outer && candidate->area > 0.0) ||
	    (expected_loop_type == ON_BrepLoop::inner && candidate->area < 0.0);
	const bool best_orientation = best &&
	    ((expected_loop_type == ON_BrepLoop::outer && best->area > 0.0) ||
	     (expected_loop_type == ON_BrepLoop::inner && best->area < 0.0));
	if (!best || (expected_orientation && !best_orientation) ||
	    (expected_orientation == best_orientation && candidate->gap < best->gap) ||
	    (expected_orientation == best_orientation &&
	     fabs(candidate->gap - best->gap) <= ON_ZERO_TOLERANCE && candidate->score < best->score))
	    best = &*candidate;
    }

    if (!best) {
	if (failure)
	    *failure = "no opposite-boundary candidate survived orientation scoring";
	return false;
	}

    bool changed = false;

    const double period = first->surf->Domain(best->direction).Length();
    ON_2dPoint previous;
    bool have_previous = false;
    for (std::list<PBCData *>::iterator data = pullbacks.begin(); data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	for (std::list<ON_2dPointArray *>::iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (!*segment)
		continue;
	    for (int i = 0; i < (*segment)->Count(); ++i) {
		ON_2dPoint *point = (*segment)->At(i);
		double value = (*point)[best->direction];
		if (*data == first)
		    value = best->first_value;
		else if (*data == second)
		    value = best->second_value;
		else if (have_previous)
		    value += round((previous[best->direction] - value) / period) * period;
		if (fabs((*point)[best->direction] - value) > ON_ZERO_TOLERANCE) {
		    (*point)[best->direction] = value;
		    changed = true;
		}
		previous = *point;
		have_previous = true;
	    }
	}
    }
    /* A seam solver may return a perfectly continuous branch one whole
     * period outside the native domain.  Translate the complete varying
     * segment—not an isolated endpoint—so doubly periodic corner joins close
     * exactly without turning an isoparametric seam into a diagonal trim.
     * Constant boundary segments remain pinned to their selected side. */
    for (std::list<PBCData *>::iterator data = pullbacks.begin();
	    data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments) continue;
	for (std::list<ON_2dPointArray *>::iterator segment =
		(*data)->segments->begin(); segment != (*data)->segments->end();
		++segment) {
	    if (!*segment || (*segment)->Count() == 0) continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!first->surf->IsClosed(direction)) continue;
		const ON_Interval domain = first->surf->Domain(direction);
		const double direction_period = domain.Length();
		if (!(direction_period > ON_ZERO_TOLERANCE)) continue;
		double minimum = (**segment)[0][direction];
		double maximum = minimum;
		for (int point = 1; point < (*segment)->Count(); ++point) {
		    minimum = std::min(minimum, (**segment)[point][direction]);
		    maximum = std::max(maximum, (**segment)[point][direction]);
		}
		if (maximum - minimum <= ON_ZERO_TOLERANCE) continue;
		const double center = 0.5 * (minimum + maximum);
		const double shift = round((domain.Mid() - center) /
		    direction_period) * direction_period;
		if (fabs(shift) > ON_ZERO_TOLERANCE) {
		    for (int point = 0; point < (*segment)->Count(); ++point)
			(**segment)[point][direction] += shift;
		    changed = true;
		}
		/* A closed 3-D curve evaluates to the same spatial point at both
		 * parameter ends, and the closest-point solver may return the same
		 * UV boundary copy for both.  Select each endpoint's periodic image
		 * from its adjacent interior sample so the complete trim retains its
		 * traversal instead of acquiring a one-period endpoint spike. */
		if ((*segment)->Count() >= 2) {
		    ON_2dPoint *start = (*segment)->At(0);
		    ON_2dPoint *next = (*segment)->At(1);
		    ON_2dPoint *end = (*segment)->At((*segment)->Count() - 1);
		    ON_2dPoint *preceding = (*segment)->At((*segment)->Count() - 2);
		    const double start_shift = round(((*next)[direction] -
			(*start)[direction]) / direction_period) * direction_period;
		    const double end_shift = round(((*preceding)[direction] -
			(*end)[direction]) / direction_period) * direction_period;
		    (*start)[direction] += start_shift;
		    (*end)[direction] += end_shift;
		    changed = changed || fabs(start_shift) > ON_ZERO_TOLERANCE ||
			fabs(end_shift) > ON_ZERO_TOLERANCE;
		}
	    }
	}
    }
    if (!changed && failure) {
	std::ostringstream detail;
	detail << "the selected direction " << best->direction
	    << " was already on boundaries " << best->first_value << '/'
	    << best->second_value;
	*failure = detail.str();
    }
    return changed;
}


static size_t
restore_closed_pullback_topology_vertices(std::list<PBCData *> &pullbacks,
    const ON_Brep *brep, double tolerance)
{
    if (!brep || !(tolerance > 0.0))
	return 0;

    size_t restored = 0;
    for (std::list<PBCData *>::iterator current = pullbacks.begin();
	    current != pullbacks.end(); ++current) {
	PBCData *data = *current;
	if (!data || !data->edge || !data->surf || !data->segments ||
		data->segments->size() != 1 ||
		data->edge->m_vi[0] != data->edge->m_vi[1] ||
		data->edge->m_vi[0] < 0 ||
		data->edge->m_vi[0] >= brep->m_V.Count())
	    continue;
	ON_2dPointArray *samples = data->segments->front();
	if (!samples || samples->Count() < 4)
	    continue;

	const ON_3dPoint &vertex = brep->m_V[data->edge->m_vi[0]].point;
	const double topology_tolerance = std::max(tolerance,
	    std::max(data->tolerance, data->edge->m_tolerance));
	const auto lifts_to_vertex = [data, &vertex, topology_tolerance](
		const ON_2dPoint &uv) {
	    const ON_3dPoint lift = data->surf->PointAt(uv.x, uv.y);
	    return lift.IsValid() &&
		lift.DistanceTo(vertex) <= topology_tolerance;
	};
	if (lifts_to_vertex((*samples)[0]) &&
		lifts_to_vertex((*samples)[samples->Count() - 1]))
	    continue;

	/* The legacy closed-surface seam resolver cyclically rotates a one-edge
	 * loop to begin at the surface seam.  The original closed curve's endpoint
	 * remains in that rotated chain (intersegment duplicate removal may retain
	 * it only once).  Cut only at a sample whose lift independently matches the
	 * declared STEP vertex; a merely nearby interior sample is not enough
	 * authority to move the topology vertex. */
	int cut = -1;
	for (int point = 1; point + 1 < samples->Count(); ++point) {
	    if (lifts_to_vertex((*samples)[point])) {
		cut = point;
		break;
	    }
	}
	int cut_segment = -1;
	ON_2dPoint inserted_cut = ON_2dPoint::UnsetPoint;
	if (cut < 0) {
	    double best_distance = DBL_MAX;
	    int best_subdivision = -1;
	    for (int segment = 0; segment + 1 < samples->Count(); ++segment) {
		const ON_2dPoint &first = (*samples)[segment];
		const ON_2dPoint &second = (*samples)[segment + 1];
		for (int subdivision = 0;
			subdivision <= kTopologyVertexSegmentSearchSubdivisions;
			++subdivision) {
		    const double fraction = static_cast<double>(subdivision) /
			kTopologyVertexSegmentSearchSubdivisions;
		    const ON_2dPoint uv((1.0 - fraction) * first.x +
			fraction * second.x, (1.0 - fraction) * first.y +
			fraction * second.y);
		    const ON_3dPoint lift = data->surf->PointAt(uv.x, uv.y);
		    const double distance = lift.IsValid() ?
			lift.DistanceTo(vertex) : DBL_MAX;
		    if (distance < best_distance) {
			best_distance = distance;
			cut_segment = segment;
			best_subdivision = subdivision;
			inserted_cut = uv;
		    }
		}
	    }
	    if (cut_segment >= 0 && best_subdivision >= 0) {
		const ON_2dPoint &first = (*samples)[cut_segment];
		const ON_2dPoint &second = (*samples)[cut_segment + 1];
		double lower = std::max(0.0,
		    static_cast<double>(best_subdivision - 1) /
			kTopologyVertexSegmentSearchSubdivisions);
		double upper = std::min(1.0,
		    static_cast<double>(best_subdivision + 1) /
			kTopologyVertexSegmentSearchSubdivisions);
		for (int iteration = 0;
			iteration < kTopologyVertexSegmentRefinementIterations;
			++iteration) {
		    const double left = (2.0 * lower + upper) / 3.0;
		    const double right = (lower + 2.0 * upper) / 3.0;
		    const ON_2dPoint left_uv((1.0 - left) * first.x +
			left * second.x, (1.0 - left) * first.y +
			left * second.y);
		    const ON_2dPoint right_uv((1.0 - right) * first.x +
			right * second.x, (1.0 - right) * first.y +
			right * second.y);
		    const ON_3dPoint left_lift = data->surf->PointAt(
			left_uv.x, left_uv.y);
		    const ON_3dPoint right_lift = data->surf->PointAt(
			right_uv.x, right_uv.y);
		    const double left_distance = left_lift.IsValid() ?
			left_lift.DistanceTo(vertex) : DBL_MAX;
		    const double right_distance = right_lift.IsValid() ?
			right_lift.DistanceTo(vertex) : DBL_MAX;
		    if (left_distance <= right_distance)
			upper = right;
		    else
			lower = left;
		}
		const double fraction = 0.5 * (lower + upper);
		inserted_cut = ON_2dPoint((1.0 - fraction) * first.x +
		    fraction * second.x, (1.0 - fraction) * first.y +
		    fraction * second.y);
	    }
	    if (cut_segment < 0 || !inserted_cut.IsValid() ||
		    !lifts_to_vertex(inserted_cut))
		continue;
	}

	ON_2dPointArray rotated;
	rotated.Reserve(samples->Count());
	bool exact_translation = true;
	const double lift_tolerance = std::max(tolerance,
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
	const auto append_unwrapped = [data, &rotated, &exact_translation,
		lift_tolerance](const ON_2dPoint &source) {
	    ON_2dPoint shifted(source);
	    if (rotated.Count() > 0) {
		const ON_2dPoint &previous = rotated[rotated.Count() - 1];
		for (int direction = 0; direction < 2; ++direction) {
		    if (!data->surf->IsClosed(direction))
			continue;
		    const double period = data->surf->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			shifted[direction] += round((previous[direction] -
			    shifted[direction]) / period) * period;
		}
		const ON_3dPoint source_lift = data->surf->PointAt(
		    source.x, source.y);
		const ON_3dPoint shifted_lift = data->surf->PointAt(
		    shifted.x, shifted.y);
		if (!source_lift.IsValid() || !shifted_lift.IsValid() ||
			shifted_lift.DistanceTo(source_lift) > lift_tolerance) {
		    exact_translation = false;
		    return;
		}
		if (shifted.DistanceTo(previous) <= ON_ZERO_TOLERANCE)
		    return;
	    }
	    rotated.Append(shifted);
	};
	if (cut >= 0) {
	    for (int point = cut; exact_translation &&
		    point < samples->Count(); ++point)
		append_unwrapped((*samples)[point]);
	    for (int point = 0; exact_translation && point <= cut; ++point)
		append_unwrapped((*samples)[point]);
	} else {
	    append_unwrapped(inserted_cut);
	    for (int point = cut_segment + 1; exact_translation &&
		    point < samples->Count(); ++point)
		append_unwrapped((*samples)[point]);
	    for (int point = 0; exact_translation && point <= cut_segment;
		    ++point)
		append_unwrapped((*samples)[point]);
	    append_unwrapped(inserted_cut);
	}
	if (!exact_translation || rotated.Count() < 3 ||
		!lifts_to_vertex(rotated[0]) ||
		!lifts_to_vertex(rotated[rotated.Count() - 1]))
	    continue;

	int full_period_directions = 0;
	bool other_direction_continuous = true;
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = data->surf->Domain(direction);
	    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
		kPeriodicParameterSnapFraction *
		std::max(1.0, domain.Length()));
	    const double delta = rotated[rotated.Count() - 1][direction] -
		rotated[0][direction];
	    if (data->surf->IsClosed(direction) && domain.IsIncreasing() &&
		    fabs(fabs(delta) - domain.Length()) <= parameter_guard) {
		++full_period_directions;
	    } else if (fabs(delta) > parameter_guard) {
		other_direction_continuous = false;
	    }
	}
	if (full_period_directions != 1 || !other_direction_continuous)
	    continue;

	*samples = rotated;
	++restored;
    }
    return restored;
}


static size_t
snap_pullback_loop_endpoints(std::list<PBCData *> &pullbacks, const ON_Brep *brep,
    double tolerance, std::string *periodic_rejection = NULL)
{
    struct SegmentRef {
	PBCData *data;
	ON_2dPointArray *samples;
    };
    std::vector<SegmentRef> segments;
    if (periodic_rejection) periodic_rejection->clear();
    if (!brep || tolerance <= 0.0) return 0;
    for (std::list<PBCData *>::iterator data = pullbacks.begin();
	 data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments || !(*data)->surf || !(*data)->edge) continue;
	for (std::list<ON_2dPointArray *>::iterator segment = (*data)->segments->begin();
	     segment != (*data)->segments->end(); ++segment) {
	    if (*segment && (*segment)->Count() >= 2)
		segments.push_back({*data, *segment});
	}
    }
    if (segments.empty()) return 0;

    /* A repeated edge that has already been placed exactly on the two sides
     * of a closed surface is a proven seam bridge.  Its boundary coordinate
     * is authoritative: moving one of its endpoints to an equivalent,
     * off-boundary periodic image makes openNURBS classify the trim as a seam
     * with not_iso geometry. */
    const auto is_pinned_seam_endpoint = [&pullbacks](PBCData *candidate,
	const ON_2dPoint &endpoint) {
	if (!candidate || !candidate->edge || !candidate->surf || !candidate->segments)
	    return false;
	size_t edge_uses = 0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	     data != pullbacks.end(); ++data) {
	    if (*data && (*data)->edge && candidate->edge &&
		    (*data)->edge->m_edge_index == candidate->edge->m_edge_index)
		++edge_uses;
	}
	if (edge_uses != 2)
	    return false;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!candidate->surf->IsClosed(direction))
		continue;
	    const ON_Interval domain = candidate->surf->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		if (fabs(endpoint[direction] - boundary) > ON_ZERO_TOLERANCE)
		    continue;
		bool on_boundary = true;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			candidate->segments->begin();
		     on_boundary && segment != candidate->segments->end(); ++segment) {
		    if (!*segment) {
			on_boundary = false;
			break;
		    }
		    for (int point = 0; point < (*segment)->Count(); ++point) {
			if (fabs((**segment)[point][direction] - boundary) >
				ON_ZERO_TOLERANCE) {
			    on_boundary = false;
			    break;
			}
		    }
		}
		if (on_boundary)
		    return true;
	    }
	}
	return false;
    };

    size_t changed = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
	SegmentRef &current = segments[i];
	SegmentRef &next = segments[(i + 1) % segments.size()];
	ON_2dPoint *end = current.samples->At(current.samples->Count() - 1);
	ON_2dPoint *start = next.samples->At(0);
	if (current.data != next.data && current.data->periodic_pole_cut_after &&
		next.data->periodic_pole_cut_before)
	    continue;
	if (!end || !start || end->DistanceTo(*start) <= ON_ZERO_TOLERANCE ||
	    current.data->surf != next.data->surf)
	    continue;

	int shared_vertex = -1;
	if (current.data != next.data) {
	    for (int current_vertex = 0; current_vertex < 2; ++current_vertex) {
		for (int next_vertex = 0; next_vertex < 2; ++next_vertex) {
		    if (current.data->edge->m_vi[current_vertex] ==
			next.data->edge->m_vi[next_vertex]) {
			if (shared_vertex >= 0 && shared_vertex !=
				current.data->edge->m_vi[current_vertex]) {
			    shared_vertex = -2;
			    break;
			}
			shared_vertex = current.data->edge->m_vi[current_vertex];
		    }
		}
		if (shared_vertex == -2) break;
	    }
	    if (shared_vertex < 0 || shared_vertex >= brep->m_V.Count())
		continue;
	}

	const ON_3dPoint lifted_end = current.data->surf->PointAt(end->x, end->y);
	const ON_3dPoint lifted_start = next.data->surf->PointAt(start->x, start->y);
	if (!lifted_end.IsValid() || !lifted_start.IsValid())
	    continue;
	const double join_tolerance = std::max(tolerance,
	    std::max(current.data->tolerance, next.data->tolerance));
	const double current_topology_tolerance = std::max(tolerance,
	    current.data->tolerance_adjusted ?
		current.data->declared_tolerance : current.data->tolerance);
	const double next_topology_tolerance = std::max(tolerance,
	    next.data->tolerance_adjusted ?
		next.data->declared_tolerance : next.data->tolerance);
	const double shared_endpoint_tolerance = std::max(tolerance,
	    std::min(current_topology_tolerance, next_topology_tolerance));
	const double next_locus_tolerance = std::max(tolerance,
	    next.data->tolerance);
	/* A one-edge closed STEP boundary is cyclically adjacent to itself.  Its
	 * two UV endpoints can therefore be distinct by exactly one surface period
	 * while lifting to the same topology vertex.  That separation is the
	 * authoritative winding needed by the later implicit-periodic-band pass;
	 * translating the complete curve and then snapping its endpoint to itself
	 * erases the winding and leaves an invalid closed pcurve on the wrong
	 * periodic image.  Preserve only a fully proved winding: the same pullback
	 * and sample chain, a closed topology edge, exactly one periodic coordinate
	 * spanning one period, continuity in the other coordinate, and coincident
	 * endpoint lifts.  Internal fragments and ordinary numerical endpoint gaps
	 * continue through the bounded join repair below. */
	bool closed_singleton_full_period = false;
	if (current.data == next.data && current.samples == next.samples &&
		current.data->edge->m_vi[0] == current.data->edge->m_vi[1] &&
		lifted_end.DistanceTo(lifted_start) <= join_tolerance) {
	    int full_period_directions = 0;
	    bool other_direction_continuous = true;
	    for (int direction = 0; direction < 2; ++direction) {
		const ON_Interval domain = current.data->surf->Domain(direction);
		const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
		    kPeriodicParameterSnapFraction *
		    std::max(1.0, domain.Length()));
		const double delta = (*end)[direction] - (*start)[direction];
		if (current.data->surf->IsClosed(direction) &&
			domain.IsIncreasing() &&
			fabs(fabs(delta) - domain.Length()) <= parameter_guard) {
		    ++full_period_directions;
		} else if (fabs(delta) > parameter_guard) {
		    other_direction_continuous = false;
		}
	    }
	    closed_singleton_full_period = full_period_directions == 1 &&
		other_direction_continuous;
	}
	if (closed_singleton_full_period)
	    continue;
	/* A measured source edge/surface mismatch may enlarge join_tolerance, but
	 * that must never authorize changing the surface locus of a neighboring
	 * pcurve.  Periodic branch changes for open edges are parameterization
	 * repairs: every shifted sample must lift to the same point as the original
	 * sample within the declared model uncertainty (with only a numerical
	 * floor).  Closed topology edges retain their whole-edge source-locus proof;
	 * they are subsequently seam-split and densely validated. */
	const double branch_lift_tolerance = std::max(tolerance,
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
	const bool next_is_closed_topology_edge = next.data->edge &&
	    next.data->edge->m_vi[0] == next.data->edge->m_vi[1];
	/* When adjacent exact endpoints differ by one closed-surface period,
	 * moving only the endpoint creates a one-period spike which the subsequent
	 * within-segment unwrap must undo.  Prefer translating the complete next
	 * trim onto the adjacent native boundary when that branch remains inside
	 * the surface domain and every translated sample still lifts to its STEP
	 * edge.  This keeps both joins of a short seam-boundary trim coherent. */
	if (shared_vertex >= 0 ||
		lifted_end.DistanceTo(lifted_start) <= join_tolerance) {
	    const CurveDistanceEvaluator next_edge_distance(next.data->curve);
	    for (int direction = 0; direction < 2; ++direction) {
		if (!next.data->surf->IsClosed(direction) ||
			!next_edge_distance.IsValid())
		    continue;
		const ON_Interval domain = next.data->surf->Domain(direction);
		const double period = domain.Length();
		if (!(period > ON_ZERO_TOLERANCE)) continue;
		const double branch_delta = (*end)[direction] - (*start)[direction];
		const int period_count = static_cast<int>(round(branch_delta / period));
		const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
		    kPeriodicParameterSnapFraction * std::max(1.0, period));
		if (std::abs(period_count) != 1 ||
			fabs(branch_delta - period_count * period) > parameter_guard)
		    continue;

		std::vector<ON_2dPointArray> shifted_segments;
		shifted_segments.reserve(next.data->segments->size());
		bool exact = true;
		for (std::list<ON_2dPointArray *>::const_iterator segment =
			next.data->segments->begin(); exact &&
			segment != next.data->segments->end(); ++segment) {
		    if (!*segment || (*segment)->Count() < 2) {
			exact = false;
			break;
		    }
		    shifted_segments.push_back(ON_2dPointArray(**segment));
		    ON_2dPointArray &shifted = shifted_segments.back();
		    for (int point = 0; exact && point < shifted.Count(); ++point) {
			const ON_3dPoint original_lift = next.data->surf->PointAt(
			    shifted[point].x, shifted[point].y);
			shifted[point][direction] += period_count * period;
			if (shifted[point][direction] < domain.Min() - parameter_guard ||
				shifted[point][direction] > domain.Max() + parameter_guard) {
			    if (periodic_rejection) {
				std::ostringstream reason;
				reason << "STEP edge #"
				    << (next.data->edge ?
					next.data->edge->m_edge_user.i : 0)
				    << " period translation left domain at "
				    << shifted[point][direction] << " ("
				    << domain.Min() << ':' << domain.Max() << ')';
				if (!periodic_rejection->empty()) *periodic_rejection += "; ";
				*periodic_rejection += reason.str();
			    }
			    exact = false;
			    break;
			}
			shifted[point][direction] = std::max(domain.Min(),
			    std::min(domain.Max(), shifted[point][direction]));
			const ON_3dPoint lift = next.data->surf->PointAt(
			    shifted[point].x, shifted[point].y);
			const double lift_error = lift.IsValid() ?
			    next_edge_distance.DistanceTo(lift,
				next_locus_tolerance) : DBL_MAX;
			const double branch_lift_error = lift.IsValid() &&
			    original_lift.IsValid() ?
			    lift.DistanceTo(original_lift) : DBL_MAX;
			exact = lift.IsValid() &&
			    lift_error <= next_locus_tolerance &&
			    (next_is_closed_topology_edge ||
				branch_lift_error <= branch_lift_tolerance);
			if (!exact && periodic_rejection) {
			    std::ostringstream reason;
			    reason << "STEP edge #"
				<< (next.data->edge ? next.data->edge->m_edge_user.i : 0)
				<< " period-translated lift missed its source by "
				<< lift_error << " (limit " << next_locus_tolerance << ')';
			    if (!periodic_rejection->empty()) *periodic_rejection += "; ";
			    *periodic_rejection += reason.str();
			}
		    }
		}
		/* Closest-point samples for an exact seam-boundary edge can wander a
		 * little off the constant parameter even though the fitted trim and
		 * its complete 3-D locus are isoparametric.  If a literal period shift
		 * would leave the native domain, prove the adjacent boundary itself
		 * against the STEP edge and use that constant branch. */
		if (!exact && (fabs((*end)[direction] - domain.Min()) <=
			parameter_guard || fabs((*end)[direction] - domain.Max()) <=
			parameter_guard)) {
		    shifted_segments.clear();
		    exact = true;
		    const double boundary = fabs((*end)[direction] - domain.Min()) <=
			parameter_guard ? domain.Min() : domain.Max();
		    for (std::list<ON_2dPointArray *>::const_iterator segment =
			    next.data->segments->begin(); exact &&
			    segment != next.data->segments->end(); ++segment) {
			if (!*segment || (*segment)->Count() < 2) {
			    exact = false;
			    break;
			}
			shifted_segments.push_back(ON_2dPointArray(**segment));
			ON_2dPointArray &shifted = shifted_segments.back();
			for (int point = 0; exact && point < shifted.Count(); ++point) {
			    const ON_3dPoint original_lift = next.data->surf->PointAt(
				shifted[point].x, shifted[point].y);
			    shifted[point][direction] = boundary;
			    const ON_3dPoint lift = next.data->surf->PointAt(
				shifted[point].x, shifted[point].y);
			    const double lift_error = lift.IsValid() ?
				next_edge_distance.DistanceTo(lift,
				    next_locus_tolerance) : DBL_MAX;
			    const double branch_lift_error = lift.IsValid() &&
				original_lift.IsValid() ?
				lift.DistanceTo(original_lift) : DBL_MAX;
			    exact = lift.IsValid() &&
				lift_error <= next_locus_tolerance &&
				(next_is_closed_topology_edge ||
				    branch_lift_error <= branch_lift_tolerance);
			    if (!exact && periodic_rejection) {
				std::ostringstream reason;
				reason << "STEP edge #"
				    << (next.data->edge ?
					next.data->edge->m_edge_user.i : 0)
				    << " adjacent seam-boundary lift missed its source by "
				    << lift_error << " (limit " << next_locus_tolerance << ')';
				if (!periodic_rejection->empty()) *periodic_rejection += "; ";
				*periodic_rejection += reason.str();
			    }
			}
		    }
		}
		if (!exact) continue;
		std::list<ON_2dPointArray *>::iterator segment =
		    next.data->segments->begin();
		for (size_t shifted = 0; shifted < shifted_segments.size();
			++shifted, ++segment)
		    **segment = shifted_segments[shifted];
		start = next.samples->At(0);
		++changed;
		break;
	    }
	    if (end->DistanceTo(*start) <= ON_ZERO_TOLERANCE)
		continue;
	}

	/* A compact STEP face on a singly-periodic surface can leave its
	 * topological cut implicit.  In that representation, two distinct
	 * boundary edges meet at the same 3-D vertex while their pcurves end on
	 * adjacent parameter images exactly one period apart.  If translating the
	 * complete following pcurve onto the preceding image was not possible
	 * above, changing only its endpoint would bend an otherwise exact
	 * isoparametric edge through a complete revolution and erase the loop's
	 * authoritative winding.  Preserve this proven cut for the face-level
	 * periodic-band repair, which splits the exact 3-D boundary at the native
	 * surface seam and installs the two required OpenNURBS seam uses.
	 *
	 * Require distinct STEP edges, an asserted shared vertex, exact endpoint
	 * lifts, one and only one full-period coordinate jump, and continuity in
	 * the other parameter direction.  Ordinary numerical endpoint gaps still
	 * proceed to the bounded common-point repair below. */
	bool implicit_periodic_cut = false;
	if (current.data != next.data && current.data->edge != next.data->edge &&
		shared_vertex >= 0 &&
		lifted_end.DistanceTo(brep->m_V[shared_vertex].point) <=
		    shared_endpoint_tolerance &&
		lifted_start.DistanceTo(brep->m_V[shared_vertex].point) <=
		    shared_endpoint_tolerance) {
	    int full_period_directions = 0;
	    bool other_direction_continuous = true;
	    for (int direction = 0; direction < 2; ++direction) {
		const double delta = (*end)[direction] - (*start)[direction];
		const ON_Interval domain = current.data->surf->Domain(direction);
		const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
		    kPeriodicParameterSnapFraction *
		    std::max(1.0, domain.Length()));
		if (current.data->surf->IsClosed(direction) &&
			domain.IsIncreasing() &&
			fabs(fabs(delta) - domain.Length()) <= parameter_guard) {
		    ++full_period_directions;
		} else if (fabs(delta) > parameter_guard) {
		    other_direction_continuous = false;
		}
	    }
	    implicit_periodic_cut = full_period_directions == 1 &&
		other_direction_continuous;
	}
	if (implicit_periodic_cut)
	    continue;
	ON_2dPoint common = *end;
	const bool end_is_pinned_seam = is_pinned_seam_endpoint(current.data, *end);
	const bool start_is_pinned_seam = is_pinned_seam_endpoint(next.data, *start);
	/* At a doubly periodic corner both incident curves are authoritative
	 * isoparametric seams.  Moving either isolated endpoint would make that
	 * complete curve non-isoparametric; their periodic branch selection is
	 * handled by whole-segment normalization above. */
	if (end_is_pinned_seam && start_is_pinned_seam) {
	    ON_2dPointArray *next_samples = next.samples;
	    ON_2dPoint *opposite = next_samples && next_samples->Count() > 1 ?
		next_samples->At(next_samples->Count() - 1) : NULL;
	    /* Closed STEP edges have coincident 3-D vertices, so their numerical
	     * pullback can choose either traversal around a periodic seam.  When
	     * the opposite endpoint is the exact pspace continuation, reverse the
	     * complete trim (and its 3-D relation flag) instead of moving one
	     * authoritative seam endpoint. */
	    if (opposite && end->DistanceTo(*opposite) + ON_ZERO_TOLERANCE <
		    end->DistanceTo(*start)) {
		next_samples->Reverse();
		next.data->order_reversed = !next.data->order_reversed;
		++changed;
	    }
	    continue;
	}
	if (end_is_pinned_seam != start_is_pinned_seam) {
	    common = end_is_pinned_seam ? *end : *start;
	    if (shared_vertex >= 0 && common.IsValid()) {
		const ON_3dPoint vertex = brep->m_V[shared_vertex].point;
		const ON_3dPoint lifted_common = current.data->surf->PointAt(common.x, common.y);
		if (!lifted_common.IsValid() ||
			lifted_common.DistanceTo(vertex) >
			    shared_endpoint_tolerance) {
		    const ON_2dPoint alternate = end_is_pinned_seam ? *start : *end;
		    const ON_3dPoint lifted_alternate = current.data->surf->PointAt(
			alternate.x, alternate.y);
		    bool preserves_boundary = lifted_alternate.IsValid() &&
			lifted_alternate.DistanceTo(vertex) <=
			    shared_endpoint_tolerance;
		    const ON_2dPoint pinned = end_is_pinned_seam ? *end : *start;
		    PBCData *pinned_data = end_is_pinned_seam ? current.data : next.data;
		    for (int direction = 0; direction < 2 && preserves_boundary; ++direction) {
			if (!current.data->surf->IsClosed(direction))
			    continue;
			const ON_Interval domain = current.data->surf->Domain(direction);
			for (int side = 0; side < 2; ++side) {
			    if (fabs(pinned[direction] - domain[side]) > ON_ZERO_TOLERANCE)
				continue;
			    bool curve_on_boundary = true;
			    for (std::list<ON_2dPointArray *>::const_iterator segment =
				    pinned_data->segments->begin(); curve_on_boundary &&
				    segment != pinned_data->segments->end(); ++segment) {
				if (!*segment) {
				    curve_on_boundary = false;
				    break;
				}
				for (int point = 0; point < (*segment)->Count(); ++point) {
				    if (fabs((**segment)[point][direction] - domain[side]) >
					    ON_ZERO_TOLERANCE) {
					curve_on_boundary = false;
					break;
				    }
				}
			    }
			    if (curve_on_boundary && fabs(alternate[direction] -
				    domain[side]) > ON_ZERO_TOLERANCE) {
				preserves_boundary = false;
				break;
			    }
			}
		    }
		    if (!preserves_boundary)
			continue;
		    common = alternate;
		}
	    }
	} else if (shared_vertex >= 0) {
	    const ON_3dPoint vertex = brep->m_V[shared_vertex].point;
	    const bool end_valid = lifted_end.DistanceTo(vertex) <=
		shared_endpoint_tolerance;
	    const bool start_valid = lifted_start.DistanceTo(vertex) <=
		shared_endpoint_tolerance;
	    if (!end_valid || !start_valid) {
		if (end_valid) {
		    common = *end;
		} else if (start_valid) {
		    common = *start;
		} else {
		    if (!current.data->context)
			current.data->context = std::make_shared<brlcad::PullbackContext>();
		    ON_3dPoint projected;
		    double distance = DBL_MAX;
		    if (!current.data->context->SurfaceClosestPoint(current.data->surf,
		    vertex, common, projected, distance, 0,
		    shared_endpoint_tolerance,
		    shared_endpoint_tolerance) ||
		    distance > shared_endpoint_tolerance)
		continue;
		    /* Select the periodic image nearest the existing loop endpoint. */
		    for (int direction = 0; direction < 2; ++direction) {
			if (!current.data->surf->IsClosed(direction)) continue;
			const double period = current.data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    common[direction] += round(((*end)[direction] - common[direction]) /
				period) * period;
		    }
		}
	    }
	} else if (lifted_end.DistanceTo(lifted_start) >
		shared_endpoint_tolerance) {
	    continue;
	}

	/* The two UVs are proven to represent the same topological 3-D point
	 * within the model uncertainty.  Use the preceding trim's endpoint so
	 * openNURBS sees an exactly closed parameter-space loop. */
	*end = common;
	*start = common;
	++changed;
    }
    return changed;
}


static bool
pullback_has_collapsed_full_period(const PBCData *data,
	const ON_2dPointArray &samples)
{
    if (!data || !data->edge || !data->surf || samples.Count() < 2 ||
	    data->edge->m_vi[0] != data->edge->m_vi[1])
	return false;

    ON_2dPoint minimum(samples[0]);
    ON_2dPoint maximum(samples[0]);
    for (int sample = 1; sample < samples.Count(); ++sample) {
	minimum.x = std::min(minimum.x, samples[sample].x);
	minimum.y = std::min(minimum.y, samples[sample].y);
	maximum.x = std::max(maximum.x, samples[sample].x);
	maximum.y = std::max(maximum.y, samples[sample].y);
    }

    for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	const int varying_direction = 1 - fixed_direction;
	if (!data->surf->IsClosed(varying_direction))
	    continue;
	const ON_Interval fixed_domain = data->surf->Domain(fixed_direction);
	const ON_Interval varying_domain = data->surf->Domain(varying_direction);
	if (!fixed_domain.IsIncreasing() || !varying_domain.IsIncreasing())
	    continue;
	const double fixed_limit = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, fixed_domain.Length() *
	    kCollapsedFullPeriodMaximumRelativeSpan);
	const double varying_limit = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, varying_domain.Length() *
	    kCollapsedFullPeriodMaximumRelativeSpan);
	if (maximum[fixed_direction] - minimum[fixed_direction] <= fixed_limit &&
		maximum[varying_direction] - minimum[varying_direction] <=
		    varying_limit)
	    return true;
    }
    return false;
}


static ON_Curve *
closed_edge_iso_line(PBCData *data, const ON_2dPointArray &samples,
    double tolerance, std::string *failure = NULL)
{
    if (failure)
	failure->clear();
    if (!data || !data->edge || !data->surf || !data->curve || samples.Count() < 2 ||
	data->edge->m_vi[0] != data->edge->m_vi[1] || tolerance <= 0.0)
	return NULL;

    struct IsoCandidate {
	int direction;
	double value;
	double score;
	ON_2dPoint start;
	ON_2dPoint end;
    };
    std::vector<IsoCandidate> candidates;
    /* Most analytic STEP curves and their surface isocurves use compatible
     * normalized parameterizations.  A point-for-point comparison is then a
     * stronger proof than a closest-curve search and is much cheaper.  Create
     * the parameterization-independent evaluator lazily only for rational
     * curves whose parameter speed differs from the surface isocurve. */
    std::unique_ptr<CurveDistanceEvaluator> source_distance;
    double best_forward_error = DBL_MAX;
    double best_reverse_error = DBL_MAX;
    double best_locus_error = DBL_MAX;
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval domain = data->surf->Domain(direction);
	if (!domain.IsIncreasing())
	    continue;
	std::vector<double> fixed_values;
	fixed_values.push_back(domain.Min());
	fixed_values.push_back(domain.Max());
	/* A complete closed boundary need not lie on a surface seam.  For
	 * example, either boundary of a toroidal face band is an interior
	 * constant-u circle.  Retain the pullback's measured constant coordinate
	 * as a candidate in addition to the two native seam values; dense locus
	 * validation below rejects it unless it is truly isoparametric. */
	double measured_value = 0.0;
	for (int sample = 0; sample < samples.Count(); ++sample)
	    measured_value += samples[sample][direction];
	measured_value /= samples.Count();
	if (data->surf->IsClosed(direction) && domain.Length() > ON_ZERO_TOLERANCE)
	    measured_value += round((domain.Mid() - measured_value) /
		domain.Length()) * domain.Length();
	if (measured_value >= domain.Min() && measured_value <= domain.Max() &&
		fabs(measured_value - domain.Min()) > ON_ZERO_TOLERANCE &&
		fabs(measured_value - domain.Max()) > ON_ZERO_TOLERANCE)
	    fixed_values.push_back(measured_value);
	for (std::vector<double>::const_iterator fixed = fixed_values.begin();
		fixed != fixed_values.end(); ++fixed) {
	    const double value = *fixed;
	    ON_2dPoint start = samples[0];
	    ON_2dPoint end = samples[samples.Count() - 1];
	    start[direction] = value;
	    end[direction] = value;
	    const int varying_direction = 1 - direction;
	    if (fabs(end[varying_direction] - start[varying_direction]) <=
		    ON_ZERO_TOLERANCE) {
		/* Coincident pullback endpoints on a closed 3-D edge can select the
		 * same periodic image and erase the visible UV winding.  Restore only
		 * the complete native period; the dense lift comparison below proves
		 * whether this is the exact isoparametric boundary or merely a closed
		 * non-isoparametric curve. */
		if (!data->surf->IsClosed(varying_direction))
		    continue;
		const ON_Interval varying_domain =
		    data->surf->Domain(varying_direction);
		if (!varying_domain.IsIncreasing())
		    continue;
		start[varying_direction] = varying_domain.Min();
		end[varying_direction] = varying_domain.Max();
	    }

	    /* The existing samples already lift to the STEP edge.  If moving only
	     * their proposed fixed coordinate onto this isocurve changes those
	     * lifts by more than the two-sided tolerance allowance, the source edge
	     * cannot occupy this isocurve.  Run this rejection-only gate before the
	     * more expensive 64-segment candidate evaluation. */
	    const double lift_gate_tolerance =
		kClosedIsoCandidateLiftGateToleranceMultiplier *
		std::max(tolerance, data->tolerance);
	    bool candidate_possible = true;
	    for (int i = 0; candidate_possible &&
		    i <= kClosedIsoCandidateRejectionSegments; ++i) {
		if (brlcad::PullbackWorkCancelled()) {
		    candidate_possible = false;
		    break;
		}
		const int sample_index = static_cast<int>(round(
		    static_cast<double>(i) * (samples.Count() - 1) /
		    kClosedIsoCandidateRejectionSegments));
		ON_2dPoint candidate_uv(samples[sample_index]);
		candidate_uv[direction] = value;
		const ON_3dPoint original_lift = data->surf->PointAt(
		    samples[sample_index].x, samples[sample_index].y);
		const ON_3dPoint candidate_lift = data->surf->PointAt(
		    candidate_uv.x, candidate_uv.y);
		if (!original_lift.IsValid() || !candidate_lift.IsValid() ||
			original_lift.DistanceTo(candidate_lift) >
			    lift_gate_tolerance)
		    candidate_possible = false;
	    }
	    if (!candidate_possible)
		continue;

	    bool evaluations_valid = true;
	    double forward_error = 0.0;
	    double reverse_error = 0.0;
	    const ON_Interval curve_domain = data->curve->Domain();
	    for (int i = 0; i <= kClosedIsoCandidateValidationSegments; ++i) {
		if (brlcad::PullbackWorkCancelled()) {
		    evaluations_valid = false;
		    break;
		}
		const double parameter = static_cast<double>(i) /
		    kClosedIsoCandidateValidationSegments;
		const ON_2dPoint uv = (1.0 - parameter) * start + parameter * end;
		const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
		if (!lifted.IsValid()) {
		    evaluations_valid = false;
		    break;
		}
		const double candidate_forward_error = lifted.DistanceTo(
		    data->curve->PointAt(curve_domain.ParameterAt(parameter)));
		const double candidate_reverse_error = lifted.DistanceTo(
		    data->curve->PointAt(curve_domain.ParameterAt(1.0 - parameter)));
		forward_error = std::max(forward_error, candidate_forward_error);
		reverse_error = std::max(reverse_error, candidate_reverse_error);
	    }
	    bool locus_valid = evaluations_valid &&
		std::min(forward_error, reverse_error) <= tolerance;
	    double locus_error = std::min(forward_error, reverse_error);
	    if (evaluations_valid && !locus_valid) {
		/* Before searching the (potentially many-span) STEP curve for every
		 * candidate point, prove that a small set of exact source points even
		 * belongs to the complete surface isocurve.  A failed membership query
		 * is a conclusive rejection; passing it is only a filter and the full
		 * bidirectional locus validation below remains mandatory. */
		std::unique_ptr<ON_Curve> candidate_iso(
		    data->surf->IsoCurve(varying_direction, value));
		CurveDistanceEvaluator candidate_distance(candidate_iso.get());
		candidate_possible = candidate_iso && candidate_iso->IsValid() &&
		    candidate_distance.IsValid();
		for (int i = 0; candidate_possible &&
			i <= kClosedIsoCandidateRejectionSegments; ++i) {
		    if (brlcad::PullbackWorkCancelled()) {
			candidate_possible = false;
			break;
		    }
		    const double parameter = static_cast<double>(i) /
			kClosedIsoCandidateRejectionSegments;
		    const ON_3dPoint source_point = data->curve->PointAt(
			curve_domain.ParameterAt(parameter));
		    const double membership_error = source_point.IsValid() ?
			candidate_distance.DistanceTo(source_point, tolerance) : DBL_MAX;
		    locus_error = std::max(locus_error, membership_error);
		    if (membership_error > tolerance)
			candidate_possible = false;
		}
		if (!candidate_possible) {
		    best_forward_error = std::min(best_forward_error, forward_error);
		    best_reverse_error = std::min(best_reverse_error, reverse_error);
		    best_locus_error = std::min(best_locus_error, locus_error);
		    continue;
		}
		if (!source_distance) {
		    source_distance.reset(new CurveDistanceEvaluator(data->curve));
		    if (!source_distance->IsValid()) {
			if (failure)
			    *failure = "the exact closed edge does not support locus-distance evaluation";
			return NULL;
		    }
		}
		locus_valid = true;
		locus_error = 0.0;
		for (int i = 0; i <= kClosedIsoCandidateValidationSegments; ++i) {
		    if (brlcad::PullbackWorkCancelled()) {
			locus_valid = false;
			break;
		    }
		    const double parameter = static_cast<double>(i) /
			kClosedIsoCandidateValidationSegments;
		    const ON_2dPoint uv = (1.0 - parameter) * start + parameter * end;
		    const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
		    const double candidate_locus_error = lifted.IsValid() ?
			source_distance->DistanceTo(lifted, tolerance) : DBL_MAX;
		    locus_error = std::max(locus_error, candidate_locus_error);
		    if (candidate_locus_error > tolerance)
			locus_valid = false;
		}
	    }
	    best_forward_error = std::min(best_forward_error, forward_error);
	    best_reverse_error = std::min(best_reverse_error, reverse_error);
	    best_locus_error = std::min(best_locus_error, locus_error);
	    if (!locus_valid)
		continue;
	    /* A rational NURBS circle and the analytic surface isoparameter do
	     * not generally advance at the same speed.  The locus-distance proof
	     * above establishes exactness independently of parameterization; the
	     * synchronized comparisons are used only to select traversal direction. */
	    const bool candidate_matches_reversed_edge =
		reverse_error < forward_error;
	    if (candidate_matches_reversed_edge != data->order_reversed)
		std::swap(start, end);

	    double score = 0.0;
	    for (int i = 0; i < samples.Count(); ++i)
		score += fabs(samples[i][direction] - value) / domain.Length();
	    candidates.push_back({direction, value, score, start, end});
	}
    }

    IsoCandidate *best = NULL;
    for (std::vector<IsoCandidate>::iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate) {
	if (!best || candidate->score < best->score)
	    best = &*candidate;
    }
    if (!best && failure) {
	std::ostringstream detail;
	detail << "no native full-period isoparametric boundary matched the exact "
	    << "closed edge (best locus error " << best_locus_error
	    << ", forward/reverse parameterized errors "
	    << best_forward_error << '/' << best_reverse_error
	    << ", tolerance " << tolerance << ')';
	*failure = detail.str();
    }
    return best ? new ON_LineCurve(best->start, best->end) : NULL;
}


/* A plane has an exact affine inverse, so do not send planar edge curves
 * through the iterative closest-point pullback.  Besides being faster, this
 * handles high-degree closed and half-closed curves whose coincident end
 * points can make an unseeded numerical search ambiguous.  The lift check is
 * deliberately retained: if an exporter supplied inconsistent topology, the
 * caller falls back to the bounded general pullback instead of accepting it. */
static PBCData *
exact_planar_pullback(const ON_Surface *surface, const ON_Curve *curve,
    double declared_tolerance, double maximum_tolerance,
    ON_Curve **exact_curve)
{
    if (exact_curve) *exact_curve = NULL;
    const ON_PlaneSurface *plane_surface = ON_PlaneSurface::Cast(surface);

    if (!plane_surface || !curve || !exact_curve ||
	    declared_tolerance <= 0.0 || maximum_tolerance < declared_tolerance)
	return NULL;

    const ON_Plane &plane = plane_surface->m_plane;
    ON_Xform world_to_plane(ON_Xform::IdentityTransformation);
    if (!world_to_plane.ChangeBasis(ON_Plane::World_xy, plane))
	return NULL;

    ON_Curve *pcurve = curve->DuplicateCurve();
    if (!pcurve || !pcurve->Transform(world_to_plane)) {
	delete pcurve;
	return NULL;
    }

    ON_Xform plane_to_parameter(ON_Xform::IdentityTransformation);
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval extents = plane_surface->Extents(direction);
	const ON_Interval domain = plane_surface->Domain(direction);
	if (!extents.IsIncreasing() || !domain.IsIncreasing()) {
	    delete pcurve;
	    return NULL;
	}
	const double scale = domain.Length() / extents.Length();
	plane_to_parameter.m_xform[direction][direction] = scale;
	plane_to_parameter.m_xform[direction][3] = domain.Min() - scale * extents.Min();
    }
    if (!pcurve->Transform(plane_to_parameter) || !pcurve->ChangeDimension(2)) {
	delete pcurve;
	return NULL;
    }

    const ON_Interval curve_domain = curve->Domain();
    const ON_Interval pcurve_domain = pcurve->Domain();
    double forward_error = 0.0;
    double reverse_error = 0.0;
    for (int sample = 0; sample <= kDenseLiftValidationSegments; ++sample) {
	const double dense_normalized = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const ON_3dPoint point = curve->PointAt(
	    curve_domain.ParameterAt(dense_normalized));
	const ON_3dPoint forward_uv = pcurve->PointAt(
	    pcurve_domain.ParameterAt(dense_normalized));
	const ON_3dPoint reverse_uv = pcurve->PointAt(
	    pcurve_domain.ParameterAt(1.0 - dense_normalized));
	const ON_3dPoint forward_lift = surface->PointAt(forward_uv.x,
	    forward_uv.y);
	const ON_3dPoint reverse_lift = surface->PointAt(reverse_uv.x,
	    reverse_uv.y);
	if (!point.IsValid() || !forward_lift.IsValid() ||
		!reverse_lift.IsValid()) {
	    forward_error = DBL_MAX;
	    reverse_error = DBL_MAX;
	    break;
	}
	forward_error = std::max(forward_error,
	    point.DistanceTo(forward_lift));
	reverse_error = std::max(reverse_error,
	    point.DistanceTo(reverse_lift));
    }
    const bool reverse = reverse_error < forward_error;
    const double measured_error = reverse ? reverse_error : forward_error;
    double effective_tolerance = declared_tolerance;
    if (measured_error > declared_tolerance)
	effective_tolerance = measured_error * kMeasuredToleranceSafetyFactor;
    if (!ON_IsValid(measured_error) || effective_tolerance > maximum_tolerance) {
	delete pcurve;
	return NULL;
    }
    if (reverse && !pcurve->Reverse()) {
	delete pcurve;
	return NULL;
    }

    ON_2dPointArray *samples = new ON_2dPointArray();
    const ON_Interval validated_domain = pcurve->Domain();
    for (int sample = 0; sample <= 64; ++sample) {
	const double normalized = static_cast<double>(sample) / 64.0;
	const ON_3dPoint uv = pcurve->PointAt(validated_domain.ParameterAt(normalized));
	samples->Append(ON_2dPoint(uv.x, uv.y));
    }

    PBCData *data = new PBCData();
    data->tolerance = effective_tolerance;
    data->flatness = effective_tolerance;
    data->curve = curve;
    data->surf = surface;
    data->surftree = NULL;
    data->segments = new std::list<ON_2dPointArray *>();
    data->segments->push_back(samples);
    data->edge = NULL;
    data->order_reversed = false;
    data->samples_source_validated = true;
    data->tolerance_adjusted = measured_error > declared_tolerance;
    data->declared_tolerance = declared_tolerance;
    *exact_curve = pcurve;
    return data;
}


/* Curves along a surface parameter direction have exact linear pcurves.  The
 * generic adaptive closest-point path can lose such an edge when it lies on a
 * periodic seam or a singular NURBS boundary.  Analytic lines are recovered
 * from their endpoints.  Native NURBS surfaces additionally expose their
 * exact knot-boundary isocurves, which lets circles and other non-linear 3-D
 * curves recover the same exact linear parameter path.  Accept a candidate
 * only when every dense validation sample lifts to the original edge within
 * the model uncertainty. */
static PBCData *
exact_isoparametric_line_pullback(const ON_Surface *surface,
    const ON_Curve *curve, double declared_tolerance,
    double maximum_tolerance, const ON_3dPoint &source_start,
    const ON_3dPoint &source_end, ON_Curve **exact_curve,
    int source_edge_id = -1, bool verbose = false)
{
    if (exact_curve) *exact_curve = NULL;
    if (!surface || !curve || !exact_curve || declared_tolerance <= 0.0 ||
	maximum_tolerance < declared_tolerance)
	return NULL;
    const bool linear_curve = curve->IsLinear(declared_tolerance);
    const bool native_line_curve = ON_LineCurve::Cast(curve) != NULL;
    const bool native_nurbs_surface = ON_NurbsSurface::Cast(surface) != NULL;
    /* Closed single-edge loops require an explicit OpenNURBS seam split.  Do
	 * not preempt that topology-aware path with a full-period linear UV curve;
	 * this generalized boundary recovery is for open non-linear STEP edges. */
    const bool distinct_topology_vertices =
	source_start.DistanceTo(source_end) > declared_tolerance;
    if (!linear_curve && !distinct_topology_vertices)
	return NULL;
    if (!linear_curve && !native_nurbs_surface)
	return NULL;

    brlcad::PullbackContext context;
    ON_2dPoint start_uv, end_uv;
    ON_3dPoint start_lift, end_lift;
    double start_distance = DBL_MAX;
    double end_distance = DBL_MAX;

    context.SurfaceClosestPoint(surface, source_start, start_uv,
	start_lift, start_distance, 0, maximum_tolerance,
	maximum_tolerance);
    context.SurfaceClosestPoint(surface, source_end, end_uv,
	end_lift, end_distance, 0, maximum_tolerance,
	maximum_tolerance);
    const bool start_projected = start_uv.IsValid() &&
	std::isfinite(start_distance) && start_distance <= maximum_tolerance;
    const bool end_projected = end_uv.IsValid() &&
	std::isfinite(end_distance) && end_distance <= maximum_tolerance;
    /* Preserve the established generic pullback path for linear NURBS on
     * analytic surfaces.  The broadened recognition is for native NURBS
     * surfaces whose singular pole can make endpoint UVs branch-ambiguous. */
    const bool nurbs_line_on_nurbs_surface = linear_curve &&
	!native_line_curve && native_nurbs_surface;
    if (linear_curve && !native_line_curve &&
	    !nurbs_line_on_nurbs_surface)
	return NULL;

    const ON_Interval curve_domain = curve->Domain();
    std::vector<ON_LineCurve *> candidates;
    if (linear_curve && start_projected && end_projected) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction)) continue;
	    const double period = surface->Domain(direction).Length();
	    if (period > ON_ZERO_TOLERANCE)
		end_uv[direction] += round((start_uv[direction] -
		    end_uv[direction]) / period) * period;
	}
	candidates.push_back(new ON_LineCurve(start_uv, end_uv));
	for (int constant_direction = 0; constant_direction < 2;
	     constant_direction++) {
	    const double values[3] = {start_uv[constant_direction],
		end_uv[constant_direction],
		0.5 * (start_uv[constant_direction] +
		    end_uv[constant_direction])};
	    for (int value = 0; value < 3; ++value) {
		ON_2dPoint candidate_start(start_uv);
		ON_2dPoint candidate_end(end_uv);
		candidate_start[constant_direction] = values[value];
		candidate_end[constant_direction] = values[value];
		candidates.push_back(new ON_LineCurve(candidate_start,
		    candidate_end));
	    }
	}
    }

    /* A line may start at a surface pole, where a 2-D closest-point query has
     * no unique answer and can choose a constant-parameter branch unrelated
     * to the other endpoint.  Search the surface's exact knot-boundary
     * isocurves in one dimension.  Both endpoints must lie on the same
     * complete isocurve, and the dense lift/locus validation below still
     * proves the resulting sub-interval against the original STEP line. */
    if (native_nurbs_surface) {
	for (int varying_direction = 0; varying_direction < 2;
	     varying_direction++) {
	    const int fixed_direction = 1 - varying_direction;
	    const int span_count = surface->SpanCount(fixed_direction);
	    if (span_count <= 0) continue;
	    std::vector<double> span_values(static_cast<size_t>(span_count) + 1);
	    if (!surface->GetSpanVector(fixed_direction, span_values.data()))
		continue;
	    for (std::vector<double>::const_iterator value = span_values.begin();
		    value != span_values.end(); ++value) {
		std::unique_ptr<ON_Curve> iso(surface->IsoCurve(
		    varying_direction, *value));
		CurveDistanceEvaluator iso_distance(iso.get());
		if (!iso || !iso->IsValid() || !iso_distance.IsValid()) continue;
		double start_parameter = 0.0;
		double end_parameter = 0.0;
		double start_error = DBL_MAX;
		double end_error = DBL_MAX;
		if (!iso_distance.ClosestParameter(source_start,
			&start_parameter, &start_error) ||
		    !iso_distance.ClosestParameter(source_end,
			&end_parameter, &end_error) ||
		    start_error > maximum_tolerance ||
		    end_error > maximum_tolerance)
		    continue;
		ON_2dPoint candidate_start;
		ON_2dPoint candidate_end;
		candidate_start[varying_direction] = start_parameter;
		candidate_end[varying_direction] = end_parameter;
		candidate_start[fixed_direction] = *value;
		candidate_end[fixed_direction] = *value;
		candidates.push_back(new ON_LineCurve(candidate_start,
		    candidate_end));
		if (surface->IsClosed(varying_direction)) {
		    const double period = surface->Domain(
			varying_direction).Length();
		    if (period > ON_ZERO_TOLERANCE) {
			candidate_end[varying_direction] -= period;
			candidates.push_back(new ON_LineCurve(candidate_start,
			    candidate_end));
			candidate_end[varying_direction] += 2.0 * period;
			candidates.push_back(new ON_LineCurve(candidate_start,
			    candidate_end));
		    }
		}
	    }
	}
    }

    ON_LineCurve *pcurve = NULL;
    double best_error = DBL_MAX;
    double closest_candidate_error = DBL_MAX;
    double best_length_error = DBL_MAX;
    bool best_reversed = false;
    const CurveDistanceEvaluator source_distance(curve);
    double source_length = 0.0;
    ON_3dPoint previous_source = curve->PointAt(curve_domain.Min());
    for (int sample = 1; sample <= kDenseLiftValidationSegments; ++sample) {
	const ON_3dPoint current_source = curve->PointAt(
	    curve_domain.ParameterAt(static_cast<double>(sample) /
		kDenseLiftValidationSegments));
	if (previous_source.IsValid() && current_source.IsValid())
	    source_length += previous_source.DistanceTo(current_source);
	previous_source = current_source;
    }
    const bool source_length_valid = source_length > ON_ZERO_TOLERANCE;
    for (std::vector<ON_LineCurve *>::iterator candidate = candidates.begin();
	 candidate != candidates.end(); ++candidate) {
	ON_LineCurve *line = *candidate;
	if (!line->ChangeDimension(2) || !line->IsValid() ||
		surface->IsIsoparametric(*line) == ON_Surface::not_iso) {
	    delete line;
	    continue;
	}
	const ON_Interval line_domain = line->Domain();
	double forward_error = 0.0;
	double reverse_error = 0.0;
	double candidate_length = 0.0;
	ON_3dPoint previous_lift = ON_3dPoint::UnsetPoint;
	for (int sample = 0; sample <= kDenseLiftValidationSegments; ++sample) {
	    if (brlcad::PullbackWorkCancelled()) {
		delete line;
		for (++candidate; candidate != candidates.end(); ++candidate)
		    delete *candidate;
		delete pcurve;
		return NULL;
	    }
	    const double normalized = static_cast<double>(sample) /
		kDenseLiftValidationSegments;
	    const ON_3dPoint target = curve->PointAt(
		curve_domain.ParameterAt(normalized));
	    const ON_3dPoint forward_uv = line->PointAt(
		line_domain.ParameterAt(normalized));
	    const ON_3dPoint reverse_uv = line->PointAt(
		line_domain.ParameterAt(1.0 - normalized));
	    const ON_3dPoint forward_lift = surface->PointAt(forward_uv.x,
		forward_uv.y);
	    const ON_3dPoint reverse_lift = surface->PointAt(reverse_uv.x,
		reverse_uv.y);
	    if (linear_curve) {
		forward_error = std::max(forward_error,
		    target.DistanceTo(forward_lift));
		reverse_error = std::max(reverse_error,
		    target.DistanceTo(reverse_lift));
	    } else if (source_distance.IsValid()) {
		/* STEP edge and surface isocurve parameterizations commonly agree.
		 * Their direct normalized correspondence is an exact membership
		 * witness and avoids a global closest-locus search at every one of
		 * the 1025 dense validation samples.  A differently parameterized
		 * but geometrically identical edge simply falls through to the
		 * parameterization-independent search, preserving the established
		 * acceptance behavior and tolerance. */
		const double direct_error = target.DistanceTo(forward_lift);
		const double locus_error = direct_error <= maximum_tolerance ?
		    direct_error : source_distance.DistanceTo(forward_lift,
			maximum_tolerance);
		forward_error = std::max(forward_error, locus_error);
		reverse_error = forward_error;
	    } else {
		forward_error = DBL_MAX;
		reverse_error = DBL_MAX;
	    }
	    /* Once every possible orientation has exceeded the acceptance
	     * tolerance, this candidate is conclusively rejected.  Continuing the
	     * remaining 1024-segment validation used to run a global closest-curve
	     * search at every sample solely to improve a verbose rejection number.
	     * Accepted candidates still traverse the complete dense validation, so
	     * this changes neither their tolerance nor their geometric proof. */
	    if ((!linear_curve && forward_error > maximum_tolerance) ||
		    (linear_curve && forward_error > maximum_tolerance &&
			reverse_error > maximum_tolerance))
		break;
	    if (previous_lift.IsValid() && forward_lift.IsValid())
		candidate_length += previous_lift.DistanceTo(forward_lift);
	    previous_lift = forward_lift;
	}
	const bool reversed = linear_curve && reverse_error < forward_error;
	const double error = reversed ? reverse_error : forward_error;
	const double length_error = source_length_valid ?
	    fabs(candidate_length - source_length) : candidate_length;
	closest_candidate_error = std::min(closest_candidate_error, error);
	/* Direct point-to-point validation proves coverage for a linear edge, so
	 * select its surface branch by geometric error before comparing length.
	 * Otherwise a long generator on a small periodic surface can prefer an
	 * equally long but offset branch merely because its sampled length rounds
	 * more closely.  Non-linear closed loci retain length-first ranking: their
	 * parameterization-independent distance could otherwise collapse a full
	 * circle to its shared endpoint. */
	const bool better_candidate = linear_curve ?
	    (error < best_error - ON_ZERO_TOLERANCE ||
		(fabs(error - best_error) <= ON_ZERO_TOLERANCE &&
		    length_error < best_length_error)) :
	    (length_error < best_length_error - ON_ZERO_TOLERANCE ||
		(fabs(length_error - best_length_error) <= ON_ZERO_TOLERANCE &&
		    error < best_error));
	if (error <= maximum_tolerance && better_candidate) {
	    delete pcurve;
	    pcurve = line;
	    best_error = error;
	    best_length_error = length_error;
	    best_reversed = reversed;
	} else {
	    delete line;
	}
    }
    if (!pcurve) {
	if (verbose && native_nurbs_surface &&
		ON_IsValid(closest_candidate_error))
	    std::cerr << "STEP edge #" << source_edge_id
		<< ": exact NURBS-boundary pullback rejected; closest dense "
		   "candidate error=" << closest_candidate_error
		<< " limit=" << maximum_tolerance << std::endl;
	return NULL;
    }
    if (best_reversed && !pcurve->Reverse()) {
	delete pcurve;
	return NULL;
    }
    const double effective_tolerance = best_error > declared_tolerance ?
	best_error * kMeasuredToleranceSafetyFactor : declared_tolerance;
    if (effective_tolerance > maximum_tolerance) {
	delete pcurve;
	return NULL;
    }

    ON_2dPointArray *samples = new ON_2dPointArray();
    const ON_Interval validated_domain = pcurve->Domain();
    for (int sample = 0; sample <= 64; ++sample) {
	const double normalized = static_cast<double>(sample) / 64.0;
	const ON_3dPoint uv = pcurve->PointAt(
	    validated_domain.ParameterAt(normalized));
	samples->Append(ON_2dPoint(uv.x, uv.y));
    }

    PBCData *data = new PBCData();
    data->tolerance = effective_tolerance;
    data->flatness = effective_tolerance;
    data->curve = curve;
    data->surf = surface;
    data->surftree = NULL;
    data->segments = new std::list<ON_2dPointArray *>();
    data->segments->push_back(samples);
    data->edge = NULL;
    data->order_reversed = false;
    data->samples_source_validated = true;
    data->tolerance_adjusted = best_error > declared_tolerance;
    data->declared_tolerance = declared_tolerance;
    *exact_curve = pcurve;
    return data;
}
