/*                 OpenNurbsInterfaces.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/OpenNurbsInterfaces.cpp
 *
 * Routines to convert STEP "OpenNurbsInterfaces" to BRL-CAD BREP
 * structures.
 *
 */

#include "common.h"

#include "brep/defines.h"
#include "brep/pullback.h"

#include "sdai.h"
class SDAI_Application_instance;

/* must come after nist step headers */
#include "brep.h"

#include <map>
#include <sstream>
#include <vector>
#include "nmg.h"

#include "STEPEntity.h"
#include "Axis1Placement.h"
#include "Factory.h"
#include "LocalUnits.h"
#include "PullbackCurve.h"
#include "Point.h"
#include "CartesianPoint.h"
#include "VertexPoint.h"
#include "Vector.h"
#include "EdgeCurve.h"
#include "OrientedEdge.h"

// Curve includes
#include "BezierCurve.h"
#include "BSplineCurve.h"
#include "BSplineCurveWithKnots.h"
#include "QuasiUniformCurve.h"
#include "RationalBezierCurve.h"
#include "RationalBSplineCurve.h"
#include "RationalBSplineCurveWithKnots.h"
#include "RationalQuasiUniformCurve.h"
#include "RationalUniformCurve.h"
#include "UniformCurve.h"

// Surface includes
#include "Line.h"
#include "Circle.h"
#include "Ellipse.h"
#include "Hyperbola.h"
#include "Parabola.h"
#include "CylindricalSurface.h"
#include "ConicalSurface.h"
#include "SweptSurface.h"
#include "SurfaceOfLinearExtrusion.h"
#include "SurfaceOfRevolution.h"
#include "Path.h"
#include "Plane.h"
#include "Loop.h"
#include "VertexLoop.h"
#include "Face.h"
#include "OpenShell.h"
#include "OrientedFace.h"
#include "FaceBound.h"
#include "FaceOuterBound.h"
#include "FaceSurface.h"
#include "BezierSurface.h"
#include "BSplineSurface.h"
#include "BSplineSurfaceWithKnots.h"
#include "QuasiUniformSurface.h"
#include "RationalBezierSurface.h"
#include "RationalBSplineSurface.h"
#include "RationalBSplineSurfaceWithKnots.h"
#include "RationalQuasiUniformSurface.h"
#include "RationalUniformSurface.h"
#include "SphericalSurface.h"
#include "ToroidalSurface.h"
#include "UniformSurface.h"

#include "AdvancedBrepShapeRepresentation.h"
#include "ShellBasedSurfaceModel.h"
#include "PullbackCurve.h"

#include "brep.h"

//#define _DEBUG_TESTING_
#ifdef _DEBUG_TESTING_
extern void print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints);
#endif

namespace {

/* Dense validation is the final proof that a repaired or reused pcurve lifts
 * to its exact STEP edge within the file uncertainty.  1024 uniform segments
 * were selected as a bounded empirical budget: they resolve the short seam
 * reversals in the Mark V acceptance model while keeping validation linear and
 * deterministic.  This is a validation budget, not a geometric tolerance or
 * a request to approximate the edge with 1024 segments. */
constexpr int kDenseLiftValidationSegments = 1024;

/* If dense validation finds a missed bow between already projected UV
 * samples, refine only that exceptional polyline.  These are work ceilings,
 * not approximation settings: every inserted point and the completed curve
 * are still checked against the exact 3-D STEP edge. */
constexpr int kMaximumAdjustedPolylineRefinementDepth = 12;
constexpr size_t kMaximumAdjustedPolylineSamples = 16384;

/* STEP pcurves at a periodic join can differ by a few measured microradians
 * even when their lifted 3-D endpoints and topology vertex coincide.  This
 * scale-relative bound only recognizes and snaps parameters already proven
 * coincident in model space; it is never used to accept geometric error. */
constexpr double kPeriodicParameterSnapFraction = 1.0e-5;

/* Keep numerical solver floors comfortably above floating-point zero without
 * replacing the model-derived tolerance used for acceptance. */
constexpr double kNumericalToleranceScale = 1024.0;

/* A relocated closed-surface seam needs a real parameter-space interval that
 * contains no sampled boundary.  Requiring one thousandth of the period keeps
 * the seam away from sampling noise while still admitting narrow exact faces;
 * every relocated pcurve is subsequently checked against its 3-D edge. */
constexpr double kMinimumSafeSeamGapFraction = 1.0e-3;
/* A proposed seam relocation must preserve the spatial coverage of its STEP
 * edge.  Locus-only checks can otherwise collapse a closed circle to its
 * shared vertex and still report zero point-to-curve distance. */
constexpr double kMinimumSeamCoverageFraction = 0.8;

/* Real exchange files occasionally contain an edge curve, its asserted
 * vertices, and its supporting surface at separations larger than the
 * declared uncertainty.  Non-exact mode may reflect that measured reality in
 * the one affected BREP edge, but only below one percent of the edge scale.
 * This is a guardrail against accepting an unrelated surface or topology-
 * scale gap, not an approximation allowance: source curves and vertices stay
 * unchanged and dense lift validation still applies. */
constexpr double kMaximumRelativeEdgeMismatch = 1.0e-2;
constexpr double kMaximumRelativeItemMismatch = 1.0e-3;
constexpr double kMeasuredToleranceSafetyFactor = 1.05;
/* Adaptive UV refinement can discover a larger source mismatch between the
 * original knot samples.  Re-measure and retry a small bounded number of
 * times; double the search tolerance when successive refinement levels expose
 * the mismatch only incrementally.  The accepted BREP tolerance is reset to
 * the largest actually measured error (and later densely validated), so this
 * exponential search does not itself loosen output geometry. */
constexpr int kMaximumMeasuredToleranceRetries = 6;

/* Curve-locus validation brackets the closest point independently in every
 * exact NURBS knot span, then refines the best interval.  Sixty-four brackets
 * are sufficient for a cubic span while keeping exceptional adjusted-edge
 * validation deterministic and bounded. */
constexpr int kCurveClosestBracketsPerSpan = 64;
constexpr int kCurveClosestRefinementIterations = 64;

} // namespace


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
    return std::max(declared_tolerance, relative_limit);
}


class CurveDistanceEvaluator {
public:
    explicit CurveDistanceEvaluator(const ON_Curve *curve)
    {
	if (!curve || !curve->GetNurbForm(m_nurbs)) return;
	const int span_count = m_nurbs.SpanCount();
	if (span_count <= 0) return;
	m_spans.resize(static_cast<size_t>(span_count) + 1);
	if (!m_nurbs.GetSpanVector(m_spans.data())) {
	    m_spans.clear();
	    return;
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
	return !m_spans.empty() && !m_bracket_points.empty();
    }

    bool ClosestParameter(const ON_3dPoint &point, double *parameter,
	    double *distance) const
    {
	if (parameter) *parameter = 0.0;
	if (distance) *distance = DBL_MAX;
	if (!parameter || !IsValid() || !point.IsValid()) return false;
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
		const double distance = RefinedDistance(point, span, bracket);
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
	std::vector<SpanCandidate> candidates;
	candidates.reserve(m_bracket_points.size());
	for (size_t span = 0; span < m_bracket_points.size(); ++span) {
	    if (brlcad::PullbackWorkCancelled()) return DBL_MAX;
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
	std::sort(candidates.begin(), candidates.end(),
	    [](const SpanCandidate &left, const SpanCandidate &right) {
		return left.chord_distance < right.chord_distance;
	    });
	for (std::vector<SpanCandidate>::const_iterator candidate =
		candidates.begin(); candidate != candidates.end(); ++candidate) {
	    if (brlcad::PullbackWorkCancelled()) return DBL_MAX;
	    const size_t span = candidate->span;
	    const int best_bracket = candidate->bracket;
	    minimum_distance = std::min(minimum_distance,
		RefinedDistance(point, span, best_bracket));
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
    double RefinedDistance(const ON_3dPoint &point, size_t span,
	    int bracket, double *parameter = NULL) const
    {
	const double golden_ratio = 0.5 * (sqrt(5.0) - 1.0);
	const ON_Interval domain(m_spans[span], m_spans[span + 1]);
	double left = domain.ParameterAt(static_cast<double>(bracket) /
	    kCurveClosestBracketsPerSpan);
	double right = domain.ParameterAt(static_cast<double>(bracket + 1) /
	    kCurveClosestBracketsPerSpan);
	double x1 = right - golden_ratio * (right - left);
	double x2 = left + golden_ratio * (right - left);
	double f1 = point.DistanceTo(m_nurbs.PointAt(x1));
	double f2 = point.DistanceTo(m_nurbs.PointAt(x2));
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
	}
	if (parameter) *parameter = f1 <= f2 ? x1 : x2;
	return std::min(f1, f2);
    }

    ON_NurbsCurve m_nurbs;
    std::vector<double> m_spans;
    std::vector<std::vector<ON_3dPoint> > m_bracket_points;
    mutable bool m_have_previous = false;
    mutable size_t m_previous_span = 0;
    mutable int m_previous_bracket = 0;
};


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
	double tolerance, ON_3dPoint &uv, double *final_distance)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint point;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, point, du, dv))
	    break;
	const ON_3dVector residual = target - point;
	best_distance = residual.Length();
	if (best_distance <= tolerance)
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
		if (surface->IsClosed(direction))
		    continue;
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
    return best_distance <= tolerance;
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
seam_boundary_score(PBCData *data, int direction, double value, double tolerance, double *score)
{
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
	    if (!lifted.IsValid() ||
		    edge_distance.DistanceTo(lifted, tolerance) > tolerance)
		return false;
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
	    source_scale * kMinimumSeamCoverageFraction)
	return false;
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
    const ON_Surface *&surface, double tolerance, std::string *failure = NULL)
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

	/* Only a nonzero net winding requires a pole cut.  Ordinary edges can
	 * cross the native seam and create one-period local jumps while the loop
	 * as a whole remains contractible; those must stay ordinary edges. */
	double unwrapped_travel = 0.0;
	for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
		data != pullbacks.end(); ++data) {
	    if (!*data || !(*data)->segments || (*data)->segments->empty() ||
		    !(*data)->segments->front() || !(*data)->segments->back() ||
		    (*data)->segments->front()->Count() == 0 ||
		    (*data)->segments->back()->Count() == 0)
		continue;
	    const ON_2dPointArray *last_segment = (*data)->segments->back();
	    double edge_travel =
		(*last_segment)[last_segment->Count() - 1][direction] -
		(*(*data)->segments->front())[0][direction];
	    edge_travel -= round(edge_travel / period) * period;
	    unwrapped_travel += edge_travel;
	}
	const double winding = unwrapped_travel / period;
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


/* Move a closed NURBS seam out of an ordinary face boundary.  STEP exporters
 * are allowed to place a face across the underlying surface seam without
 * supplying a duplicated topological seam edge.  In that case independently
 * wrapped pullbacks produce a full-period jump even though every 3-D edge is
 * exact.  Choose the largest sampled boundary-free interval, change the
 * surface seam there, and remap each sample to the new domain.  This is an
 * exact reparameterization: no sample is accepted unless its lift is unchanged
 * within model uncertainty. */
static bool
relocate_surface_seam_away_from_boundary(
    std::list<PBCData *> &pullbacks, ON_Brep *brep, ON_BrepFace *face,
    const ON_Surface *&surface, double tolerance, std::string *failure = NULL)
{
    if (failure)
	failure->clear();

    const ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
    if (!brep || !face || !surface || !nurbs ||
	    !(tolerance > 0.0) || pullbacks.empty()) {
	if (failure)
	    *failure = !brep || !face || !surface ?
		"the BREP face or surface is unavailable" :
		(!nurbs ?
		"ordinary seam relocation requires a native NURBS surface" :
		"the pullback set or tolerance is empty");
	return false;
    }

    const ON_NurbsSurface &source_nurbs = *nurbs;

    std::set<const ON_BrepEdge *> edges;
    for (std::list<PBCData *>::const_iterator data = pullbacks.begin();
	 data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->edge || !edges.insert((*data)->edge).second) {
	    if (failure)
		*failure = "the loop contains a repeated topological edge";
	    return false; /* A repeated edge is a topological seam, handled above. */
	}
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval old_domain = surface->Domain(direction);
	const double period = old_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

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
	double seam_phase = std::fmod(gap_start + 0.5 * largest_gap, period);
	if (seam_phase < 0.0) seam_phase += period;
	const double seam = old_domain.Min() + seam_phase;
	const double endpoint_guard = std::max(ON_ZERO_TOLERANCE,
	    1.0e-10 * period);
	if (seam <= old_domain.Min() + endpoint_guard ||
		seam >= old_domain.Max() - endpoint_guard) {
	    if (failure)
		*failure = "the only empty interval selected an existing domain endpoint";
	    continue;
	}

	double nurbs_seam_u = direction == 0 ? seam :
	    surface->Domain(0).Mid();
	double nurbs_seam_v = direction == 1 ? seam :
	    surface->Domain(1).Mid();
	if (!nurbs && !surface->GetNurbFormParameterFromSurfaceParameter(
		nurbs_seam_u, nurbs_seam_v, &nurbs_seam_u, &nurbs_seam_v)) {
	    if (failure)
		*failure = "the candidate seam could not be mapped to the rational surface";
	    continue;
	}
	const double nurbs_seam = direction == 0 ? nurbs_seam_u : nurbs_seam_v;
	ON_NurbsSurface candidate(source_nurbs);
	if (!candidate.Domain(direction).Includes(nurbs_seam) ||
		!candidate.ChangeSurfaceSeam(direction, nurbs_seam) ||
		!candidate.IsValid()) {
	    if (failure)
		*failure = "openNURBS rejected the candidate surface seam change";
	    continue;
	}
	const ON_Interval new_domain = candidate.Domain(direction);
	const double new_period = new_domain.Length();
	if (!(new_period > ON_ZERO_TOLERANCE)) {
	    if (failure)
		*failure = "the relocated rational surface has no periodic domain";
	    continue;
	}
	std::vector<ON_2dPointArray> remapped;
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
		    if (!nurbs && !surface->GetNurbFormParameterFromSurfaceParameter(
			    surface_parameter.x, surface_parameter.y,
			    &mapped.x, &mapped.y)) {
			valid = false;
			break;
		    }
		    double parameter = mapped[direction];
		    while (parameter < new_domain.Min() - endpoint_guard)
			parameter += new_period;
		    while (parameter > new_domain.Max() + endpoint_guard)
			parameter -= new_period;
		    mapped[direction] = parameter;
		    points[point] = mapped;
		    const ON_3dPoint old_lift = surface->PointAt(
			surface_parameter.x, surface_parameter.y);
		    const ON_3dPoint new_lift = candidate.PointAt(
			points[point].x, points[point].y);
		    const double lift_error = old_lift.IsValid() && new_lift.IsValid() ?
			old_lift.DistanceTo(new_lift) : DBL_MAX;
		    valid = lift_error <= tolerance;
		    if (!valid) {
			std::ostringstream detail;
			detail << " (lift error " << lift_error << " at "
			    << original.x << ':' << original.y << " -> "
			    << points[point].x << ':' << points[point].y << ')';
			invalid_detail = detail.str();
		    }
		    if (valid && point > 0 && fabs(points[point][direction] -
			    points[point - 1][direction]) > 0.5 * new_period) {
			valid = false;
			std::ostringstream detail;
			detail << " (mapped segment retained a periodic jump from "
			    << points[point - 1][direction] << " to "
			    << points[point][direction] << ')';
			invalid_detail = detail.str();
		    }
		}
		remapped.push_back(points);
	    }
	}
	if (!valid) {
	    if (failure)
		*failure = "the candidate seam did not preserve every sampled 3-D lift" +
		    invalid_detail;
	    continue;
	}

	/* Give this face a private surface.  Mutating the original in place would
	 * invalidate trims on any previously completed face sharing that surface. */
	ON_NurbsSurface *replacement = new ON_NurbsSurface(candidate);
	const int replacement_index = brep->AddSurface(replacement);
	if (replacement_index < 0) {
	    delete replacement;
	    if (failure)
		*failure = "the BREP rejected the relocated rational surface";
	    continue;
	}
	face->m_si = replacement_index;
	face->SetProxySurface(replacement);
	surface = replacement;
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
		if (*segment && remapped_index < remapped.size())
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


static bool
refined_fragment_polyline(PBCData *data, const ON_2dPointArray &samples,
	double tolerance, ON_Curve **result, std::string *failure)
{
    if (result) *result = NULL;
    if (failure) failure->clear();
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
    if (!source_distance.ClosestParameter(start_lift, &start_parameter,
		&start_distance) ||
	    !source_distance.ClosestParameter(end_lift, &end_parameter,
		&end_distance) || start_distance > tolerance ||
	    end_distance > tolerance) {
	if (failure) *failure = "fragment endpoints did not locate an exact source interval";
	return false;
    }

    if (!data->context)
	data->context = std::make_shared<brlcad::PullbackContext>();
    ON_3dPointArray points;
    points.Reserve(kDenseLiftValidationSegments + 1);
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
	ON_3dPoint uv = previous;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	bool projected = sample == 0 || sample == kDenseLiftValidationSegments;
	if (sample == 0)
	    uv.Set(samples[0].x, samples[0].y, 0.0);
	else if (sample == kDenseLiftValidationSegments)
	    uv.Set(samples[samples.Count() - 1].x,
		samples[samples.Count() - 1].y, 0.0);
	else {
	    projected = refine_surface_point_seeded(data->surf, target,
		tolerance, uv, &distance);
	    if (!projected) {
		ON_2dPoint projected_uv(uv.x, uv.y);
		projected = data->context->SurfaceClosestPoint(data->surf, target,
		    projected_uv, lift, distance, 0, tolerance, tolerance);
		if (projected)
		    uv.Set(projected_uv.x, projected_uv.y, 0.0);
	    }
	}
	if (!projected) {
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
	if (!target.IsValid() || !lift.IsValid() ||
		lift.DistanceTo(target) > tolerance) {
	    if (failure) {
		std::ostringstream details;
		details << "dense subedge projection missed the source by "
		    << lift.DistanceTo(target) << " at sample " << sample;
		*failure = details.str();
	    }
	    return false;
	}
	if (points.Count() == 0 || uv.DistanceTo(points[points.Count() - 1]) >
		ON_ZERO_TOLERANCE)
	    points.Append(uv);
	previous = uv;
    }
    ON_PolylineCurve *polyline = points.Count() >= 2 ?
	new ON_PolylineCurve(points) : NULL;
    if (!polyline || !polyline->ChangeDimension(2) || !polyline->IsValid()) {
	delete polyline;
	if (failure) *failure = "dense fragment polyline construction failed";
	return false;
    }
    *result = polyline;
    return true;
}


static bool
split_pullback_segment_edge(ON_Brep *brep, PBCData *data,
	const ON_Curve *pcurve, const ON_2dPointArray &samples,
	double tolerance, ON_BrepEdge **segment_edge, bool *trim_reversed,
	std::string *failure)
{
    if (failure)
	failure->clear();
    if (segment_edge)
	*segment_edge = NULL;
    if (trim_reversed)
	*trim_reversed = false;
    if (!brep || !data || !data->edge || !data->curve || !data->surf ||
	    !pcurve || samples.Count() < 2 || !(tolerance > 0.0) ||
	    !segment_edge || !trim_reversed)
	return false;

    ON_NurbsCurve source_nurbs;
    const CurveDistanceEvaluator source_distance(data->curve);
    if (!data->curve->GetNurbForm(source_nurbs) ||
	    !source_distance.IsValid()) {
	if (failure) *failure = "source edge NURBS conversion failed";
	return false;
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
	    start_distance > tolerance || end_distance > tolerance) {
	if (failure) {
	    std::ostringstream details;
	    details << "fragment endpoints did not project onto the source edge: "
		<< start_distance << '/' << end_distance;
	    *failure = details.str();
	}
	return false;
    }
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	data->curve->Domain().Length() * 1.0e-12);
    if (fabs(start_parameter - end_parameter) <= parameter_guard) {
	if (failure) *failure = "fragment endpoints selected the same source parameter";
	return false;
    }

    const ON_Interval segment_domain(std::min(start_parameter, end_parameter),
	std::max(start_parameter, end_parameter));
    ON_Curve *subcurve = data->curve->DuplicateCurve();
    if (!subcurve || !subcurve->Trim(segment_domain) ||
	    !subcurve->IsValid()) {
	delete subcurve;
	if (failure) *failure = "source curve rejected the fragment parameter interval";
	return false;
    }

    const ON_3dPoint edge_start = subcurve->PointAtStart();
    const ON_3dPoint edge_end = subcurve->PointAtEnd();
    *trim_reversed = loop_start.DistanceTo(edge_end) <
	loop_start.DistanceTo(edge_start);
    const ON_3dPoint expected_start = *trim_reversed ? edge_end : edge_start;
    const ON_3dPoint expected_end = *trim_reversed ? edge_start : edge_end;
    if (loop_start.DistanceTo(expected_start) > tolerance ||
	    loop_end.DistanceTo(expected_end) > tolerance) {
	delete subcurve;
	if (failure) *failure = "trim and subedge endpoint orientations did not agree";
	return false;
    }

    /* Prove that the complete emitted pcurve fragment belongs to this exact
	 * 3-D subedge.  Splitting topology at a surface singularity is not a
	 * geometric approximation; both curves retain their original loci. */
    const CurveDistanceEvaluator subedge_distance(subcurve);
    const ON_Interval pcurve_domain = pcurve->Domain();
    bool exact = subedge_distance.IsValid() && pcurve_domain.IsIncreasing();
    int rejected_sample = -1;
    double rejected_distance = 0.0;
    for (int sample = 0; exact && sample <= kDenseLiftValidationSegments;
	    ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kDenseLiftValidationSegments;
	const ON_3dPoint uv = pcurve->PointAt(
	    pcurve_domain.ParameterAt(fraction));
	const ON_3dPoint lift = data->surf->PointAt(uv.x, uv.y);
	rejected_distance = lift.IsValid() ?
	    subedge_distance.DistanceTo(lift, tolerance) : DBL_MAX;
	exact = lift.IsValid() && rejected_distance <= tolerance;
	if (!exact) rejected_sample = sample;
    }
    if (!exact) {
	delete subcurve;
	if (failure) {
	    std::ostringstream details;
	    details << "fragment pcurve left the exact subedge at sample "
		<< rejected_sample << " by " << rejected_distance;
	    *failure = details.str();
	}
	return false;
    }

    const int source_edge_id = data->edge->m_edge_user.i;
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
	const bool same_direction = candidate_start.DistanceTo(edge_start) <= tolerance &&
	    candidate_end.DistanceTo(edge_end) <= tolerance;
	const bool opposite_direction = candidate_start.DistanceTo(edge_end) <= tolerance &&
	    candidate_end.DistanceTo(edge_start) <= tolerance;
	if (!same_direction && !opposite_direction)
	    continue;
	const CurveDistanceEvaluator candidate_distance(candidate.EdgeCurveOf());
	if (!candidate_distance.IsValid() ||
		candidate_distance.DistanceTo(subcurve_midpoint, tolerance) > tolerance)
	    continue;
	delete subcurve;
	*segment_edge = &candidate;
	*trim_reversed = loop_start.DistanceTo(candidate_end) <
	    loop_start.DistanceTo(candidate_start);
	return true;
    }

    const auto matching_vertex = [brep, source_edge_id, tolerance](
	const ON_3dPoint &point) -> int {
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_edge_user.i != source_edge_id)
		continue;
	    for (int end = 0; end < 2; ++end) {
		const int vi = edge.m_vi[end];
		if (vi >= 0 && vi < brep->m_V.Count() &&
			brep->m_V[vi].point.DistanceTo(point) <= tolerance)
		    return vi;
	    }
	}
	return -1;
    };
    int start_vertex = matching_vertex(edge_start);
    int end_vertex = matching_vertex(edge_end);
    if (start_vertex < 0)
	start_vertex = brep->NewVertex(edge_start, tolerance).m_vertex_index;
    if (end_vertex < 0)
	end_vertex = brep->NewVertex(edge_end, tolerance).m_vertex_index;
    if (start_vertex < 0 || end_vertex < 0) {
	delete subcurve;
	return false;
    }
    const int curve_index = brep->AddEdgeCurve(subcurve);
    if (curve_index < 0)
	return false;
    ON_BrepEdge &created = brep->NewEdge(brep->m_V[start_vertex],
	brep->m_V[end_vertex], curve_index, NULL, tolerance);
    created.m_edge_user.i = source_edge_id;
    created.m_tolerance = tolerance;
    *segment_edge = &created;
    return true;
}


static bool
normalize_periodic_pullback_segments(std::list<PBCData *> &pullbacks,
	const ON_Surface *surface, double tolerance, size_t *normalized_segments)
{
    if (normalized_segments)
	*normalized_segments = 0;
    if (!surface || !(tolerance > 0.0))
	return false;

    /* Translating fragments independently in both coordinates can place
     * adjacent trims on incompatible copies of a doubly-periodic surface,
     * even though every individual 3-D lift is unchanged.  The local branch
     * normalization is admissible only when exactly one surface direction is
     * closed.  Doubly-periodic loops are normalized coherently by the seam-
     * pair and loop-chain repair passes below. */
    if (surface->IsClosed(0) == surface->IsClosed(1))
	return false;

    bool changed = false;
    for (std::list<PBCData *>::iterator data = pullbacks.begin();
	    data != pullbacks.end(); ++data) {
	if (!*data || !(*data)->segments)
	    continue;
	for (std::list<ON_2dPointArray *>::iterator segment =
		(*data)->segments->begin(); segment != (*data)->segments->end();
		++segment) {
	    if (!*segment || (*segment)->Count() < 2)
		continue;
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
		const ON_3dPoint source_lift = surface->PointAt(
		    (**segment)[point].x, (**segment)[point].y);
		const ON_3dPoint candidate_lift = surface->PointAt(candidate[point].x,
		    candidate[point].y);
		const double coordinate_scale = source_lift.IsValid() ? std::max(1.0,
		    std::max(fabs(source_lift.x), std::max(fabs(source_lift.y),
			fabs(source_lift.z)))) : 1.0;
		const double numerical_floor = 512.0 * DBL_EPSILON * coordinate_scale;
		/* Integer-period translation must preserve the already validated 3-D
		 * lift.  Compare those two lifts directly: rechecking against the STEP
		 * edge can incorrectly reject a valid reparameterization after a
		 * separately measured endpoint snap widened that edge's tolerance. */
		if (!source_lift.IsValid() || !candidate_lift.IsValid() ||
			candidate_lift.DistanceTo(source_lift) > numerical_floor) {
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
    std::list<ON_2dPointArray *> replacement;
    size_t splits = 0;
    bool valid = true;

    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); valid && segment != data->segments->end();
	    ++segment) {
	if (!*segment || (*segment)->Count() < 2) {
	    valid = false;
	    break;
	}
	ON_2dPointArray *current = new ON_2dPointArray();
	ON_2dPoint previous = UnwrapUVPoint(data->surf, (**segment)[0], tolerance);
	current->Append(previous);
	for (int point = 1; valid && point < (*segment)->Count(); ++point) {
	    if (brlcad::PullbackWorkCancelled()) {
		valid = false;
		break;
	    }
	    const ON_2dPoint native = UnwrapUVPoint(data->surf,
		(**segment)[point], tolerance);
	    int u_direction = 0;
	    int v_direction = 0;
	    if (!ConsecutivePointsCrossClosedSeam(data->surf, native, previous,
		    u_direction, v_direction, tolerance)) {
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
	    if (!Find3DCurveSeamCrossing(*data, previous_parameter,
		    native_parameter, 0.0, seam_parameter, from, to, tolerance,
		    data->tolerance, data->tolerance)) {
		valid = false;
		break;
	    }
	    ForceToClosestSeam(data->surf, from, tolerance);
	    ForceToClosestSeam(data->surf, to, tolerance);
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

    if (!valid || splits == 0) {
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
merge_ordinary_periodic_pullback_fragments(PBCData *data, double tolerance)
{
    if (!data || !data->segments || data->segments->size() < 2 ||
	    !data->surf || !(tolerance > 0.0))
	return false;

    std::unique_ptr<ON_2dPointArray> merged(new ON_2dPointArray());
    const double lift_tolerance = std::max(ON_ZERO_TOLERANCE *
	kNumericalToleranceScale, tolerance * 1.0e-8);
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (!*segment || (*segment)->Count() < 2)
	    return false;
	ON_2dPointArray shifted(**segment);
	if (merged->Count() > 0) {
	    const ON_2dPoint previous = (*merged)[merged->Count() - 1];
	    const ON_2dPoint first = shifted[0];
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
		if (!original_lift.IsValid() || !shifted_lift.IsValid() ||
			original_lift.DistanceTo(shifted_lift) > lift_tolerance)
		    return false;
	    }
	    const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE,
		std::max(data->surf->Domain(0).Length(),
		    data->surf->Domain(1).Length()) * 1.0e-10);
	    if (previous.DistanceTo(shifted[0]) > parameter_tolerance)
		return false;
	}
	const int first_point = merged->Count() > 0 ? 1 : 0;
	for (int point = first_point; point < shifted.Count(); ++point) {
	    if (merged->Count() == 0 || shifted[point].DistanceTo(
		    (*merged)[merged->Count() - 1]) > ON_ZERO_TOLERANCE)
		merged->Append(shifted[point]);
	}
    }
    if (merged->Count() < 2)
	return false;

    while (!data->segments->empty()) {
	delete data->segments->front();
	data->segments->pop_front();
    }
    data->segments->push_back(merged.release());
    return true;
}


static bool
snap_seam_pullback_pair(std::list<PBCData *> &pullbacks, PBCData *first, PBCData *second,
	ON_BrepLoop::TYPE expected_loop_type, double tolerance)
{
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
	    if (!seam_boundary_score(first, direction, first_value, tolerance, &first_score) ||
		!seam_boundary_score(second, direction, second_value, tolerance, &second_score))
		continue;
	    double area = 0.0;
	    double gap = 0.0;
	    pullback_loop_metrics(pullbacks, first, first_value, second, second_value,
		direction, domain.Length(), &area, &gap);
	    candidates.push_back({direction, first_value, second_value,
		first_score + second_score, area, gap});
	}
    }
    if (candidates.empty())
	return false;

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
    if (!best)
	return false;

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
    return changed;
}


static size_t
snap_pullback_loop_endpoints(std::list<PBCData *> &pullbacks, const ON_Brep *brep,
    double tolerance)
{
    struct SegmentRef {
	PBCData *data;
	ON_2dPointArray *samples;
    };
    std::vector<SegmentRef> segments;
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
	    if (*data && (*data)->edge == candidate->edge)
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
			lifted_common.DistanceTo(vertex) > join_tolerance) {
		    const ON_2dPoint alternate = end_is_pinned_seam ? *start : *end;
		    const ON_3dPoint lifted_alternate = current.data->surf->PointAt(
			alternate.x, alternate.y);
		    bool preserves_boundary = lifted_alternate.IsValid() &&
			lifted_alternate.DistanceTo(vertex) <= join_tolerance;
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
	    const bool end_valid = lifted_end.DistanceTo(vertex) <= join_tolerance;
	    const bool start_valid = lifted_start.DistanceTo(vertex) <= join_tolerance;
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
			    vertex, common, projected, distance, 0, join_tolerance,
			    join_tolerance) || distance > join_tolerance)
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
	} else if (lifted_end.DistanceTo(lifted_start) > join_tolerance) {
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


static ON_Curve *
closed_edge_iso_line(PBCData *data, const ON_2dPointArray &samples, double tolerance)
{
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
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval domain = data->surf->Domain(direction);
	if (!domain.IsIncreasing())
	    continue;
	for (int side = 0; side < 2; ++side) {
	    const double value = domain[side];
	    ON_2dPoint start = samples[0];
	    ON_2dPoint end = samples[samples.Count() - 1];
	    start[direction] = value;
	    end[direction] = value;
	    if (fabs(end[1 - direction] - start[1 - direction]) <= ON_ZERO_TOLERANCE)
		continue;

	    bool forward_valid = true;
	    bool reverse_valid = true;
	    const ON_Interval curve_domain = data->curve->Domain();
	    for (int i = 0; i <= 64; ++i) {
		const double parameter = static_cast<double>(i) / 64.0;
		const ON_2dPoint uv = (1.0 - parameter) * start + parameter * end;
		const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
		if (!lifted.IsValid()) {
		    forward_valid = false;
		    reverse_valid = false;
		    break;
		}
		if (lifted.DistanceTo(data->curve->PointAt(curve_domain.ParameterAt(parameter))) > tolerance)
		    forward_valid = false;
		if (lifted.DistanceTo(data->curve->PointAt(curve_domain.ParameterAt(1.0 - parameter))) > tolerance)
		    reverse_valid = false;
	    }
	    if (!forward_valid && !reverse_valid)
		continue;

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
    data->tolerance_adjusted = measured_error > declared_tolerance;
    data->declared_tolerance = declared_tolerance;
    *exact_curve = pcurve;
    return data;
}


/* Lines along an analytic surface's parameter direction (for example a
 * cylinder generator) have exact linear pcurves.  The generic adaptive
 * closest-point path can lose such an edge when it lies exactly on a periodic
 * seam.  Recover it from its two endpoints, but accept it only when it is
 * isoparametric and every dense validation sample lifts to the original line
 * within the model uncertainty. */
static PBCData *
exact_isoparametric_line_pullback(const ON_Surface *surface,
    const ON_Curve *curve, double tolerance, ON_Curve **exact_curve)
{
    if (exact_curve) *exact_curve = NULL;
    if (!surface || !curve || !exact_curve || tolerance <= 0.0 ||
	!ON_LineCurve::Cast(curve))
	return NULL;

    brlcad::PullbackContext context;
    ON_2dPoint start_uv, end_uv;
    ON_3dPoint start_lift, end_lift;
    double start_distance = DBL_MAX;
    double end_distance = DBL_MAX;
    if (!context.SurfaceClosestPoint(surface, curve->PointAtStart(), start_uv,
	    start_lift, start_distance, 0, tolerance, tolerance) ||
	!context.SurfaceClosestPoint(surface, curve->PointAtEnd(), end_uv,
	    end_lift, end_distance, 0, tolerance, tolerance) ||
	start_distance > tolerance || end_distance > tolerance)
	return NULL;

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction)) continue;
	const double period = surface->Domain(direction).Length();
	if (period > ON_ZERO_TOLERANCE)
	    end_uv[direction] += round((start_uv[direction] - end_uv[direction]) /
		period) * period;
    }

    const ON_Interval curve_domain = curve->Domain();
    std::vector<ON_LineCurve *> candidates;
    candidates.push_back(new ON_LineCurve(start_uv, end_uv));
    for (int constant_direction = 0; constant_direction < 2;
	 constant_direction++) {
	const double values[3] = {start_uv[constant_direction],
	    end_uv[constant_direction],
	    0.5 * (start_uv[constant_direction] + end_uv[constant_direction])};
	for (int value = 0; value < 3; ++value) {
	    ON_2dPoint candidate_start(start_uv);
	    ON_2dPoint candidate_end(end_uv);
	    candidate_start[constant_direction] = values[value];
	    candidate_end[constant_direction] = values[value];
	    candidates.push_back(new ON_LineCurve(candidate_start, candidate_end));
	}
    }

    ON_LineCurve *pcurve = NULL;
    double best_error = DBL_MAX;
    bool best_reversed = false;
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
	    forward_error = std::max(forward_error, target.DistanceTo(
		surface->PointAt(forward_uv.x, forward_uv.y)));
	    reverse_error = std::max(reverse_error, target.DistanceTo(
		surface->PointAt(reverse_uv.x, reverse_uv.y)));
	}
	const bool reversed = reverse_error < forward_error;
	const double error = reversed ? reverse_error : forward_error;
	if (error <= tolerance && error < best_error) {
	    delete pcurve;
	    pcurve = line;
	    best_error = error;
	    best_reversed = reversed;
	} else {
	    delete line;
	}
    }
    if (!pcurve)
	return NULL;
    if (best_reversed && !pcurve->Reverse()) {
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
    data->tolerance = tolerance;
    data->flatness = tolerance;
    data->curve = curve;
    data->surf = surface;
    data->surftree = NULL;
    data->segments = new std::list<ON_2dPointArray *>();
    data->segments->push_back(samples);
    data->edge = NULL;
    data->order_reversed = false;
    *exact_curve = pcurve;
    return data;
}

ON_Brep *
AdvancedBrepShapeRepresentation::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
AdvancedBrepShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    LIST_OF_REPRESENTATION_ITEMS::iterator i;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    for (i = items.begin(); i != items.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }

    return true;
}

ON_Brep *
ShellBasedSurfaceModel::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
ShellBasedSurfaceModel::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    LIST_OF_SHELL_BOUNDARIES::iterator i;
    for (i = sbsm_boundary.begin(); i != sbsm_boundary.end(); ++i) {
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    return true;
}

//
// Curve handlers
//
bool
BezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }
    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	// knot index (>= 0 and < Order + CV_count - 2)
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    V_MIN(multiplicity, degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
UniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


//
// Surface handlers
//
bool
BezierSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add bezier surface
    //ON_BezierSurface* surf = ON_BezierSurface::New(3, false, u_degree+1, v_degree+1);
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
BSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
	    v++;
	}
	u++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}

ON_Brep *
BSplineSurfaceWithKnots::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    ON_Brep *b2 = ON_Brep::New();
    b2->NewFace(*brep->m_S[0]);
    b2->Flip();

    delete brep;

    return b2;
}


bool
BSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    if (u_closed == 1) {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < u_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == this->u_multiplicities.end() && (multiplicity < u_degree) && (knot_value > 1.0)) {
		break;
	    }

	    V_MIN(multiplicity, u_degree);

	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);

	    V_MIN(multiplicity, u_degree);

	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    if (v_closed == 1) {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < v_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == v_multiplicities.end() && (multiplicity < v_degree) && (knot_value > 1.0)) {
		break;
	    }

	    V_MIN(multiplicity, v_degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);

	    V_MIN(multiplicity, v_degree);

	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
	    v++;
	}
	u++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}


bool
QuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add rational bezier surface
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
RationalBSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    LIST_OF_REALS::iterator r;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
	}
	u++;
	w++;
    }
    ON_id = brep->AddSurface(surf);

    return true;
}


bool
RationalBSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (self_intersect) {
    }
    if (u_closed) {
    }
    if (v_closed) {
    }

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, true, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);

	V_MIN(multiplicity, u_degree);

	for (int j = 0; j < multiplicity; j++, knot_index++) {
	    surf->SetKnot(0, knot_index, knot_value);
	}
	r++;
	m++;
    }
    m = v_multiplicities.begin();
    r = v_knots.begin();
    knot_index = 0;
    while (m != v_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);

	V_MIN(multiplicity, v_degree);

	for (int j = 0; j < multiplicity; j++) {
	    surf->SetKnot(1, knot_index++, knot_value);
	}
	r++;
	m++;
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
	}
	u++;
	w++;
    }

    ON_id = brep->AddSurface(surf);

    return true;
}


bool
RationalQuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
UniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


void
FaceSurface::AddFace(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    ON_BrepFace &face = brep->NewFace(face_geometry->GetONId());
    if (same_sense == BTrue) {
	face.m_bRev = false;
    } else {
	face.m_bRev = true;
	face.Reverse(0); //need to remove here but check for reversed face in raytracer
    }

    ON_id = face.m_face_index;
}


bool
FaceSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (step)
	step->SetProgressDetail("computing exact STEP face edge bounds", id);

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    ON_BoundingBox *bb = GetEdgeBounds(brep);
    if (!bb) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error calculating openNURBS brep bounds." << std::endl;
	return false;
    }
	if (brlcad::PullbackWorkCancelled()) {
	    delete bb;
	    return false;
	}

    face_geometry->SetCurveBounds(bb);
    delete bb;

    if (step)
	step->SetProgressDetail("building exact STEP face surface", id);
    if (!face_geometry->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    AddFace(brep);
	if (brlcad::PullbackWorkCancelled()) return false;

    //TODO: remove debugging code
    if ((false) && (ON_id == 72)) {
	std::cerr << "Debug:LoadONBrep for FaceSurface:" << ON_id << std::endl;
    }
    if (step)
	step->SetProgressDetail("constructing exact STEP face loops", id);
    if (!Face::LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    if (reverse) {
	ON_BrepFace *face = brep->Face(ON_id);
	face->Reverse(1);
	face->m_bRev = face->m_bRev ? false : true;
    }
    return true;
}


bool
OrientedFace::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    if (!face_element->LoadONBrep(brep)) {
#ifndef AP242
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
#endif
	return false;
    }

    return true;
}


void
Point::AddVertex(ON_Brep *UNUSED(brep))
{
    std::cerr << "Warning: " << entityname << "::AddVertex() should be overridden by parent class." << std::endl;
}


void
CartesianPoint::AddVertex(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (vertex_index < 0) {
	ON_3dPoint p(coordinates[0] * LocalUnits::length, coordinates[1] * LocalUnits::length, coordinates[2] * LocalUnits::length);
	ON_BrepVertex &v = brep->NewVertex(p);
	vertex_index = v.m_vertex_index;
	v.m_tolerance = LocalUnits::tolerance;
	ON_id = v.m_vertex_index;
    }
}


void
BSplineCurve::AddPolyLine(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (ON_id < 0) {
	int num_control_points = control_points_list.size();

	if ((degree == 1) && (num_control_points >= 2)) {
	    LIST_OF_POINTS::iterator i = control_points_list.begin();
	    CartesianPoint *cp = (*i);
	    while ((++i) != control_points_list.end()) {
		ON_3dPoint start_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		cp = (*i);
		ON_3dPoint end_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		ON_LineCurve *line = new ON_LineCurve(ON_3dPoint(start_point), ON_3dPoint(end_point));
		brep->m_C3.Append(line);
	    }
	} else if (num_control_points > 2) {
	    ON_NurbsCurve *c = ON_NurbsCurve::New(3, false, degree + 1, num_control_points);
	    /* FIXME: do something with c */
	    delete c;
	} else {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading polyLine." << std::endl;
	}
    }
}


void
VertexPoint::AddVertex(ON_Brep *brep)
{
    vertex_geometry->AddVertex(brep);
}


ON_BoundingBox *
Face::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return NULL;
	ON_BoundingBox *bb = (*i)->GetEdgeBounds(brep);
	if (bb != NULL) {
	    if (u == NULL) {
		u = new ON_BoundingBox();
	    }
	    u->Union(*bb);
	}
	delete bb;
    }
    return u;
}


bool
Face::LoadONBrep(ON_Brep *brep)
{
    //TODO: Check for Outer bound if none check for
    // direction perhaps offer input option possibly
    // check for outer spanning to bounds
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (step)
	    step->SetProgressDetail("constructing exact STEP face loop",
		(*i) ? (*i)->STEPid() : id, 0, 0, std::string(),
		"face=#" + std::to_string(id));
	(*i)->SetFaceIndex(ON_id);
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    return true;
}


bool
FaceOuterBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    SetOuter();

    if (!FaceBound::LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


ON_BoundingBox *
FaceBound::GetEdgeBounds(ON_Brep *brep)
{
    return bound->GetEdgeBounds(brep);
}


bool
FaceBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id < 0) {
	enum ON_BrepLoop::TYPE btype;
	if (IsInner()) {
	    btype = ON_BrepLoop::inner;
	} else {
	    btype = ON_BrepLoop::outer;
	}
	ON_BrepLoop &loop = brep->NewLoop(btype, brep->m_F[ON_face_index]);
	ON_id = loop.m_loop_index;
    }
    bound->SetLoopIndex(ON_id);
    if (!bound->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!Oriented()) {
	ON_BrepLoop &loop = brep->m_L[ON_id];
	brep->FlipLoop(loop);
    }
    /* lets use the cues from STEP (Oriented) for now but leave this here
     * in case we have to go back to brute force

     if (IsInner()) {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) > 0) {
     brep->FlipLoop(loop);
     }
     } else {
     ON_BrepLoop& loop = brep->m_L[ON_id];
     if (brep->LoopDirection((const ON_BrepLoop&) loop) < 0) {
     brep->FlipLoop(loop);
     }
     }
    */

    return true;
}

bool
EdgeCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    if ((false) && (brep->m_E.Count() == 484)) {
	std::cerr << "Debug:LoadONBrep for EdgeCurve:" << brep->m_E.Count() << std::endl;
    }

    Vertex *start = NULL;
    Vertex *end = NULL;
    if (same_sense == 1) {
	start = edge_start;
	end = edge_end;
    } else {
	start = edge_end;
	end = edge_start;
    }
    edge_geometry->Start(start);
    edge_geometry->End(end);

    if (!edge_geometry->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_BrepEdge &edge = brep->NewEdge(brep->m_V[start->GetONId()], brep->m_V[end->GetONId()], edge_geometry->GetONId());
    edge.m_tolerance = LocalUnits::tolerance;
    edge.m_edge_user.i = id;
    ON_id = edge.m_edge_index;
    if (same_sense != 1) {
	edge.Reverse();
    }

    return true;
}

bool
OrientedEdge::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    //already loaded
    }

    //TODO: remove debugging code
    //if ((false) && (brep->m_E.Count() == 5)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << brep->m_E.Count() << std::endl;
    //}

    if (!edge_start->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_end->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_element->LoadONBrep(brep)) {
	if (step && step->Verbose())
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_id = edge_element->GetONId();

    //TODO: remove debugging code
    //if ((true) && (ON_id == 12)) {
    //std::cerr << "Debug:LoadONBrep for OrientedEdge:" << ON_id << std::endl;
    //}
    return true;
}


ON_BoundingBox *
Path::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;

    LIST_OF_ORIENTED_EDGES::iterator i;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) {
	    delete u;
	    return NULL;
	}
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    delete u;
	    return NULL;
	}
	if (u == NULL) {
	    u = new ON_BoundingBox();
	}
	const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	const ON_Curve *curve = edge->EdgeCurveOf();
	u->Union(curve->BoundingBox());
    }

    return u;
}


bool
Path::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;
    LIST_OF_ORIENTED_EDGES::iterator i;

    if ((false) && (id == 29429)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }

    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	if (!(*i)->LoadONBrep(brep)) {
	    if (step && step->Verbose())
		std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    //TODO: remove debugging code
    if ((false) && (id == 26089)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }
    if (!LoadONTrimmingCurves(brep)) {
	return false;
    }

    return true;
}


bool
Path::ShiftSurfaceSeam(ON_Brep *brep, double *t)
{
    const ON_BrepLoop *loop = NULL;
    const ON_BrepFace *face = NULL;
    const ON_Surface *surface = NULL;
    double ang_min = 0.0;
    double smin, smax;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    loop = &brep->m_L[ON_path_index];
    if (!loop) {
	/* nothing to do */
	return false;
    }

    face = loop->Face();
    if (!face) {
	/* nothing to do */
	return false;
    }

    surface = face->SurfaceOf();
    if (!surface) {
	/* nothing to do */
	return false;
    }

    if (surface->IsCone() || surface->IsCylinder()) {
	if (surface->IsClosed(0)) {
	    surface->GetDomain(0, &smin, &smax);
	} else {
	    surface->GetDomain(1, &smin, &smax);
	}

	LIST_OF_ORIENTED_EDGES::iterator i;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();
	    double tmin, tmax;
	    curve->GetDomain(&tmin, &tmax);

	    if (((tmin < 0.0) && (tmax > 0.0)) && ((tmin > smin) || (tmax < smax))) {
		V_MIN(ang_min, tmin);
	    }

	}

	if (ang_min < 0.0) {
	    *t = ang_min;
	    return true;
	}
    }
    return false;
}


#ifdef _DEBUG_TESTING_
bool _debug_print_ = false;
#endif


/* Insert the standard exact BREP representation for a face boundary that
 * winds around a surface pole.  The two trims of the new seam edge have
 * opposite 3-D orientation and therefore add no geometric boundary; the
 * singular trim connects their UV endpoints across the collapsed pole. */
static bool
insert_periodic_pole_cut(ON_Brep *brep, ON_BrepLoop &loop,
    const ON_Surface *surface, const ON_BrepTrim &boundary_trim,
    const ON_2dPoint &boundary_end, const ON_2dPoint &boundary_start,
    double tolerance)
{
    if (!brep || !surface || !(tolerance > 0.0) ||
	    boundary_trim.m_vi[1] < 0 ||
	    boundary_trim.m_vi[1] >= brep->m_V.Count())
	return false;

    int seam_direction = -1;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	const double gap = fabs(boundary_start[direction] -
	    boundary_end[direction]);
	const double parameter_tolerance = kPeriodicParameterSnapFraction *
	    std::max(1.0, period);
	const bool endpoints_are_seam_sides =
	    (fabs(boundary_end[direction] - domain.Min()) <= parameter_tolerance &&
	     fabs(boundary_start[direction] - domain.Max()) <= parameter_tolerance) ||
	    (fabs(boundary_end[direction] - domain.Max()) <= parameter_tolerance &&
	     fabs(boundary_start[direction] - domain.Min()) <= parameter_tolerance);
	if (fabs(gap - period) <= parameter_tolerance && endpoints_are_seam_sides) {
	    seam_direction = direction;
	    break;
	}
    }
    if (seam_direction < 0)
	return false;

    const int singular_direction = 1 - seam_direction;
    const ON_Interval singular_domain = surface->Domain(singular_direction);
    const double boundary_parameter = 0.5 *
	(boundary_end[singular_direction] + boundary_start[singular_direction]);
    int singular_side = -1;
    if (singular_direction == 0) {
	if (surface->IsSingular(3)) singular_side = 0; /* west */
	if (surface->IsSingular(1) && (singular_side < 0 ||
		fabs(boundary_parameter - singular_domain.Max()) <
		fabs(boundary_parameter - singular_domain.Min())))
	    singular_side = 1; /* east */
    } else {
	if (surface->IsSingular(0)) singular_side = 0; /* south */
	if (surface->IsSingular(2) && (singular_side < 0 ||
		fabs(boundary_parameter - singular_domain.Max()) <
		fabs(boundary_parameter - singular_domain.Min())))
	    singular_side = 1; /* north */
    }
    if (singular_side < 0)
	return false;

    ON_2dPoint first_pole = boundary_end;
    ON_2dPoint second_pole = boundary_start;
    first_pole[singular_direction] = singular_domain[singular_side];
    second_pole[singular_direction] = singular_domain[singular_side];
    const ON_3dPoint boundary_lift = surface->PointAt(
	boundary_end.x, boundary_end.y);
    const ON_3dPoint start_lift = surface->PointAt(
	boundary_start.x, boundary_start.y);
    const ON_3dPoint first_pole_lift = surface->PointAt(
	first_pole.x, first_pole.y);
    const ON_3dPoint second_pole_lift = surface->PointAt(
	second_pole.x, second_pole.y);
    const ON_BrepVertex &boundary_vertex =
	brep->m_V[boundary_trim.m_vi[1]];
    if (!boundary_lift.IsValid() || !start_lift.IsValid() ||
	    !first_pole_lift.IsValid() || !second_pole_lift.IsValid() ||
	    boundary_lift.DistanceTo(start_lift) > tolerance ||
	    boundary_lift.DistanceTo(boundary_vertex.Point()) > tolerance ||
	    first_pole_lift.DistanceTo(second_pole_lift) > tolerance)
	return false;

    std::unique_ptr<ON_Curve> seam_curve(surface->IsoCurve(
	singular_direction, boundary_end[seam_direction]));
    const ON_Interval seam_subdomain(
	std::min(boundary_end[singular_direction],
	    singular_domain[singular_side]),
	std::max(boundary_end[singular_direction],
	    singular_domain[singular_side]));
    if (!seam_curve || !seam_subdomain.IsIncreasing() ||
	    !seam_curve->Trim(seam_subdomain) || !seam_curve->IsValid())
	return false;
    if (seam_curve->PointAtStart().DistanceTo(boundary_lift) >
	    seam_curve->PointAtEnd().DistanceTo(boundary_lift) &&
	    !seam_curve->Reverse())
	return false;
    if (seam_curve->PointAtStart().DistanceTo(boundary_lift) > tolerance ||
	    seam_curve->PointAtEnd().DistanceTo(first_pole_lift) > tolerance)
	return false;

    std::unique_ptr<ON_LineCurve> first_trim_curve(
	new ON_LineCurve(boundary_end, first_pole));
    std::unique_ptr<ON_LineCurve> singular_trim_curve(
	new ON_LineCurve(first_pole, second_pole));
    std::unique_ptr<ON_LineCurve> second_trim_curve(
	new ON_LineCurve(second_pole, boundary_start));
    if (!first_trim_curve->ChangeDimension(2) ||
	    !singular_trim_curve->ChangeDimension(2) ||
	    !second_trim_curve->ChangeDimension(2) ||
	    !first_trim_curve->IsValid() || !singular_trim_curve->IsValid() ||
	    !second_trim_curve->IsValid())
	return false;

    const int seam_curve_index = brep->AddEdgeCurve(seam_curve.release());
    const int first_trim_index = brep->AddTrimCurve(first_trim_curve.release());
    const int singular_trim_index = brep->AddTrimCurve(
	singular_trim_curve.release());
    const int second_trim_index = brep->AddTrimCurve(second_trim_curve.release());
    if (seam_curve_index < 0 || first_trim_index < 0 ||
	    singular_trim_index < 0 || second_trim_index < 0)
	return false;

    ON_BrepVertex &pole_vertex = brep->NewVertex(first_pole_lift, tolerance);
    ON_BrepEdge &seam_edge = brep->NewEdge(
	brep->m_V[boundary_trim.m_vi[1]], pole_vertex, seam_curve_index,
	NULL, tolerance);
    ON_BrepTrim &first_seam = brep->NewTrim(seam_edge, false, loop,
	first_trim_index);
    const ON_Surface::ISO singular_iso = singular_direction == 0 ?
	(singular_side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
	(singular_side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
    ON_BrepTrim &singular_trim = brep->NewSingularTrim(pole_vertex, loop,
	singular_iso, singular_trim_index);
    ON_BrepTrim &second_seam = brep->NewTrim(seam_edge, true, loop,
	second_trim_index);

    const ON_Interval seam_domain = surface->Domain(seam_direction);
    const bool first_is_min = fabs(boundary_end[seam_direction] -
	seam_domain.Min()) <= fabs(boundary_end[seam_direction] -
	seam_domain.Max());
    first_seam.m_type = ON_BrepTrim::seam;
    second_seam.m_type = ON_BrepTrim::seam;
    first_seam.m_iso = seam_direction == 0 ?
	(first_is_min ? ON_Surface::W_iso : ON_Surface::E_iso) :
	(first_is_min ? ON_Surface::S_iso : ON_Surface::N_iso);
    second_seam.m_iso = seam_direction == 0 ?
	(first_is_min ? ON_Surface::E_iso : ON_Surface::W_iso) :
	(first_is_min ? ON_Surface::N_iso : ON_Surface::S_iso);
    singular_trim.m_iso = singular_iso;
    first_seam.m_tolerance[0] = first_seam.m_tolerance[1] = tolerance;
    second_seam.m_tolerance[0] = second_seam.m_tolerance[1] = tolerance;
    singular_trim.m_tolerance[0] = singular_trim.m_tolerance[1] = tolerance;
    seam_edge.m_tolerance = tolerance;
    return true;
}


bool
Path::LoadONTrimmingCurves(ON_Brep *brep)
{
    LIST_OF_ORIENTED_EDGES::iterator i;
    list<PBCData *> curve_pullback_samples;
    std::map<PBCData *, ON_Curve *> exact_pullbacks;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_BrepLoop *loop = &brep->m_L[ON_path_index];
    loop->m_loop_user.i = id;
    ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face->SurfaceOf();
    const int initial_trim_count = loop->TrimCount();
    const ON_BoundingBox item_bounds = brep->BoundingBox();
    const double item_scale = item_bounds.IsValid() ?
	item_bounds.Diagonal().Length() : 0.0;

#if 0
    if (surface) {
	double surface_width, surface_height;
	if (surface->GetSurfaceSize(&surface_width, &surface_height)) {
	    // reparameterization of the face's surface and transforms the "u"
	    // and "v" coordinates of all the face's parameter space trimming
	    // curves to minimize distortion in the map from parameter space to 3d..
	    face->SetDomain(0, 0.0, surface_width);
	    face->SetDomain(1, 0.0, surface_height);
	}
    }
#endif
#ifdef _DEBUG_TESTING_
    if (_debug_print_) {
	int curve_cnt = 0;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge *edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve *curve = edge->EdgeCurveOf();

	    ON_Interval interval = curve->Domain();
	    double delta = interval.Length()/100.0;
	    for(int j =0; j < 100; j++) {
		ON_3dPoint p = curve->PointAt(interval.m_t[0] + j*delta);
		std::cerr << "in pt_" << curve_cnt << " sph " << p.x << " " << p.y << " " << p.z << " 0.1000"  << std::endl;
		curve_cnt++;
	    }
	}
    }
#endif
    // build surface tree making sure not to remove trimmed subsurfaces
    // since currently building trims and need full tree
    // bool removeTrimmed = false;

    //TODO: remove debugging code
    if ((false) && (id == 24894)) {
	std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
    }
    struct EdgePullbackResult {
	PBCData *data = NULL;
	ON_Curve *exact_curve = NULL;
	const ON_BrepEdge *edge = NULL;
	OrientedEdge *oriented_edge = NULL;
	bool orient_with_curve = true;
	bool diagnostic_recorded = false;
    };
    std::vector<OrientedEdge *> ordered_edges(edge_list.begin(), edge_list.end());
    std::vector<EdgePullbackResult> edge_results(ordered_edges.size());
    const auto construct_edge_pullback = [&](size_t edge_index) {
	EdgePullbackResult &result = edge_results[edge_index];
	result.oriented_edge = ordered_edges[edge_index];
	if (!result.oriented_edge) return;
	result.edge = &brep->m_E[result.oriented_edge->GetONId()];
	result.orient_with_curve = result.oriented_edge->OrientWithEdge();
	/* The parent item reports its bounded-work expiry.  Suppress a second,
	 * misleading "edge #-1" diagnostic when a helper observes that shared
	 * deadline before beginning this particular edge. */
	if (brlcad::PullbackWorkCancelled()) {
	    result.diagnostic_recorded = true;
	    return;
	}
	if (step)
	    step->SetProgressDetail("constructing exact STEP trim pullback",
		result.oriented_edge->STEPid(), 0, 0, std::string(),
		"loop=#" + std::to_string(id));
	const ON_Curve *curve = result.edge->EdgeCurveOf();
	PBCData *edge_data = NULL;

	if ((false) && (id == 34193)) {
	    std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
	}
	double planar_tolerance_limit = LocalUnits::tolerance;
	if (step && !step->ImportOptions().exact &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe)
	    planar_tolerance_limit = maximum_verified_edge_tolerance(curve,
		LocalUnits::tolerance, item_scale);
	edge_data = exact_planar_pullback(surface, curve, LocalUnits::tolerance,
	    planar_tolerance_limit, &result.exact_curve);
	if (!edge_data)
	    edge_data = exact_isoparametric_line_pullback(surface, curve,
		LocalUnits::tolerance, &result.exact_curve);
	if (!edge_data) {
	    if (step && step->Verbose() && ON_PlaneSurface::Cast(surface))
		std::cerr << "EDGE_LOOP #" << id << ": exact planar trim rejected for edge #"
		    << result.oriented_edge->STEPid() << "; using bounded pullback" << std::endl;
	    edge_data = pullback_samples(surface, curve, LocalUnits::tolerance,
		LocalUnits::tolerance, LocalUnits::tolerance, LocalUnits::tolerance);
	    if (edge_data && pullback_sample_count(edge_data) < 2) {
		const double resolution = short_curve_pullback_resolution(curve,
		    LocalUnits::tolerance);
		PBCData *refined = pullback_samples(surface, curve,
		    LocalUnits::tolerance, LocalUnits::tolerance,
		    resolution, resolution);
		if (refined && pullback_sample_count(refined) >= 2) {
		    destroy_pullback_data(edge_data);
		    edge_data = refined;
		    if (step && step->ImportOptions().repair ==
			    brlcad::step::RepairMode::Safe)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "refined pullback resolution for a feature below model tolerance");
		} else {
		    destroy_pullback_data(refined);
		}
	    }
	    if (edge_data && edge_data->rejected_projection_samples > 0 && step &&
		    !step->ImportOptions().exact &&
		    step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		    edge_data->maximum_projection_distance > LocalUnits::tolerance &&
		    edge_data->rejected_projection_samples >
			edge_data->failed_projection_samples) {
		const double adjustment_limit = maximum_verified_edge_tolerance(
		    curve, LocalUnits::tolerance, item_scale);
		double effective_tolerance = edge_data->maximum_projection_distance *
		    kMeasuredToleranceSafetyFactor;
		double measured_tolerance = effective_tolerance;
		PBCData *accepted_adjustment = NULL;
		for (int attempt = 0;
			attempt < kMaximumMeasuredToleranceRetries &&
			effective_tolerance <= adjustment_limit; ++attempt) {
		    PBCData *adjusted = pullback_samples(surface, curve,
			effective_tolerance, effective_tolerance,
			effective_tolerance, effective_tolerance);
		    if (adjusted && pullback_sample_count(adjusted) >= 2 &&
			    adjusted->rejected_projection_samples == 0) {
			accepted_adjustment = adjusted;
			accepted_adjustment->tolerance = measured_tolerance;
			accepted_adjustment->flatness = measured_tolerance;
			break;
		    }
		    double next_tolerance = 0.0;
		    if (adjusted) {
			/* Preserve the most informative bounded failure in case no
			 * retry can validate every sample. */
			edge_data->failure_reason = adjusted->failure_reason;
			edge_data->projection_samples = adjusted->projection_samples;
			edge_data->rejected_projection_samples =
			    adjusted->rejected_projection_samples;
			edge_data->failed_projection_samples =
			    adjusted->failed_projection_samples;
			edge_data->maximum_projection_distance =
			    adjusted->maximum_projection_distance;
			if (adjusted->maximum_projection_distance >
				0.0) {
			    measured_tolerance = std::max(measured_tolerance,
				adjusted->maximum_projection_distance *
				kMeasuredToleranceSafetyFactor);
			    next_tolerance = std::max(measured_tolerance,
				effective_tolerance * 2.0);
			}
		    }
		    destroy_pullback_data(adjusted);
		    if (!(next_tolerance > effective_tolerance)) break;
		    effective_tolerance = next_tolerance;
		}
		if (accepted_adjustment) {
		    destroy_pullback_data(edge_data);
		    edge_data = accepted_adjustment;
		    edge_data->tolerance_adjusted = true;
		    edge_data->declared_tolerance = LocalUnits::tolerance;
		}
	    }
	    if (edge_data && result.edge && result.edge->Vertex(0) &&
		    result.edge->Vertex(1)) {
		/* An EDGE_CURVE also asserts that its 3-D curve terminates at its
		 * two VERTEX_POINTs.  Some exchange files violate that assertion
		 * even when the curve-to-surface pullback is exact.  Reflect the
		 * measured endpoint separation in the OpenNURBS edge tolerance only
		 * after applying the same scale-relative safe-repair bound. */
		const double start_gap = curve->PointAtStart().DistanceTo(
		    result.edge->Vertex(0)->Point());
		const double end_gap = curve->PointAtEnd().DistanceTo(
		    result.edge->Vertex(1)->Point());
		const double endpoint_gap = std::max(start_gap, end_gap);
		if (ON_IsValid(endpoint_gap) && endpoint_gap > edge_data->tolerance) {
		    const double adjustment_limit = maximum_verified_edge_tolerance(
			curve, LocalUnits::tolerance, item_scale);
		    const double effective_tolerance = endpoint_gap *
			kMeasuredToleranceSafetyFactor;
		    if (step && !step->ImportOptions().exact &&
			    step->ImportOptions().repair ==
				brlcad::step::RepairMode::Safe &&
			    effective_tolerance <= adjustment_limit) {
			edge_data->tolerance = effective_tolerance;
			edge_data->flatness = effective_tolerance;
			edge_data->tolerance_adjusted = true;
			edge_data->declared_tolerance = LocalUnits::tolerance;
		    } else {
			if (step) {
			    std::ostringstream reason;
			    reason << "source edge curve endpoint separation "
				<< endpoint_gap << " exceeds ";
			    if (step->ImportOptions().exact)
				reason << "the exact model tolerance "
				    << LocalUnits::tolerance;
			    else
				reason << "what could be validated within the bounded "
				    "safe-repair limit "
				    << adjustment_limit << " (declared "
				    << LocalUnits::tolerance << ')';
			    reason << " on STEP edge #" << result.edge->m_edge_user.i;
			    step->RecordDiagnostic(
				brlcad::step::DiagnosticSeverity::Error, id,
				"EDGE_LOOP", "edge_list", reason.str());
			    result.diagnostic_recorded = true;
			}
			destroy_pullback_data(edge_data);
			edge_data = NULL;
		    }
		}
	    }
	    if (edge_data && edge_data->rejected_projection_samples > 0) {
		if (step) {
		    std::ostringstream reason;
		    const int edge_id = result.edge ? result.edge->m_edge_user.i : -1;
		    if (edge_data->failure_reason == PullbackFailureReason::Cancelled) {
			reason << "trim validation was cancelled for STEP edge #"
			    << edge_id << " after its per-item work budget expired";
		    } else if (edge_data->failure_reason == PullbackFailureReason::SurfaceDistanceExceeded) {
			reason << "source curve/surface separation "
			    << edge_data->maximum_projection_distance << " exceeds ";
			if (step->ImportOptions().exact)
			    reason << "the exact model tolerance " << LocalUnits::tolerance;
			else
			    reason << "what could be validated within the bounded "
				"safe-repair limit "
				<< maximum_verified_edge_tolerance(curve,
				    LocalUnits::tolerance, item_scale) << " (declared "
				<< LocalUnits::tolerance << ')';
			reason << " on STEP edge #" << edge_id
			    << " (" << edge_data->rejected_projection_samples << '/'
			    << edge_data->projection_samples << " projection samples rejected)";
		    } else {
			reason << "bounded closest-point projection failed for STEP edge #"
			    << edge_id << " (" << edge_data->rejected_projection_samples << '/'
			    << edge_data->projection_samples << " samples rejected, "
			    << edge_data->failed_projection_samples
			    << " without a finite closest-point candidate) in STEP loop #"
			    << id;
			const ON_ClassId *surface_class = surface ? surface->ClassId() : NULL;
			if (surface_class)
			    reason << " on " << surface_class->ClassName();
			if (edge_data->maximum_projection_distance > 0.0)
			    reason << "; largest finite rejected distance was "
				<< edge_data->maximum_projection_distance;
		    }
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
			"EDGE_LOOP", "edge_list", reason.str());
		    result.diagnostic_recorded = true;
		}
		destroy_pullback_data(edge_data);
		edge_data = NULL;
	    }
	}
	result.data = edge_data;
    };

    if (step)
	step->ParallelForGeometry(edge_results.size(), construct_edge_pullback);
    else
	for (size_t edge_index = 0; edge_index < edge_results.size(); ++edge_index)
	    construct_edge_pullback(edge_index);

    PBCData *data = NULL;
    for (size_t edge_index = 0; edge_index < edge_results.size(); ++edge_index) {
	EdgePullbackResult &result = edge_results[edge_index];
	data = result.data;
	if (data == NULL) {
	    if (step && !result.diagnostic_recorded) {
		std::ostringstream reason;
		reason << "STEP edge #" << (result.edge ?
		    result.edge->m_edge_user.i : -1)
		    << " did not produce bounded pullback data";
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		    "EDGE_LOOP", "edge_list", reason.str());
	    }
	    if (step && step->Verbose())
		std::cerr << "EDGE_LOOP #" << id << ": could not construct a validated trim for edge #"
		    << (result.oriented_edge ? result.oriented_edge->STEPid() : -1)
		    << std::endl;
	    continue;
	}
	if (result.exact_curve)
	    exact_pullbacks[data] = result.exact_curve;
	if (step && data->failed_seam_crossing_searches > 0) {
	    for (size_t failure = 0;
		    failure < data->failed_seam_crossing_searches; ++failure)
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
		    id, "EDGE_LOOP", "edge_list",
		    "closed-curve seam crossing was deferred to bounded loop-level resolution");
	}

	if (!result.orient_with_curve) {
	    data->order_reversed = true;
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && exact->second && !exact->second->Reverse()) {
		delete exact->second;
		exact_pullbacks.erase(exact);
	    }
	} else {
	    data->order_reversed = false;
	}
	data->edge = result.edge;
	curve_pullback_samples.push_back(data);
	if (!result.orient_with_curve) {
	    list<ON_2dPointArray *>::iterator si;
	    si = data->segments->begin();
	    list<ON_2dPointArray *> rsegs;
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		samples->Reverse();
		rsegs.push_front(samples);
		si++;
	    }
	    data->segments->clear();
	    si = rsegs.begin();
	    while (si != rsegs.end()) {
		ON_2dPointArray *samples = (*si);
		data->segments->push_back(samples);
		si++;
	    }
	    rsegs.clear();
	}
    }
    if (curve_pullback_samples.size() != edge_list.size()) {
	if (step)
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
		"EDGE_LOOP", "edge_list",
		"one or more STEP edges did not produce validated pullback data");
	for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	     exact != exact_pullbacks.end(); ++exact)
	    delete exact->second;
	while (!curve_pullback_samples.empty()) {
	    data = curve_pullback_samples.front();
	    while (data->segments && !data->segments->empty()) {
		delete data->segments->front();
		data->segments->pop_front();
	    }
	    delete data->segments;
	    delete data;
	    curve_pullback_samples.pop_front();
	}
	return false;
    }
    const auto log_adjusted_endpoints = [this](const char *stage,
	const std::list<PBCData *> &pullbacks) {
	if (!step || !step->Verbose()) return;
	for (std::list<PBCData *>::const_iterator current = pullbacks.begin();
	     current != pullbacks.end(); ++current) {
	    const PBCData *current_data = *current;
	    if (!current_data || !current_data->tolerance_adjusted ||
		    !current_data->segments || current_data->segments->empty() ||
		    !current_data->segments->front() ||
		    current_data->segments->front()->Count() == 0)
		continue;
	    const ON_2dPoint uv = (*current_data->segments->front())[0];
	    const ON_3dPoint lift = current_data->surf->PointAt(uv.x, uv.y);
	    std::cerr << "EDGE_LOOP #" << id << ": " << stage
		<< " adjusted STEP edge #"
		<< (current_data->edge ? current_data->edge->m_edge_user.i : -1)
		<< " first uv=" << uv.x << ':' << uv.y << " locus error="
		<< (lift.IsValid() ? distance_to_curve(current_data->curve, lift) : DBL_MAX)
		<< std::endl;
	}
    };
    log_adjusted_endpoints("before seam resolution", curve_pullback_samples);
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("Before check_pullback_data", curve_pullback_samples, false);
    }
#endif
    // Exact affine planar pullbacks cannot cross a parametric seam or end at
    // a surface singularity.  Preserve those curves verbatim; the general
    // seam resolver is intentionally limited to periodic/non-planar results.
    const bool exact_open_surface = exact_pullbacks.size() == curve_pullback_samples.size() &&
	!surface->IsClosed(0) && !surface->IsClosed(1);
    // check for seams and singularities
    if (!exact_open_surface && !check_pullback_data(curve_pullback_samples)) {
	std::cerr << "Error: Can not resolve seam or singularity issues." << std::endl;
	for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	     exact != exact_pullbacks.end(); ++exact)
	    delete exact->second;
	while (!curve_pullback_samples.empty()) {
	    destroy_pullback_data(curve_pullback_samples.front());
	    curve_pullback_samples.pop_front();
	}
	return false;
    }
    log_adjusted_endpoints("after seam resolution", curve_pullback_samples);

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	size_t normalized_segments = 0;
	if (normalize_periodic_pullback_segments(curve_pullback_samples, surface,
		LocalUnits::tolerance, &normalized_segments)) {
	    for (size_t repair = 0; repair < normalized_segments; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "translated an exact pcurve onto the native periodic surface domain");
	}
	std::string pole_cut_failure;
	const bool aligned_pole_cut = align_surface_seam_with_periodic_loop_cut(
	    curve_pullback_samples, brep, face, surface, LocalUnits::tolerance,
	    &pole_cut_failure);
	if (aligned_pole_cut) {
	    for (std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.begin(); exact != exact_pullbacks.end(); ++exact)
		delete exact->second;
	    exact_pullbacks.clear();
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"aligned a closed surface seam with an exact pole cut");
	} else if (align_nurbs_surface_seam(curve_pullback_samples, surface,
		LocalUnits::tolerance)) {
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"aligned a periodic NURBS surface seam with its closed edge");
	} else {
	    std::string relocation_failure;
	    if (relocate_surface_seam_away_from_boundary(
		    curve_pullback_samples, brep, face, surface,
		    LocalUnits::tolerance,
		    &relocation_failure)) {
	    /* Exact pcurve objects were expressed in the original surface
	     * parameterization.  The remapped samples are fully lift-validated;
	     * rebuild curves from them rather than reusing stale parameter curves. */
	    for (std::map<PBCData *, ON_Curve *>::iterator exact =
		    exact_pullbacks.begin(); exact != exact_pullbacks.end(); ++exact)
		delete exact->second;
	    exact_pullbacks.clear();
	    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		"relocated a closed rational surface seam outside the face boundary");
	    } else if (step->Verbose() && !relocation_failure.empty()) {
		std::cerr << "EDGE_LOOP #" << id
		    << ": periodic surface seam relocation rejected: "
		    << relocation_failure << std::endl;
	    }
	    if (step->Verbose() && !pole_cut_failure.empty())
		std::cerr << "EDGE_LOOP #" << id
		    << ": periodic pole-cut alignment rejected: "
		    << pole_cut_failure << std::endl;
	}
    }

    if (step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->edge)
		continue;
	    size_t edge_uses = 0;
	    PBCData *paired_data = NULL;
	    for (std::list<PBCData *>::const_iterator other = curve_pullback_samples.begin();
		 other != curve_pullback_samples.end(); ++other) {
		if (*other && (*other)->edge == current_data->edge) {
		    ++edge_uses;
		    if (*other != current_data && paired_data == NULL)
			paired_data = *other;
		}
	    }
	    /* Process the pair only from its first occurrence in loop order. */
	    bool current_is_first = true;
	    for (std::list<PBCData *>::const_iterator prior = curve_pullback_samples.begin();
		 prior != current && prior != curve_pullback_samples.end(); ++prior) {
		if (*prior && (*prior)->edge == current_data->edge) {
		    current_is_first = false;
		    break;
		}
	    }
	if (edge_uses == 2 && current_is_first && snap_seam_pullback_pair(
		curve_pullback_samples, current_data, paired_data, loop->m_type, LocalUnits::tolerance)) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "normalized seam pcurve within model tolerance");
	    }
	}
	/* Normalize proven closed boundary edges before closing adjacent samples.
	 * Doing this during trim emission is too late: a boundary line can move an
	 * endpoint after its neighboring seam curve has already been constructed. */
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    if (brlcad::PullbackWorkCancelled()) break;
	    PBCData *current_data = *current;
	    if (!current_data || !current_data->segments)
		continue;
	    for (std::list<ON_2dPointArray *>::iterator segment = current_data->segments->begin();
		 segment != current_data->segments->end(); ++segment) {
		if (!*segment || (*segment)->Count() < 2)
		    continue;
		ON_Curve *boundary = closed_edge_iso_line(current_data, **segment,
		    LocalUnits::tolerance);
		if (!boundary)
		    continue;
		const ON_3dPoint start = boundary->PointAtStart();
		const ON_3dPoint end = boundary->PointAtEnd();
		(*segment)->At(0)->Set(start.x, start.y);
		(*segment)->At((*segment)->Count() - 1)->Set(end.x, end.y);
		delete boundary;
	    }
	}
	const size_t endpoint_repairs = snap_pullback_loop_endpoints(
	    curve_pullback_samples, brep, LocalUnits::tolerance);
	if (endpoint_repairs) {
	    for (size_t repair = 0; repair < endpoint_repairs; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "snapped adjacent pcurve endpoint within validated edge tolerance");
	}
	if (endpoint_repairs) {
	    bool seam_changed = false;
	    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
		 current != curve_pullback_samples.end(); ++current) {
		if (brlcad::PullbackWorkCancelled()) break;
		PBCData *current_data = *current;
		if (!current_data || !current_data->edge)
		    continue;
		PBCData *paired_data = NULL;
		size_t edge_uses = 0;
		bool current_is_first = true;
		bool reached_current = false;
		for (std::list<PBCData *>::iterator other = curve_pullback_samples.begin();
		     other != curve_pullback_samples.end(); ++other) {
		    if (other == current) reached_current = true;
		    if (*other && (*other)->edge == current_data->edge) {
			++edge_uses;
			if (other != current && !paired_data) paired_data = *other;
			if (other != current && !reached_current) current_is_first = false;
		    }
		}
		if (edge_uses == 2 && current_is_first && snap_seam_pullback_pair(
			curve_pullback_samples, current_data, paired_data, loop->m_type,
			LocalUnits::tolerance)) {
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"normalized seam pcurve within model tolerance");
		    seam_changed = true;
		}
	    }
	    if (seam_changed) {
		const size_t final_endpoint_repairs = snap_pullback_loop_endpoints(
		    curve_pullback_samples, brep, LocalUnits::tolerance);
		for (size_t repair = 0; repair < final_endpoint_repairs; ++repair)
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"snapped adjacent pcurve endpoint within validated edge tolerance");
	    }
	}
	/* Exact loop closure can select the equivalent endpoint on the opposite
	 * side of a periodic domain.  Re-run whole-segment unwrapping after that
	 * endpoint choice so the newly closed join cannot leave a one-period jump
	 * between the endpoint and its first interior pullback sample. */
	size_t post_snap_normalized_segments = 0;
	if (normalize_periodic_pullback_segments(curve_pullback_samples, surface,
		LocalUnits::tolerance, &post_snap_normalized_segments)) {
	    for (size_t repair = 0; repair < post_snap_normalized_segments; ++repair)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "unwrapped a periodic pcurve after exact loop closure");
	}
	for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	     current != curve_pullback_samples.end(); ++current) {
	    size_t seam_splits = 0;
	    if (!split_periodic_pullback_at_native_seams(*current,
		    LocalUnits::tolerance, &seam_splits))
		continue;
	    const bool singular_split = pullback_requires_singular_topology_split(
		*current, std::max(LocalUnits::tolerance, (*current)->tolerance));
	    if (!singular_split && merge_ordinary_periodic_pullback_fragments(
		    *current, LocalUnits::tolerance)) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "rejoined an ordinary periodic seam crossing into one exact edge use");
		continue;
	    }
	    for (size_t split = 0; split < seam_splits; ++split)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "split a periodic pcurve at a proven surface singularity");
	}
    }
    log_adjusted_endpoints("after endpoint repair", curve_pullback_samples);

    /* A single STEP edge can require multiple continuous pcurve fragments
     * when it crosses a surface singularity.  Reserve all possible subedges
     * before creating any of them; ON_SimpleArray growth would otherwise
     * invalidate the PBCData edge pointers retained during this loop. */
    int possible_subedges = 0;
    std::map<PBCData *, int> source_edge_indices;
    std::map<PBCData *, bool> singular_topology_splits;
    for (std::list<PBCData *>::iterator current = curve_pullback_samples.begin();
	    current != curve_pullback_samples.end(); ++current) {
	if (!*current || !(*current)->edge || !(*current)->segments)
	    continue;
	source_edge_indices[*current] = (*current)->edge->m_edge_index;
	const bool singular_split = pullback_requires_singular_topology_split(
	    *current, std::max(LocalUnits::tolerance, (*current)->tolerance));
	singular_topology_splits[*current] = singular_split;
	if (singular_split)
	    possible_subedges += static_cast<int>((*current)->segments->size());
    }
    const bool surface_has_pole = surface &&
	(surface->IsSingular(0) || surface->IsSingular(1) ||
	 surface->IsSingular(2) || surface->IsSingular(3));
    const int possible_pole_cuts = surface_has_pole ?
	static_cast<int>(curve_pullback_samples.size()) : 0;
    if (possible_subedges > 0 || possible_pole_cuts > 0) {
	brep->m_E.Reserve(brep->m_E.Count() + possible_subedges +
	    possible_pole_cuts);
	for (std::map<PBCData *, int>::const_iterator source =
		source_edge_indices.begin(); source != source_edge_indices.end(); ++source) {
	    if (source->second >= 0 && source->second < brep->m_E.Count())
		source->first->edge = &brep->m_E[source->second];
	}
    }
#ifdef _DEBUG_TESTING_
    //TODO: remove debugging
    if (_debug_print_) {
	std::cerr << "Face " << face->m_face_index << " id " << id << std::endl;
	print_pullback_data("After check_pullback_data", curve_pullback_samples, false);
    }
#endif
    list<PBCData *>::iterator cs = curve_pullback_samples.begin();
    list<PBCData *>::iterator next_cs;
    bool trim_construction_failed = false;

    cs = curve_pullback_samples.begin();
    while (cs != curve_pullback_samples.end()) {
	if (brlcad::PullbackWorkCancelled()) {
	    trim_construction_failed = true;
	    break;
	}
	next_cs = cs;
	next_cs++;
	if (next_cs == curve_pullback_samples.end()) {
	    next_cs = curve_pullback_samples.begin();
	}
	data = (*cs);
	const bool split_at_singularity = singular_topology_splits[data];
	list<ON_2dPointArray *>::iterator si;
	si = data->segments->begin();
	PBCData *ndata = (*next_cs);
	list<ON_2dPointArray *>::iterator nsi;
	nsi = ndata->segments->begin();
	ON_2dPointArray *nsamples = NULL;

	while (si != data->segments->end()) {
	    if (brlcad::PullbackWorkCancelled()) {
		trim_construction_failed = true;
		break;
	    }
	    nsi = si;
	    nsi++;
	    if (nsi == data->segments->end()) {
		PBCData *nsidata = (*next_cs);
		nsi = nsidata->segments->begin();
	    }
	    ON_2dPointArray *samples = (*si);
	    nsamples = (*nsi);

	    int trimCurve = brep->m_C2.Count();
	    //TODO: remove debugging code
	    if ((false) && (trimCurve == 68)) {
		std::cerr << "Debug:LoadONTrimmingCurves for Path:" << trimCurve << std::endl;
	    }
	    ON_Curve *c2d = NULL;
	    bool used_polyline_fallback = false;
	    /* A short exact boundary can collapse to one pullback sample when its
	     * length is below the asserted model uncertainty.  Do not silently
	     * omit that topology edge.  Leave c2d unset so the bounded adjacent-
	     * endpoint reconstruction below can prove and restore the pcurve. */
	    if (samples && samples->Count() >= 2) {
	    std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.find(data);
	    if (exact != exact_pullbacks.end() && data->segments->size() == 1 &&
		exact->second) {
		const ON_Interval exact_domain = exact->second->Domain();
	const ON_3dPoint exact_start = exact->second->PointAt(exact_domain.Min());
	const ON_3dPoint exact_end = exact->second->PointAt(exact_domain.Max());
	if (ON_2dPoint(exact_start.x, exact_start.y).DistanceTo((*samples)[0]) <=
			ON_ZERO_TOLERANCE &&
		    ON_2dPoint(exact_end.x, exact_end.y).DistanceTo(
			(*samples)[samples->Count() - 1]) <= ON_ZERO_TOLERANCE) {
		    c2d = exact->second;
		    exact->second = NULL;
		}
	    }
	    bool regenerated = false;
	    if (!c2d && step && step->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
		c2d = closed_edge_iso_line(data, *samples, LocalUnits::tolerance);
		if (c2d && (c2d->PointAtStart().DistanceTo((*samples)[0]) >
			ON_ZERO_TOLERANCE ||
		    c2d->PointAtEnd().DistanceTo((*samples)[samples->Count() - 1]) >
			ON_ZERO_TOLERANCE)) {
		    delete c2d;
		    c2d = NULL;
		}
		regenerated = c2d != NULL;
	    }
	    if (regenerated) {
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "regenerated closed-edge boundary pcurve within model tolerance");
	    } else if (!c2d) {
		const bool closed_edge = data->edge &&
		    data->edge->m_vi[0] == data->edge->m_vi[1];
		if (data->tolerance_adjusted) {
		    /* A smooth interpolant can overshoot between closest-point samples
		     * by far more than the measured source mismatch.  The BREP's 3-D
		     * edge remains the original exact curve; use the bounded UV sample
		     * path solely as its trim association and prove it densely below. */
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample)
			points.Append(ON_3dPoint((*samples)[sample].x,
			    (*samples)[sample].y, 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(points);
		    if (polyline->ChangeDimension(2) && polyline->IsValid()) {
			c2d = polyline;
			used_polyline_fallback = true;
		    } else {
			delete polyline;
		    }
		} else if (split_at_singularity) {
		    std::string refinement_failure;
		    (void)refined_fragment_polyline(data, *samples,
			LocalUnits::tolerance, &c2d, &refinement_failure);
		    if (!c2d && step && step->Verbose() &&
			    !refinement_failure.empty())
			std::cerr << "EDGE_LOOP #" << id << ": STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " fragment refinement rejected: "
			    << refinement_failure << std::endl;
		} else if (closed_edge) {
		    /* The local cubic fitter extrapolates open-curve endpoint
		     * tangents.  On a closed 3-D edge whose UV endpoints lie on
		     * different sides of a periodic domain, that fit can overshoot
		     * the already validated pullback samples and reverse one endpoint
		     * tangent.  Preserve the adaptive, lift-bounded UV path directly. */
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample)
			points.Append(ON_3dPoint((*samples)[sample].x,
			    (*samples)[sample].y, 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(points);
		    if (polyline->ChangeDimension(2) && polyline->IsValid())
			c2d = polyline;
		    else
			delete polyline;
		} else {
		    ON_2dPointArray interpolation_samples(*samples);
		    c2d = interpolateCurve(interpolation_samples);
		}
		if (!c2d) {
		    ON_3dPointArray points;
		    points.Reserve(samples->Count());
		    for (int sample = 0; sample < samples->Count(); ++sample) {
			const ON_3dPoint point((*samples)[sample].x,
			    (*samples)[sample].y, 0.0);
			if (points.Count() > 0 && point.DistanceTo(
				points[points.Count() - 1]) <= ON_ZERO_TOLERANCE) {
			    points[points.Count() - 1] = point;
			    continue;
			}
			points.Append(point);
		    }
		    ON_PolylineCurve *polyline = points.Count() >= 2 ?
			new ON_PolylineCurve(points) : NULL;
		    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid()) {
			c2d = polyline;
			used_polyline_fallback = true;
		    } else {
			delete polyline;
		    }
		}
	    }
	    if (used_polyline_fallback && step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "preserved validated pullback samples after curve fitting failed");
	    }
	    if (!c2d && samples && samples->Count() < 2 && step &&
		step->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
		data->edge && data->curve) {
		/* The second use of a collapsed seam is the same exact STEP edge in
		 * reverse.  Reuse the first validated pcurve instead of asking a
		 * closest-point solver to distinguish endpoints whose separation is
		 * smaller than the model uncertainty. */
		for (int lti = 0; lti < loop->TrimCount() && !c2d; ++lti) {
		    const ON_BrepTrim *paired_trim = loop->Trim(lti);
		    if (!paired_trim || paired_trim->m_ei != data->edge->m_edge_index ||
			!paired_trim->TrimCurveOf())
			continue;
		    ON_Curve *candidate = paired_trim->DuplicateCurve();
		    if (!candidate)
			continue;
		    if (paired_trim->m_bRev3d != data->order_reversed &&
			!candidate->Reverse()) {
			delete candidate;
			continue;
		    }
		    const ON_Interval candidate_domain = candidate->Domain();
		    const ON_Interval edge_domain = data->curve->Domain();
		    bool candidate_valid = candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(data->order_reversed ?
				1.0 - fraction : fraction));
			if (!lifted.IsValid() || !target.IsValid() ||
				lifted.DistanceTo(target) > LocalUnits::tolerance)
			    candidate_valid = false;
		    }
		    if (!candidate_valid) {
			delete candidate;
			continue;
		    }
		    c2d = candidate;
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"reused an exact paired collapsed seam pcurve");
		}
	    }
	    if (!c2d && step && step->ImportOptions().repair ==
		    brlcad::step::RepairMode::Safe && nsamples &&
		    nsamples->Count() > 0 &&
		    data->curve && data->surf) {
		/* A collapsed edge may be the first member of its STEP loop.  In
		 * that case there is no previously emitted trim to seed the start,
		 * but its two declared vertices still provide authoritative 3-D
		 * endpoints for the bounded closest-point reconstruction below. */
		const ON_BrepTrim *previous_trim = loop->TrimCount() > 0 ?
		    loop->Trim(loop->TrimCount() - 1) : NULL;
		ON_3dPoint start = previous_trim ? previous_trim->PointAtEnd() :
		    ON_3dPoint::UnsetPoint;
		if (!previous_trim && samples && samples->Count() > 0)
		    start.Set((*samples)[0].x, (*samples)[0].y, 0.0);
		const ON_3dPoint adjacent_start = start;
		ON_3dPoint end((*nsamples)[0].x, (*nsamples)[0].y, 0.0);
		const ON_Interval edge_domain = data->curve->Domain();
		if (!data->context)
		    data->context = std::make_shared<brlcad::PullbackContext>();
		const double pullback_tolerance = short_curve_pullback_resolution(
		    data->curve, LocalUnits::tolerance);
		ON_3dPoint target_start = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 1 : 0]);
		ON_3dPoint target_end = data->curve->PointAt(edge_domain[
		    data->order_reversed ? 0 : 1]);
		/* Edge geometry can retain a wider proxy domain than a sub-tolerance
		 * topological use.  Its declared STEP vertices are authoritative for
		 * recovery of the lost pullback endpoint on every surface, not only a
		 * periodic one. */
		const int start_vertex = data->edge->m_vi[data->order_reversed ? 1 : 0];
		const int end_vertex = data->edge->m_vi[data->order_reversed ? 0 : 1];
		if (start_vertex >= 0 &&
			start_vertex < brep->m_V.Count())
		    target_start = brep->m_V[start_vertex].point;
		if (end_vertex >= 0 &&
			end_vertex < brep->m_V.Count())
		    target_end = brep->m_V[end_vertex].point;
		ON_2dPoint pulled_start, pulled_end;
		ON_3dPoint pulled_lift;
		double pulled_distance = DBL_MAX;
		/* Preserve an adjacent endpoint when its lift already represents the
		 * declared STEP vertex within model uncertainty.  This keeps the loop
		 * exactly closed in parameter space.  A periodic adjacent edge may begin
		 * at an arbitrary parameter on its 3-D circle, so solve the declared
		 * vertex when that bounded adjacency proof fails. */
		const ON_3dPoint adjacent_lift = data->surf->PointAt(
		    adjacent_start.x, adjacent_start.y);
		const bool adjacent_start_valid = previous_trim &&
		    adjacent_lift.IsValid() &&
		    adjacent_lift.DistanceTo(target_start) <= LocalUnits::tolerance;
		ON_3dPoint seeded_start = start;
		if (!adjacent_start_valid && refine_surface_point_seeded(data->surf,
			target_start, pullback_tolerance, seeded_start,
			&pulled_distance)) {
		    start = seeded_start;
		} else if (!adjacent_start_valid &&
			data->context->SurfaceClosestPoint(data->surf, target_start,
			pulled_start, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    start.Set(pulled_start.x, pulled_start.y, 0.0);
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    start[direction] += round((adjacent_start[direction] -
				start[direction]) / period) * period;
		    }
		}
		ON_3dPoint seeded_end = start;
		if (refine_surface_point_seeded(data->surf, target_end,
			pullback_tolerance, seeded_end, &pulled_distance)) {
		    end = seeded_end;
		} else if (data->context->SurfaceClosestPoint(data->surf, target_end,
			pulled_end, pulled_lift, pulled_distance, 0,
			pullback_tolerance, pullback_tolerance) &&
			pulled_distance <= pullback_tolerance) {
		    end.Set(pulled_end.x, pulled_end.y, 0.0);
		}

		/* Try each closed parameter direction as the constant seam coordinate.
		 * The one-sample pullback has lost its varying endpoint, so recover that
		 * endpoint from the exact 3-D edge and accept a candidate only after a
		 * dense lift comparison with the complete edge. */
		int boundary_direction = -1;
		ON_LineCurve *boundary = NULL;
		bool exact_boundary = false;
		double maximum_boundary_error = 0.0;
		double rejected_boundary_fraction = 0.0;
		const auto validate_boundary = [data, &edge_domain, &target_start,
			&target_end](
			ON_LineCurve *candidate, double *maximum_error,
			double *rejected_fraction) {
		    bool candidate_valid = candidate && candidate->ChangeDimension(2) &&
			candidate->IsValid();
		    const ON_Interval candidate_domain = candidate_valid ?
			candidate->Domain() : ON_Interval::EmptyInterval;
		    if (candidate_valid) {
			const ON_3dPoint start_uv = candidate->PointAt(candidate_domain.Min());
			const ON_3dPoint end_uv = candidate->PointAt(candidate_domain.Max());
			const ON_3dPoint start_lift = data->surf->PointAt(
			    start_uv.x, start_uv.y);
			const ON_3dPoint end_lift = data->surf->PointAt(end_uv.x, end_uv.y);
			/* pullback_tolerance is a deliberately tighter solver resolution
			 * used to distinguish the endpoints of a sub-tolerance feature.
			 * The source file's uncertainty remains the acceptance tolerance. */
			const double endpoint_tolerance = LocalUnits::tolerance;
			candidate_valid = start_lift.IsValid() && end_lift.IsValid() &&
			    start_lift.DistanceTo(target_start) <= endpoint_tolerance &&
			    end_lift.DistanceTo(target_end) <= endpoint_tolerance;
		    }
		    *maximum_error = 0.0;
		    *rejected_fraction = 0.0;
		    for (int sample = 0; candidate_valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    candidate_valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    candidate_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			const double edge_fraction = data->order_reversed ?
			    1.0 - fraction : fraction;
			const ON_3dPoint target = data->curve->PointAt(
			    edge_domain.ParameterAt(edge_fraction));
			const double error = lifted.IsValid() && target.IsValid() ?
			    lifted.DistanceTo(target) : DBL_MAX;
			*maximum_error = std::max(*maximum_error, error);
			if (error > LocalUnits::tolerance) {
			    *rejected_fraction = fraction;
			    candidate_valid = false;
			}
		    }
		    return candidate_valid;
		};
		/* First try the direct endpoint association on every surface.  A
		 * sub-tolerance edge on a periodic surface is not necessarily a seam;
		 * it may vary in the closed direction while remaining an ordinary
		 * isoparametric trim.  Dense lift validation below is authoritative. */
		{
		    ON_LineCurve *candidate = new ON_LineCurve(start, end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    if (validate_boundary(candidate, &candidate_maximum_error,
			    &candidate_rejected_fraction)) {
			boundary = candidate;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = candidate_maximum_error;
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		for (int fixed_direction = 0; fixed_direction < 2 && !exact_boundary;
		     ++fixed_direction) {
		    if (!data->surf->IsClosed(fixed_direction))
			continue;
		    ON_3dPoint candidate_end = end;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!data->surf->IsClosed(direction))
			    continue;
			const double period = data->surf->Domain(direction).Length();
			if (period > ON_ZERO_TOLERANCE)
			    candidate_end[direction] += round((start[direction] -
				candidate_end[direction]) / period) * period;
		    }
		    candidate_end[fixed_direction] = start[fixed_direction];
		    ON_LineCurve *candidate = new ON_LineCurve(start, candidate_end);
		    double candidate_maximum_error = 0.0;
		    double candidate_rejected_fraction = 0.0;
		    const bool candidate_valid = validate_boundary(candidate,
			&candidate_maximum_error, &candidate_rejected_fraction);
		    if (candidate_valid) {
			boundary_direction = fixed_direction;
			boundary = candidate;
			end = candidate_end;
			exact_boundary = true;
			maximum_boundary_error = candidate_maximum_error;
		    } else {
			maximum_boundary_error = std::max(maximum_boundary_error,
			    candidate_maximum_error);
			rejected_boundary_fraction = candidate_rejected_fraction;
			delete candidate;
		    }
		}
		if (exact_boundary) {
		    const ON_2dPoint original_next_start = (*nsamples)[0];
		    (*nsamples)[0].Set(end.x, end.y);
		    bool next_curve_valid = true;
		    if (next_cs == curve_pullback_samples.begin()) {
			const bool can_rebuild_next = initial_trim_count == 0 &&
			    loop->TrimCount() > 0 &&
			    ndata && ndata->curve && ndata->surf;
			ON_2dPointArray next_interpolation(*nsamples);
			ON_Curve *next_curve = can_rebuild_next ?
			    interpolateCurve(next_interpolation) : NULL;
			const auto make_next_polyline = [nsamples]() -> ON_Curve * {
			    ON_3dPointArray points;
			    for (int point = 0; point < nsamples->Count(); ++point) {
				const ON_3dPoint candidate((*nsamples)[point].x,
				    (*nsamples)[point].y, 0.0);
				if (points.Count() == 0 || candidate.DistanceTo(
					points[points.Count() - 1]) > ON_ZERO_TOLERANCE)
				    points.Append(candidate);
			    }
			    ON_PolylineCurve *polyline = points.Count() >= 2 ?
				new ON_PolylineCurve(points) : NULL;
			    if (polyline && polyline->ChangeDimension(2) && polyline->IsValid())
				return polyline;
			    delete polyline;
			    return NULL;
			};
			const auto validates_next = [ndata](ON_Curve *candidate) {
			    if (!candidate || !candidate->ChangeDimension(2) ||
				    !candidate->IsValid())
				return false;
			    const ON_Interval candidate_domain = candidate->Domain();
			    const ON_Interval next_edge_domain = ndata->curve->Domain();
			    for (int sample = 0;
				    sample <= kDenseLiftValidationSegments; ++sample) {
				if (brlcad::PullbackWorkCancelled()) return false;
				const double fraction = static_cast<double>(sample) /
				    kDenseLiftValidationSegments;
				const ON_3dPoint uv = candidate->PointAt(
				    candidate_domain.ParameterAt(fraction));
				const ON_3dPoint lifted = ndata->surf->PointAt(uv.x, uv.y);
				const ON_3dPoint target = ndata->curve->PointAt(
				    next_edge_domain.ParameterAt(ndata->order_reversed ?
					1.0 - fraction : fraction));
				if (!lifted.IsValid() || lifted.DistanceTo(target) >
					LocalUnits::tolerance)
				    return false;
			    }
			    return true;
			};
			next_curve_valid = can_rebuild_next && validates_next(next_curve);
			if (!next_curve_valid && can_rebuild_next) {
			    delete next_curve;
			    next_curve = make_next_polyline();
			    next_curve_valid = validates_next(next_curve);
			}
			if (next_curve_valid) {
			    ON_BrepTrim *first_trim = loop->Trim(0);
			    const int next_c2 = brep->AddTrimCurve(next_curve);
			    if (!first_trim || next_c2 < 0 ||
				    !brep->SetTrimCurve(*first_trim, next_c2)) {
				if (next_c2 < 0)
				    delete next_curve;
				next_curve_valid = false;
			    } else {
				brep->SetTrimIsoFlags(*first_trim);
			    }
			} else {
			    delete next_curve;
			}
		    }
		    if (next_curve_valid) {
			c2d = boundary;
			const bool periodic_surface = data->surf->IsClosed(0) ||
			    data->surf->IsClosed(1);
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    periodic_surface ?
			    "regenerated a collapsed seam from exact adjacent endpoints" :
			    "regenerated a collapsed boundary pcurve within model tolerance");
		    } else {
			(*nsamples)[0] = original_next_start;
			delete boundary;
		    }
		} else {
		    if (step->Verbose())
			std::cerr << "EDGE_LOOP #" << id
			    << ": collapsed pcurve proof rejected for STEP edge "
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " boundary direction " << boundary_direction
			    << " uv " << start.x << ':' << start.y << " -> "
			    << end.x << ':' << end.y << " at normalized "
			    << rejected_boundary_fraction << " max error "
			    << maximum_boundary_error << " tolerance "
			    << LocalUnits::tolerance << " target length "
			    << target_start.DistanceTo(target_end) << " endpoint errors "
			    << data->surf->PointAt(start.x, start.y).DistanceTo(target_start)
			    << '/' << data->surf->PointAt(end.x, end.y).DistanceTo(target_end)
			    << " surface closed "
			    << (data->surf->IsClosed(0) ? '1' : '0')
			    << (data->surf->IsClosed(1) ? '1' : '0') << " domains "
			    << data->surf->Domain(0).Min() << ':'
			    << data->surf->Domain(0).Max() << ','
			    << data->surf->Domain(1).Min() << ':'
			    << data->surf->Domain(1).Max() << std::endl;
		    delete boundary;
		}
	    }
	    /* Absent/collapsed-pcurve recovery above may construct an exact curve
	     * from adjacent endpoints even when the seam splitter supplied zero or
	     * one samples.  Reconstitute those endpoint samples for the subsequent
	     * loop-closure and singular-bridge checks. */
	    if (c2d && samples && samples->Count() < 2) {
		const ON_Interval recovered_domain = c2d->Domain();
		const ON_3dPoint recovered_start = c2d->PointAt(recovered_domain.Min());
		const ON_3dPoint recovered_end = c2d->PointAt(recovered_domain.Max());
		samples->Empty();
		samples->Append(ON_2dPoint(recovered_start.x, recovered_start.y));
		samples->Append(ON_2dPoint(recovered_end.x, recovered_end.y));
	    }
	    if (c2d && data->tolerance_adjusted) {
		double maximum_lift_error = 0.0;
		double maximum_error_fraction = 0.0;
		ON_3dPoint maximum_error_uv = ON_3dPoint::UnsetPoint;
		const double adjustment_limit = maximum_verified_edge_tolerance(
		    data->curve, data->declared_tolerance, item_scale);
		const auto validate_locus = [&](ON_Curve *candidate) {
		    maximum_lift_error = 0.0;
		    maximum_error_fraction = 0.0;
		    maximum_error_uv = ON_3dPoint::UnsetPoint;
		    if (!candidate) return false;
		    const ON_Interval pcurve_domain = candidate->Domain();
		    CurveDistanceEvaluator edge_distance(data->curve);
		    bool valid = pcurve_domain.IsIncreasing() && edge_distance.IsValid();
		    for (int sample = 0; valid &&
			    sample <= kDenseLiftValidationSegments; ++sample) {
			if (brlcad::PullbackWorkCancelled()) {
			    valid = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) /
			    kDenseLiftValidationSegments;
			const ON_3dPoint uv = candidate->PointAt(
			    pcurve_domain.ParameterAt(fraction));
			const ON_3dPoint lifted = data->surf->PointAt(uv.x, uv.y);
			/* A UV curve has its own parameterization.  Validate its 3-D
			 * geometric locus against the closest exact source-curve point. */
			const double error = lifted.IsValid() ?
			    edge_distance.DistanceTo(lifted, data->tolerance) : DBL_MAX;
			if (error > maximum_lift_error) {
			    maximum_lift_error = error;
			    maximum_error_fraction = fraction;
			    maximum_error_uv = uv;
			}
		    }
		    if (valid && maximum_lift_error > data->tolerance) {
			const double dense_tolerance = maximum_lift_error *
			    kMeasuredToleranceSafetyFactor;
			if (dense_tolerance <= adjustment_limit)
			    data->tolerance = dense_tolerance;
			else
			    valid = false;
		    }
		    return valid;
		};

		bool lift_valid = validate_locus(c2d);
		if (!lift_valid && !brlcad::PullbackWorkCancelled() &&
			used_polyline_fallback && samples && samples->Count() >= 2) {
		    ON_Curve *refined = NULL;
		    double measured_tolerance = 0.0;
		    if (refine_adjusted_pullback_polyline(data, *samples,
			    maximum_error_uv, adjustment_limit, &refined,
			    &measured_tolerance)) {
			delete c2d;
			c2d = refined;
			data->tolerance = std::max(data->tolerance,
			    measured_tolerance);
			lift_valid = validate_locus(c2d);
			if (lift_valid && step)
			    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
				"adaptively refined a closed-surface pcurve after dense locus validation");
		    }
		}
		if (!lift_valid) {
		    const bool cancelled = brlcad::PullbackWorkCancelled();
		    if (step && step->Verbose()) {
			std::cerr << "EDGE_LOOP #" << id << ": adjusted pcurve for STEP edge #"
			    << (data->edge ? data->edge->m_edge_user.i : -1)
			    << " failed dense locus validation at " << maximum_error_fraction
			    << " uv=" << maximum_error_uv.x << ':' << maximum_error_uv.y
			    << " error=" << maximum_lift_error << " limit="
			    << adjustment_limit << " segments=" << data->segments->size()
			    << " samples=" << (samples ? samples->Count() : 0);
			if (samples && samples->Count() >= 2) {
			const int last = samples->Count() - 1;
			std::cerr << " sample-uvs=" << (*samples)[0].x << ':'
			    << (*samples)[0].y << ',' << (*samples)[1].x << ':'
			    << (*samples)[1].y;
			if (last > 1)
			    std::cerr << "," << (*samples)[last - 1].x << ':'
				<< (*samples)[last - 1].y;
			std::cerr << ',' << (*samples)[last].x << ':'
			    << (*samples)[last].y;
			}
			std::cerr << std::endl;
		    }
		    delete c2d;
		    c2d = NULL;
		    data->failure_reason = cancelled ? PullbackFailureReason::Cancelled :
			PullbackFailureReason::SurfaceDistanceExceeded;
		    if (!cancelled)
			data->maximum_projection_distance = maximum_lift_error;
		} else if (step) {
		    const int edge_id = data->edge ? data->edge->m_edge_user.i : -1;
		    std::ostringstream warning;
		    warning << "source edge geometry separation exceeds declared tolerance "
			<< data->declared_tolerance << "; using densely verified edge tolerance "
			<< data->tolerance << " for STEP edge #" << edge_id;
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
			id, "EDGE_LOOP", "edge_list", warning.str());
		    step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			"adjusted one OpenNURBS edge tolerance to measured source geometry");
		}
	    }
	    if (!c2d) {
		if (step) {
		    const int edge_id = data->edge ? data->edge->m_edge_user.i : -1;
		    std::ostringstream reason;
		    if (data->failure_reason == PullbackFailureReason::Cancelled) {
			reason << "trim validation was cancelled for STEP edge #"
			    << edge_id << " after its per-item work budget expired";
		    } else if (data->failure_reason == PullbackFailureReason::SurfaceDistanceExceeded) {
			if (data->rejected_projection_samples == 0) {
			    reason << "constructed pcurve lift deviated from STEP edge #"
				<< edge_id << " by "
				<< data->maximum_projection_distance
				<< " after dense curve-locus validation";
			} else {
			    reason << "source curve/surface separation "
				<< data->maximum_projection_distance
				<< " exceeds model tolerance " << LocalUnits::tolerance
				<< " for STEP edge #" << edge_id << " ("
				<< data->rejected_projection_samples << '/'
				<< data->projection_samples
				<< " projection samples rejected)";
			}
		    } else {
			reason << "could not construct an exact trim for STEP edge #"
			    << edge_id << " from "
			    << (samples ? samples->Count() : 0)
			    << " validated pullback samples ("
			    << data->failed_projection_samples
			    << " closest-point solver failures)";
		    }
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error, id,
			"EDGE_LOOP", "edge_list", reason.str());
		}
		if (step && step->Verbose())
		    std::cerr << "EDGE_LOOP #" << id
			<< ": could not construct trim for STEP edge "
			<< (data->edge ? data->edge->m_edge_user.i : -1)
			<< " from " << samples->Count() << " pullback samples"
			<< std::endl;
		trim_construction_failed = true;
		break;
	    }
	    trimCurve = brep->m_C2.Count();
	    brep->m_C2.Append(c2d);

	    ON_BrepEdge *trim_edge = const_cast<ON_BrepEdge *>(data->edge);
	    bool trim_reversed = data->order_reversed;
	    std::string split_failure;
	    if (split_at_singularity &&
		    !split_pullback_segment_edge(brep, data, c2d, *samples,
			data->tolerance, &trim_edge, &trim_reversed,
			&split_failure)) {
		if (step)
		    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
			id, "EDGE_LOOP", "edge_list",
			"could not split STEP edge #" +
			std::to_string(data->edge ? data->edge->m_edge_user.i : -1) +
			" into exact continuous pcurve fragments" +
			(split_failure.empty() ? std::string() :
			 ": " + split_failure));
		trim_construction_failed = true;
		break;
	    }
	    if (split_at_singularity && step)
		step->RecordRepair(id, "EDGE_LOOP", "edge_list",
		    "split an exact STEP edge at a surface parameter singularity");
	    trim_edge->m_tolerance = std::max(trim_edge->m_tolerance, data->tolerance);
	    ON_BrepTrim &trim = brep->NewTrim(*trim_edge, trim_reversed,
		(ON_BrepLoop &) * loop, trimCurve);
	    trim.m_tolerance[0] = data->tolerance;
	    trim.m_tolerance[1] = data->tolerance;
	    ON_Interval PD = trim.ProxyCurveDomain();
	    trim.m_iso = surface->IsIsoparametric(*c2d, &PD);

	    // check for bridging trim, trims along singularities
	    // are implicitly expected
	    ON_2dPoint end_current, start_next;
	    end_current = (*samples)[samples->Count() - 1];
	    /* The following seam fragment may be the absent pcurve that will be
	     * regenerated on its own iteration.  Defer this bridge decision until
	     * it has exact endpoints instead of indexing an empty sample array. */
	    start_next = (nsamples && nsamples->Count() > 0) ?
		(*nsamples)[0] : end_current;

	    if (end_current.DistanceTo(start_next) > LocalUnits::tolerance) {
		// endpoints don't connect
		int is;
		const ON_Surface *surf = data->surf;
		const bool selected_pole_cut = data->periodic_pole_cut_after &&
		    ndata && ndata->periodic_pole_cut_before;
		if (selected_pole_cut && insert_periodic_pole_cut(brep, *loop, surf, trim,
			end_current, start_next, LocalUnits::tolerance)) {
		    if (step)
			step->RecordRepair(id, "EDGE_LOOP", "edge_list",
			    "inserted an exact paired seam cut to a surface pole");
		} else if ((is = check_pullback_singularity_bridge(surf, end_current, start_next)) >= 0) {
		    // insert trim
		    // insert singular trim along
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    ON_Surface::ISO iso = ON_Surface::N_iso;
		    switch (is) {
			case 0:
			    //south
			    iso = ON_Surface::S_iso;
			    break;
			case 1:
			    //east
			    iso = ON_Surface::E_iso;
			    break;
			case 2:
			    //north
			    iso = ON_Surface::N_iso;
			    break;
			case 3:
			    //west
			    iso = ON_Surface::W_iso;
		    }

		    ON_Curve *sing_c2d = new ON_LineCurve(end_current, start_next);
		    trimCurve = brep->m_C2.Count();
		    brep->m_C2.Append(sing_c2d);

		    const int vi = trim.m_vi[1];

		    ON_BrepTrim &sing_trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop &) * loop, iso, trimCurve);

		    sing_trim.m_tolerance[0] = LocalUnits::tolerance;
		    sing_trim.m_tolerance[1] = LocalUnits::tolerance;
		    ON_Interval sing_PD = sing_trim.ProxyCurveDomain();
		    sing_trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &sing_PD);
		    sing_trim.m_iso = iso;
		    //trim.Reverse();
		} /*else if ((is = check_pullback_seam_bridge(surf, end_current, start_next)) >= 0) {
		  // insert trim
		  // insert singular trim along
		  // 0 = south, 1 = east, 2 = north, 3 = west
		  ON_Surface::ISO iso;
		  switch (is) {
		  case 0:
		  //south
		  iso = ON_Surface::S_iso;
		  break;
		  case 1:
		  //east
		  iso = ON_Surface::E_iso;
		  break;
		  case 2:
		  //north
		  iso = ON_Surface::N_iso;
		  break;
		  case 3:
		  //west
		  iso = ON_Surface::W_iso;
		  }

		  ON_Curve* c2d = new ON_LineCurve(end_current, start_next);
		  trimCurve = brep->m_C2.Count();
		  brep->m_C2.Append(c2d);

		  int vi;
		  int vo;
		  if (data->order_reversed) {
		  vi = data->edge->m_vi[0];
		  vo = data->edge->m_vi[1];
		  } else {
		  vi = data->edge->m_vi[1];
		  vo = data->edge->m_vi[0];
		  }
		  #ifdef TREATASBOUNDARY
		  ON_BrepEdge& e = (ON_BrepEdge&)*data->edge;
		  ON_BrepTrim& trim = brep->NewTrim(e, false, (ON_BrepLoop&) *loop, trimCurve);

		  trim.m_type = ON_BrepTrim::boundary;
		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  #else
		  ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop&) *loop, iso, trimCurve);

		  trim.m_tolerance[0] = LocalUnits::tolerance;
		  trim.m_tolerance[1] = LocalUnits::tolerance;
		  ON_Interval PD = trim.ProxyCurveDomain();
		  trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
		  trim.m_iso = iso;
		  #endif
		  trim.IsValid(&tl);
		  }*/
	    }
	    si++;
	}
    if (trim_construction_failed)
	    break;
	cs++;
    }

    if (step && step->Verbose() && loop->TrimCount() > 1) {
	for (int trim_offset = 0; trim_offset < loop->TrimCount(); ++trim_offset) {
	    const ON_BrepTrim *current_trim = loop->Trim(trim_offset);
	    const ON_BrepTrim *next_trim = loop->Trim((trim_offset + 1) % loop->TrimCount());
	    if (!current_trim || !next_trim)
		continue;
	    const ON_3dPoint current_end = current_trim->PointAtEnd();
	    const ON_3dPoint next_start = next_trim->PointAtStart();
	    if (current_end.DistanceTo(next_start) <= ON_ZERO_TOLERANCE)
		continue;
	    std::cerr << "EDGE_LOOP #" << id << ": trim " << current_trim->m_trim_index
		<< " (edge " << current_trim->m_ei << ", type "
		<< static_cast<int>(current_trim->m_type) << ", iso "
		<< static_cast<int>(current_trim->m_iso) << ") ends at ("
		<< current_end.x << ',' << current_end.y << ") but trim "
		<< next_trim->m_trim_index << " (edge " << next_trim->m_ei
		<< ", type " << static_cast<int>(next_trim->m_type) << ", iso "
		<< static_cast<int>(next_trim->m_iso) << ") starts at ("
		<< next_start.x << ',' << next_start.y << ')' << std::endl;
	}
    }

    while (!curve_pullback_samples.empty()) {
	data = curve_pullback_samples.front();
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
	delete data;
	curve_pullback_samples.pop_front();
    }
    for (std::map<PBCData *, ON_Curve *>::iterator exact = exact_pullbacks.begin();
	 exact != exact_pullbacks.end(); ++exact)
	delete exact->second;

    if (loop->TrimCount() == initial_trim_count && step && step->Verbose())
	std::cerr << "EDGE_LOOP #" << id << ": trim construction produced no edge uses" << std::endl;

    return !trim_construction_failed;
}


bool
Plane::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	ON_PlaneSurface *s = dynamic_cast<ON_PlaneSurface *>(brep->m_S[ON_id]);

	if (s && trim_curve_3d_bbox) {
	    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();

	    // origin may not lie within face so include in extent
	    double maxdist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_max);
	    double mindist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_min);
	    bbdiag += FMAX(maxdist, mindist);

	    ON_Interval extents(-bbdiag, bbdiag);
	    s->Extend(0,extents);
	    s->Extend(1,extents);
	}

	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    ON_PlaneSurface *s = new ON_PlaneSurface(p);

    if (!trim_curve_3d_bbox) {
	delete s;
	return false;
    }
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    // origin may not lie within face so include in extent
    double maxdist = origin.DistanceTo(trim_curve_3d_bbox->m_max);
    double mindist = origin.DistanceTo(trim_curve_3d_bbox->m_min);
    bbdiag += FMAX(maxdist, mindist);

    //TODO: look into line curves that are just point and direction
    ON_Interval extents(-bbdiag, bbdiag);
    s->SetExtents(0, extents);
    s->SetExtents(1, extents);
    s->SetDomain(0, 0.0, 1.0);
    s->SetDomain(1, 0.0, 1.0);

    ON_id = brep->AddSurface(s);

    return true;
}


bool
CylindricalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // new surface if reused because of bounding
    //if (ON_id >= 0)
    //	return true; // already loaded

    if ((false) && (brep->m_S.Count() == 56)) {
	std::cerr << "LoadONBrep for CylindricalSurface: " << 55 << std::endl;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // make sure origin is part of the bbox
    trim_curve_3d_bbox->Set(origin, true);

    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    origin = origin - bbdiag * norm;
    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    //ON_Cylinder cyl(c, ON_DBL_MAX);
    ON_Cylinder cyl(c, 2.0 * bbdiag);

    ON_RevSurface *s = cyl.RevSurfaceForm();
    if (s) {
	double r = fabs(cyl.circle.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	s->SetDomain(0, 0.0, 2.0 * ON_PI * r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ConicalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    if (!xaxis.Unitize() || !norm.Unitize() || !trim_curve_3d_bbox ||
	    !trim_curve_3d_bbox->IsValid())
	return false;

    const double angle = semi_angle * LocalUnits::planeangle;
    const double tan_semi_angle = tan(angle);
    const double local_radius = radius * LocalUnits::length;

    if (step && step->Verbose())
	std::cerr << "CONICAL_SURFACE #" << id << ": radius=" << radius
	    << " length-factor=" << LocalUnits::length << " semi-angle="
	    << semi_angle << " angle-factor=" << LocalUnits::planeangle
	    << " radians=" << angle << std::endl;

    /* STEP's conical_surface parameter extends through the apex onto the
     * opposite nappe.  ON_Cone::RevSurfaceForm() only constructs one finite
     * nappe, which makes valid trims beyond the apex impossible to project.
     * Revolving the complete generator line reproduces the STEP locus
     * exactly.  The trim box only bounds the evaluation domain; it does not
     * alter the cone angle, radius, or imported topology. */
    ON_3dPoint profile_origin = origin;
    const bool cylindrical_limit = fabs(tan_semi_angle) <= ON_SQRT_EPSILON;
    if (!cylindrical_limit)
	profile_origin = origin - (local_radius / tan_semi_angle) * norm;

    const double dx = std::max(fabs(trim_curve_3d_bbox->m_min.x - profile_origin.x),
	    fabs(trim_curve_3d_bbox->m_max.x - profile_origin.x));
    const double dy = std::max(fabs(trim_curve_3d_bbox->m_min.y - profile_origin.y),
	    fabs(trim_curve_3d_bbox->m_max.y - profile_origin.y));
    const double dz = std::max(fabs(trim_curve_3d_bbox->m_min.z - profile_origin.z),
	    fabs(trim_curve_3d_bbox->m_max.z - profile_origin.z));
    double reach = sqrt(dx * dx + dy * dy + dz * dz);
    reach = std::max(reach, fabs(local_radius));
    if (!(reach > ON_SQRT_EPSILON) || !ON_IsValid(reach))
	return false;
    reach *= 1.01; /* Domain margin only; the analytic surface is unchanged. */

    ON_3dPoint profile_start;
    ON_3dPoint profile_end;
    if (cylindrical_limit) {
	profile_start = profile_origin - reach * norm + local_radius * xaxis;
	profile_end = profile_origin + reach * norm + local_radius * xaxis;
    } else {
	profile_start = profile_origin - reach * norm -
	    (reach * tan_semi_angle) * xaxis;
	profile_end = profile_origin + reach * norm +
	    (reach * tan_semi_angle) * xaxis;
    }

    ON_LineCurve *profile = new ON_LineCurve(profile_start, profile_end);
    profile->SetDomain(-reach, reach);
    ON_RevSurface *surface = ON_RevSurface::New();
    surface->m_curve = profile;
    surface->m_axis = ON_Line(profile_origin, profile_origin + norm);
    surface->SetAngleRadians(0.0, ON_2PI);
    surface->m_t.Set(0.0, ON_2PI);
    surface->m_bTransposed = false;
    surface->BoundingBox();
    if (!surface->IsValid()) {
	delete surface;
	return false;
    }

    ON_id = brep->AddSurface(surface);
    if (ON_id < 0) {
	delete surface;
	return false;
    }

    return true;
}


int
intersectLines(const ON_Line &l1, const ON_Line &l2, ON_3dPoint &out)
{
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    point_t l1_from, l2_from;
    VMOVE(l1_from, l1.from);
    VMOVE(l2_from, l2.from);

    ON_3dVector d;
    vect_t l1_dir, l2_dir;

    d = l1.Direction();
    VMOVE(l1_dir, d);
    d = l2.Direction();
    VMOVE(l2_dir, d);

    fastf_t l1_dist, l2_dist;
    int i = bg_isect_line3_line3(&l1_dist, &l2_dist, l1_from, l1_dir,
				 l2_from, l2_dir, &tol);
    if (i == 1) {
	ON_3dVector l1_unit_dir = l1.Direction();
	l1_unit_dir.Unitize();

	out = l1.from + l1_unit_dir * l1_dist;
    }
    return i;
}


void
Circle::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    ON_3dPoint P = c.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;


    P = c.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

#define ANGLE_ZERO_TOL 1.0e-6

static double
simplify_angle(double rad)
{
    double result;

    result = fmod(rad, 2.0 * ON_PI);

    if (NEAR_ZERO(result, ANGLE_ZERO_TOL)) {
	result = 0.0;
    } else if (result < 0.0) {
	result += 2.0 * ON_PI;
    }

    return result;
}

static double
radians_from_xaxis_to_ellipse_point(Conic *conic, ON_3dPoint p, double a = 1.0, double b = 1.0)
{
    ON_3dPoint origin(conic->GetOrigin());
    ON_3dVector xaxis(conic->GetXAxis());
    ON_3dVector yaxis(conic->GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // get p after translating to origin
    ON_3dPoint canonical_p = p - origin;

    // decompose into x and y components
    double x = canonical_p * xaxis;
    double y = canonical_p * yaxis;

    // ellipse scaling
    x /= a;
    y /= b;

    return atan2(y, x);
}

bool
Circle::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    if ((false) && ((brep->m_C3.Count() == 3) || (id == 1723))) {
	std::cerr << "Debug:LoadONBrep for Circle:ID:" << id << std::endl;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double r = radius * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle circle(plane, origin, r);

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt);
	s = radians_from_xaxis_to_ellipse_point(this, endpt);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = circle.PointAt(t);
	endpt = circle.PointAt(s);
    }

    double theta = s - t;

    if (VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST)) {
	theta = 2.0 * ON_PI;
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }

    double dtheta = theta / narcs;
    double w = cos(dtheta / 2.0);
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; /* 2 * max narcs + 1 */
    ON_3dPoint circleP1, isect, circleP2;
    ON_3dVector tangentP1, tangentP2;

    circleP1 = circle.PointAt(angle); // was using 'startpt' from edge_curve but found case where not in tol
    tangentP1 = circle.TangentAt(angle);

    for (int i = 0; i < narcs; i++) {
	angle = angle + dtheta;

	circleP2 = circle.PointAt(angle);
	tangentP2 = circle.TangentAt(angle);
	ON_Line tangent1(circleP1, circleP1 + r * tangentP1);
	ON_Line tangent2(circleP2, circleP2 + r * tangentP2);

	if (intersectLines(tangent1, tangent2, isect) != 1) {
	    if (step && step->Verbose())
		std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}

	cpts.Append(circleP1);

	isect *= w; // must pre-weight before putting into NURB
	cpts.Append(isect);

	W[2 * i] = 1.0;
	W[2 * i + 1] = w;

	circleP1 = circleP2;
	tangentP1 = tangentP2;
    }
    cpts.Append(circle.PointAt(s));
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Ellipse::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    ON_Ellipse ellipse(plane, a, b);

    ON_3dPoint P = ellipse.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    P = ellipse.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

bool
Ellipse::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane plane(origin, xaxis, yaxis);

    ON_3dPoint center = origin;
    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Ellipse ellipse(plane, a, b);

    // double eccentricity = sqrt(1.0 - (b * b) / (a * a));
    // ON_3dPoint focus_1 = center + (eccentricity * a) * xaxis;
    // ON_3dPoint focus_2 = center - (eccentricity * a) * xaxis;

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt, a, b);
	s = radians_from_xaxis_to_ellipse_point(this, endpt, a, b);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = ellipse.PointAt(t);
	endpt = ellipse.PointAt(s);
    }

    double theta = s - t;

    if (VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST)) {
	theta = 2.0 * ON_PI;
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }
    double dtheta = theta / narcs;
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; // 2 * max narcs + 1
    ON_3dPoint Pnt[2 * 4 + 1];
    ON_3dVector Tangent1, Tangent2;
    ON_3dPoint P0, PX, P2, P1;

    P0 = ellipse.PointAt(angle);
    for (int i = 0; i < narcs; i++) {
	Tangent1 = ellipse.TangentAt(angle);
	PX = ellipse.PointAt(angle + dtheta / 2.0);
	// step angle
	angle = angle + dtheta;
	P2 = ellipse.PointAt(angle);
	Tangent2 = ellipse.TangentAt(angle);
	ON_Line tangent1(P0, P0 + Tangent1);
	ON_Line tangent2(P2, P2 + Tangent2);
	if (intersectLines(tangent1, tangent2, P1) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	ON_Line l1(P1, center);
	ON_Line l2(P0, P2);
	ON_3dPoint PM;
	if (intersectLines(l1, l2, PM) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	double mx = PM.DistanceTo(PX);
	double mp1 = PM.DistanceTo(P1);
	double R = mx / mp1;
	double w = R / (1 - R);

	cpts.Append(P0);
	Pnt[2 * i] = P0;
	W[2 * i] = 1.0;

	Pnt[2 * i + 1] = P1;
	P1 = (w) * P1; // must pre-weight before putting into NURB
	cpts.Append(P1);
	W[2 * i + 1] = w;

	P0 = P2;
    }
    cpts.Append(P2);
    Pnt[2 * narcs] = P2;
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Hyperbola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    ON_3dPoint center = origin;
    double a = semi_axis;
    double b = semi_imag_axis;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }

    double y = b * tan(t);
    double x = a / cos(t);

    ON_3dVector X = x * xaxis;
    ON_3dVector Y = y * yaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    y = b * tan(s);
    x = a / cos(s);

    X = x * xaxis;
    Y = y * yaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Hyperbola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    norm.Unitize();
    xaxis.Unitize();
    yaxis.Unitize();

    ON_3dPoint center = origin * LocalUnits::length;
    double a = semi_axis * LocalUnits::length;
    double b = semi_imag_axis * LocalUnits::length;

    double eccentricity = sqrt(1.0 + (b * b) / (a * a));
    ON_3dPoint focus = center + (eccentricity * a) * xaxis;
    ON_3dPoint focusprime = center - (eccentricity * a) * xaxis;
    // ON_3dPoint vertex = center + a * xaxis;
    // ON_3dPoint directrix = center + (a / eccentricity) * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) {
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 =  pnt2 * LocalUnits::length;

    // calc tangent P0, P2 intersection
    ON_3dVector ToFocus = focus - P0;
    ToFocus.Unitize();

    ON_3dVector ToFocusPrime = focusprime - P0;
    ToFocusPrime.Unitize();

    ON_3dVector bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs0(P0, P0 + bisector);

    ToFocus = focus - P2;
    ToFocus.Unitize();

    ToFocusPrime = focusprime - P2;
    ToFocusPrime.Unitize();

    bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs2(P2, P2 + bisector);
    ON_3dPoint P1;
    if (intersectLines(bs0, bs2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    ON_Line l1(focus, P1);
    ON_Line l2(P0, P2);
    ON_3dPoint M = (P0 + P2) / 2.0;

    ON_Line ctom(center, M);
    ON_3dVector dtom = ctom.Direction();
    dtom.Unitize();
    double m = (dtom * yaxis) / (dtom * xaxis);
    double x1 = a * b * sqrt(1.0 / (b * b - m * m * a * a));
    double y1 = b * sqrt((x1 * x1) / (a * a) - 1.0);
    if (m < 0.0) {
	y1 *= -1.0;
    }
    ON_3dVector X = x1 * xaxis;
    ON_3dVector Y = y1 * yaxis;
    ON_3dPoint Pv = center + X + Y;
    double mx = M.DistanceTo(Pv);
    double mp1 = M.DistanceTo(P1);
    double R = mx / mp1;
    double w = R / (1 - R);

    P1 = (w) * P1; // must pre-weight before putting into NURB

    // add hyperbola weightings
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve *hypernurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*hypernurbscurve);

    ON_id = brep->AddEdgeCurve(hypernurbscurve);

    return true;
}


void
Parabola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    xaxis.Unitize();
    ON_3dVector yaxis(GetYAxis());
    yaxis.Unitize();

    ON_3dPoint center = origin;
    double fd = focal_dist;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    double x = 2.0 * fd * t; // tan(t);
    double y = (x * x) / (4 * fd);

    ON_3dVector X = x * yaxis;
    ON_3dVector Y = y * xaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    x = 2.0 * fd * s; //tan(s);
    y = (x * x) / (4 * fd);

    X = x * yaxis;
    Y = y * xaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Parabola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    // ON_3dVector yaxis(GetYAxis());
    // ON_3dVector normal(GetNormal());

    ON_3dPoint center = origin * LocalUnits::length;
    double fd = focal_dist * LocalUnits::length;
    ON_3dPoint focus = center + fd * xaxis;
    // ON_3dPoint directrix = center - fd * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 = pnt2 * LocalUnits::length;

    // calc tang intersect with transverse axis
    ON_3dVector ToFocus;

    ToFocus = P0 - focus;
    ToFocus.Unitize();
    ON_3dVector bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent1(P0, P0 + 10.0 * bisector);

    ToFocus = P2 - focus;
    ToFocus.Unitize();
    bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent2(P2, P2 + 10.0 * bisector);

    ON_3dPoint P1;
    if (intersectLines(tangent1, tangent2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    // make parabola from bezier
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();

    ON_NurbsCurve *parabnurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*parabnurbscurve);
    ON_id = brep->AddEdgeCurve(parabnurbscurve);

    return true;
}


void
Line::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint ptstart(pnt->Point3d());
    ON_3dVector vdir( dir->Orientation());
    ON_3dPoint ptend = ptstart + (vdir*dir->Magnitude());
    ON_Line l(ptstart, ptend);

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    VMOVE(startpoint,l.PointAt(t));
    VMOVE(endpoint,l.PointAt(s));

    SetPointTrim(startpoint, endpoint);
}


bool
Line::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint startpnt = ON_3dPoint::UnsetPoint;
    ON_3dPoint endpnt = ON_3dPoint::UnsetPoint;

    if (trimmed) { //explicitly trimmed
	startpnt = trim_startpoint;
	endpnt = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	startpnt = start->Point3d();
	endpnt = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    startpnt = startpnt * LocalUnits::length;
    endpnt = endpnt * LocalUnits::length;

    ON_LineCurve *l = new ON_LineCurve(startpnt, endpnt);

    ON_id = brep->AddEdgeCurve(l);

    return true;
}


bool
SurfaceOfLinearExtrusion::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_Curve *curve = brep->m_C3[swept_curve->GetONId()];

    // use trimming edge bounding box unioned with bounding box of
    // curve being extruded; calc diagonal to make sure extrusion
    // magnitude is well represented
    ON_BoundingBox curvebb = curve->BoundingBox();
    trim_curve_3d_bbox->Union(curvebb);
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length(); // already converted to local units;

    ON_3dPoint dir(extrusion_axis->Orientation());
    double mag = extrusion_axis->Magnitude() * LocalUnits::length;
    mag = FMAX(mag, bbdiag);

    ON_3dPoint startpnt;
    if (swept_curve->PointAtStart() == NULL) {
	startpnt = curve->PointAtStart();
    } else {
	startpnt = swept_curve->PointAtStart();
	startpnt = startpnt * LocalUnits::length;
    }

    // add a little buffer in the surface extrusion distance
    // by extruding "+/-mag" distance along "dir"
    ON_3dPoint extrusion_endpnt = startpnt + mag * dir;
    ON_3dPoint extrusion_startpnt = startpnt - mag * dir;

    ON_LineCurve *l = new ON_LineCurve(extrusion_startpnt, extrusion_endpnt);

    // the following extrude code lifted from OpenNURBS ON_BrepExtrude()
    ON_Line path_line;
    path_line.from = extrusion_startpnt;
    path_line.to = extrusion_endpnt;
    ON_3dVector path_vector = path_line.Direction();
    if (path_vector.IsZero()) {
	delete l;
	return false;
    }

    ON_SumSurface *sum_srf = 0;

    ON_Curve *srf_base_curve = curve->Duplicate();
    srf_base_curve->Translate(-mag * dir);

    ON_3dPoint sum_basepoint = ON_origin - l->PointAtStart();
    sum_srf = new ON_SumSurface();

    if (!sum_srf) {
	delete l;
	return false;
    }
    sum_srf->m_curve[0] = srf_base_curve;
    sum_srf->m_curve[1] = l; //srf_path_curve;
    sum_srf->m_basepoint = sum_basepoint;
    sum_srf->BoundingBox(); // fills in sum_srf->m_bbox


    ON_id = brep->AddSurface(sum_srf);

    return true;
}


bool
SurfaceOfRevolution::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_3dPoint start(axis_position->GetOrigin());
    start = start * LocalUnits::length;

    ON_3dVector dir(axis_position->GetNormal());
    ON_3dPoint end = start + dir;

    ON_Line axisline(start, end);
    ON_RevSurface *revsurf = ON_RevSurface::New();

    if (!revsurf) {
	return false;
    }
    revsurf->m_curve = brep->m_C3[swept_curve->GetONId()]->DuplicateCurve();
    revsurf->m_axis = axisline;
    revsurf->BoundingBox(); // fills in sum_srf->m_bbox

    //ON_Brep* b = ON_BrepRevSurface(revsurf, true, true);

    //if (!revsurf)
    //return false;

    ON_id = brep->AddSurface(revsurf);

    return true;
}


bool
SphericalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // get sphere center
    ON_3dPoint center(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    center = center * LocalUnits::length;
    if (!xaxis.Unitize() || !yaxis.Unitize())
	return false;

    // Creates a sphere with given center and radius.
    ON_Sphere sphere(center, radius * LocalUnits::length);
    sphere.plane = ON_Plane(center, xaxis, yaxis);
    if (!sphere.IsValid())
	return false;

    ON_RevSurface *s = sphere.RevSurfaceForm(false, nullptr);
    if (s) {
	double r = fabs(sphere.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	s->SetDomain(1, -r, r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ToroidalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    if (!xaxis.Unitize() || !yaxis.Unitize() || !norm.Unitize())
	return false;

    origin = origin * LocalUnits::length;

    ON_Plane p(origin, xaxis, yaxis);

    const double major = major_radius * LocalUnits::length;
    const double minor = minor_radius * LocalUnits::length;
    ON_Surface *surface = NULL;

    // Creates a torus parallel to the plane
    // with given major and minor radius.
    if (fabs(major) > fabs(minor)) {
	    ON_Torus torus(p, major, minor);
	    ON_RevSurface *revolution = torus.RevSurfaceForm();
	    if (revolution) {
		double r = fabs(torus.major_radius);
		if (r <= ON_SQRT_EPSILON) r = 1.0;
		revolution->SetDomain(0, 0.0, 2.0 * ON_PI * r);
		r = fabs(torus.minor_radius);
		if (r <= ON_SQRT_EPSILON) r = 1.0;
		revolution->SetDomain(1, 0.0, 2.0 * ON_PI * r);
		surface = revolution;
	    }
    } else {
	    /* ON_Torus intentionally rejects horn and spindle tori.  STEP permits
	     * them, and the same locus is represented exactly by revolving a
	     * rational NURBS circle whose center is major_radius from the axis.
	     * This retains the self-intersection instead of dropping the face or
	     * substituting approximate geometry. */
	    const ON_3dPoint profile_center = origin + major * xaxis;
	    ON_Plane profile_plane(profile_center, xaxis, norm);
	    ON_Circle profile_circle(profile_plane, fabs(minor));
	    ON_NurbsCurve *profile = ON_NurbsCurve::New();
	    if (!profile_circle.IsValid() || !profile ||
		    !profile_circle.GetNurbForm(*profile)) {
		delete profile;
		return false;
	    }
	    double profile_scale = fabs(minor);
	    if (profile_scale <= ON_SQRT_EPSILON) profile_scale = 1.0;
	    profile->SetDomain(0.0, 2.0 * ON_PI * profile_scale);

	    ON_RevSurface *revolution = ON_RevSurface::New();
	    revolution->m_curve = profile;
	    revolution->m_axis = ON_Line(origin, origin + norm);
	    revolution->m_angle = ON_Interval(0.0, 2.0 * ON_PI);
	    double revolution_scale = fabs(major);
	    if (revolution_scale <= ON_SQRT_EPSILON) revolution_scale = 1.0;
	    revolution->m_t = ON_Interval(0.0,
		2.0 * ON_PI * revolution_scale);
	    revolution->m_bTransposed = false;
	    revolution->BoundingBox();
	    if (!revolution->IsValid()) {
		delete revolution;
		return false;
	    }
	    surface = revolution;
    }

    if (!surface)
	return false;
    ON_id = brep->AddSurface(surface);
    if (ON_id < 0) {
	delete surface;
	return false;
    }

    return true;
}


bool
VertexLoop::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //load vertex
    loop_vertex->LoadONBrep(brep);

    ON_3dPoint vertex(loop_vertex->Point3d());

    // create singular trim;
    ON_BrepLoop &loop = brep->m_L[ON_loop_index];
    ON_BrepFace *face = loop.Face();
    const ON_Surface *surface = face->SurfaceOf();

    ON_Interval U = surface->Domain(0);
    ON_Interval V = surface->Domain(1);
    ON_3dPoint corner[4];
    corner[0] = surface->PointAt(U.m_t[0], V.m_t[0]);
    corner[1] = surface->PointAt(U.m_t[1], V.m_t[0]);
    corner[2] = surface->PointAt(U.m_t[0], V.m_t[1]);
    corner[3] = surface->PointAt(U.m_t[1], V.m_t[1]);

    ON_2dPoint start, end;
    ON_Surface::ISO iso = ON_Surface::N_iso;
    if (VNEAR_EQUAL(vertex, corner[0], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	    //south;
	    end = ON_2dPoint(U.m_t[1], V.m_t[0]);
	    iso = ON_Surface::S_iso;
	} else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	    //west
	    end = ON_2dPoint(U.m_t[0], V.m_t[1]);
	    iso = ON_Surface::W_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[1], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //east
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::E_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[1]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //north
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::N_iso;
	}
    }
    ON_Curve *c2d = new ON_LineCurve(start, end);
    int trimCurve = brep->m_C2.Count();
    brep->m_C2.Append(c2d);

    (void)brep->NewSingularTrim(brep->m_V[loop_vertex->GetONId()], loop, iso, trimCurve);
    /* FIXME: do something with this trim */

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
