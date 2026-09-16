/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepPullback.cpp
 *
 * Closed-surface parameter handling and exact pcurve pullback/regeneration.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

void
closed_trim_endpoint_alignments(const ON_BrepTrim &trim, const ON_BrepEdge &edge,
	const ON_Surface *surface, double alignment[2])
{
    alignment[0] = ON_UNSET_VALUE;
    alignment[1] = ON_UNSET_VALUE;
    if (!surface)
	return;
    const ON_Interval trim_domain = trim.Domain();
    for (int end = 0; end < 2; ++end) {
	ON_3dPoint uv, lifted_point;
	ON_3dVector uv_tangent, du, dv;
	const double trim_parameter = trim_domain[end];
	const double edge_parameter = edge.Domain()[trim.m_bRev3d ? 1 - end : end];
	if (!trim.Ev1Der(trim_parameter, uv, uv_tangent) ||
		!surface->Ev1Der(uv.x, uv.y, lifted_point, du, dv))
	    continue;
	ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	ON_3dVector edge_tangent = edge.TangentAt(edge_parameter);
	if (!lifted_tangent.Unitize() || !edge_tangent.Unitize())
	    continue;
	alignment[end] = lifted_tangent * edge_tangent;
	if (trim.m_bRev3d)
	    alignment[end] = -alignment[end];
    }
}


bool
closed_trim_endpoint_alignment_is_valid(double alignment)
{
    /* ON_UNSET_VALUE is deliberately a large negative sentinel.  Comparing
     * it directly with zero mistakes an unevaluable tangent (most commonly
     * at a collapsed surface pole) for strong evidence of reversed geometry
     * and can launch an unnecessary dense pcurve regeneration. */
    return ON_IsValid(alignment) && alignment > ON_UNSET_VALUE;
}


bool
refine_surface_pullback_seeded(const ON_Surface *surface, const ON_3dPoint &target,
    double tolerance, ON_3dPoint &uv, double *final_distance,
    bool confine_to_domain, double acceptance_tolerance)
{
    if (final_distance)
	*final_distance = DBL_MAX;
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return false;
    if (!(acceptance_tolerance > 0.0))
	acceptance_tolerance = tolerance;

    if (confine_to_domain) {
	for (int direction = 0; direction < 2; ++direction) {
	    const ON_Interval domain = surface->Domain(direction);
	    uv[direction] = std::max(domain.Min(),
		std::min(domain.Max(), uv[direction]));
	}
    }

    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 32; ++iteration) {
	ON_3dPoint lifted;
	ON_3dVector du, dv;
	if (!surface->Ev1Der(uv.x, uv.y, lifted, du, dv) || !lifted.IsValid())
	    break;
	const ON_3dVector residual = target - lifted;
	best_distance = residual.Length();
	if (best_distance <= tolerance) {
	    if (final_distance)
		*final_distance = best_distance;
	    return true;
	}

	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = metric_scale * 1.0e-14;
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
		if (!confine_to_domain && surface->IsClosed(direction))
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
    return best_distance <= acceptance_tolerance;
}


void
normalize_closed_surface_parameter(const ON_Surface *surface,
	const ON_3dPoint &target, double tolerance, ON_3dPoint &uv)
{
    if (!surface || !target.IsValid() || !uv.IsValid() || !(tolerance > 0.0))
	return;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		(uv[direction] >= domain.Min() && uv[direction] <= domain.Max()))
	    continue;
	double wrapped = fmod(uv[direction] - domain.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	ON_3dPoint candidate = uv;
	candidate[direction] = domain.Min() + wrapped;
	const ON_3dPoint lifted = surface->PointAt(candidate.x, candidate.y);
	if (lifted.IsValid() && lifted.DistanceTo(target) <= tolerance)
	    uv = candidate;
    }
}


bool
has_unwrapped_closed_parameter(const ON_Surface *surface, const ON_3dPoint &uv)
{
    if (!surface || !uv.IsValid())
	return false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double guard = std::max(ON_ZERO_TOLERANCE,
	    domain.Length() * 1.0e-12);
	if (uv[direction] < domain.Min() - guard ||
		uv[direction] > domain.Max() + guard)
	    return true;
    }
    return false;
}


/* Evaluate a mathematically closed surface at a continuous, possibly
 * unwrapped UV image without asking a non-periodic NURBS implementation to
 * extrapolate beyond its native knot domain.  The returned native parameter
 * is lift-equivalent by the surface's own IsClosed() contract.  Intermediate
 * pcurves may use at most one such image while periodic topology is being
 * split; final OpenNURBS validation still requires native-domain trims. */
ON_3dPoint
closed_surface_native_parameter(const ON_Surface *surface, ON_3dPoint uv)
{
    if (!surface || !uv.IsValid())
	return ON_3dPoint::UnsetPoint;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		(uv[direction] >= domain.Min() &&
		 uv[direction] <= domain.Max()))
	    continue;
	double wrapped = fmod(uv[direction] - domain.Min(), period);
	if (wrapped < 0.0)
	    wrapped += period;
	uv[direction] = domain.Min() + wrapped;
    }
    return uv;
}


ON_3dPoint
closed_surface_point_at(const ON_Surface *surface, const ON_3dPoint &uv)
{
    const ON_3dPoint native = closed_surface_native_parameter(surface, uv);
    return surface && native.IsValid() ?
	surface->PointAt(native.x, native.y) : ON_3dPoint::UnsetPoint;
}


bool
closed_surface_ev1der(const ON_Surface *surface, const ON_3dPoint &uv,
	ON_3dPoint &point, ON_3dVector &du, ON_3dVector &dv)
{
    const ON_3dPoint native = closed_surface_native_parameter(surface, uv);
    return surface && native.IsValid() &&
	surface->Ev1Der(native.x, native.y, point, du, dv);
}


bool
regenerate_trim_polyline(ON_Brep *brep, ON_BrepTrim &trim,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs, double tolerance,
	std::string *failure_reason, PeriodicPullbackCrossing *periodic_crossing,
	const ON_3dPoint *required_start, const ON_3dPoint *required_end,
	bool prefer_edge_driven, STEPWrapper *wrapper,
	bool preserve_required_uv_images, ON_Curve **generated_curve)
{
    if (failure_reason)
	failure_reason->clear();
    if (periodic_crossing)
	*periodic_crossing = PeriodicPullbackCrossing();
    if (generated_curve)
	*generated_curve = NULL;
    if (!brep || !surface || !(tolerance > 0.0))
	return false;

    const ON_BrepEdge *edge = trim.Edge();
    if (!edge)
	return false;
    const ON_Interval trim_domain = trim.Domain();
    brlcad::PullbackContext pullback_context;
    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_feature_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    const auto pullback_solver_tolerance = [edge_feature_scale](
	    double acceptance_tolerance) {
	double solver_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    acceptance_tolerance * 0.1);
	if (edge_feature_scale > ON_ZERO_TOLERANCE &&
		acceptance_tolerance > edge_feature_scale *
		    kPullbackLooseToleranceFeatureFraction)
	    solver_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		std::min(solver_tolerance, edge_feature_scale *
		    kPullbackSolverFeatureFraction));
	return solver_tolerance;
    };
    std::string edge_driven_failure;
    for (int regeneration_mode = 0; regeneration_mode < 2; ++regeneration_mode) {
    const bool edge_driven = prefer_edge_driven ? regeneration_mode == 0 :
	regeneration_mode == 1;
    const int maximum_segments = edge_driven ? 64 : kDenseValidationSegments;
    for (int segment_count = 64; segment_count <= maximum_segments; segment_count *= 2) {
	std::string rejection = "unknown rejection";
	ON_3dPointArray points;
	points.Reserve(segment_count + 1);
	std::vector<double> normalized_parameters;
	normalized_parameters.reserve(segment_count + 3);
	bool valid = true;
	for (int sample = 0; sample <= segment_count; ++sample) {
	    if (wrapper && (sample % 8) == 0) {
		std::ostringstream detail;
		detail << "trim=T" << trim.m_trim_index << " sample="
		    << sample << '/' << segment_count << " mode="
		    << (edge_driven ? "edge" : "source");
		wrapper->SetProgressDetail("sampling exact edge pullback", 0,
		    static_cast<uint64_t>(sample),
		    static_cast<uint64_t>(segment_count), "samples", detail.str());
	    }
	    if (brlcad::PullbackWorkCancelled()) {
		if (failure_reason)
		    *failure_reason = "regenerated-pcurve sampling was cancelled";
		return false;
	    }
	    const double normalized = static_cast<double>(sample) /
		static_cast<double>(segment_count);
	    const ON_3dPoint *required_endpoint = sample == 0 ? required_start :
		(sample == segment_count ? required_end : NULL);
	    const ON_3dPoint source_uv = required_endpoint ? *required_endpoint :
		trim.PointAt(trim_domain.ParameterAt(normalized));
	    ON_3dPoint uv = source_uv;
	    if (edge_driven && sample > 0 && sample < segment_count) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		double pullback_distance = DBL_MAX;
		/* An exact surface point can have several parameter-space preimages at
		 * a pole or collapsed boundary.  Continue from the preceding exact
		 * sample before consulting the independently parameterized source
		 * pcurve, so adjacent samples remain on one lift-equivalent branch. */
		const bool have_continuation_seed = points.Count() > 0;
		if (have_continuation_seed)
		    uv = points[points.Count() - 1];
		bool pulled = refine_surface_pullback_seeded(surface, edge_point,
		    solver_tolerance, uv, &pullback_distance,
		    !has_unwrapped_closed_parameter(surface, uv),
		    pullback_tolerance);
		if (!pulled && have_continuation_seed) {
		    uv = source_uv;
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, uv),
			pullback_tolerance);
		}
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
			    pulled_lift, pullback_distance, 0,
			    solver_tolerance,
			    pullback_tolerance) || pullback_distance > pullback_tolerance) {
			rejection = "exact edge pullback failed";
			valid = false;
			break;
		    }
		    uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    /* Select the whole-domain branch nearest the supplied pcurve, but
		     * retain a shift only when its lift proves it is the same exact edge
		     * point.  This also handles imported periodic geometry whose closure
		     * flag was not retained by the NURBS representation. */
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round(
			    (source_uv[direction] - uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
		/* A seeded Newton solve may converge on an extrapolated image of a
		 * geometrically closed but non-periodic NURBS surface.  Anchor every
		 * successful projection—not
		 * only the global closest-point fallback—to the image nearest the
		 * supplied pcurve before enforcing continuity with the preceding
		 * sample.  Accept only an exact integral-period shift whose lift still
		 * matches the authoritative edge.  Without this anchor, successive
		 * samples can drift by dozens of whole periods and force pathological
		 * adaptive subdivision. */
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction) ||
			    surface->IsPeriodic(direction))
			continue;
		    const double direction_period =
			surface->Domain(direction).Length();
		    if (!(direction_period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = uv;
		    shifted_uv[direction] += round((source_uv[direction] -
			uv[direction]) / direction_period) * direction_period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			uv = shifted_uv;
		}
		bool excessive_period_drift = false;
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction) ||
			    surface->IsPeriodic(direction))
			continue;
		    const double direction_period =
			surface->Domain(direction).Length();
		    excessive_period_drift = excessive_period_drift ||
			(direction_period > ON_ZERO_TOLERANCE &&
			 fabs(uv[direction] - source_uv[direction]) >
			     kMaximumSeededPullbackPeriodDrift * direction_period);
		}
		if (excessive_period_drift) {
		    ON_2dPoint native_uv;
		    ON_3dPoint native_lift;
		    double native_distance = DBL_MAX;
		    if (pullback_context.SurfaceClosestPoint(surface, edge_point,
			    native_uv, native_lift, native_distance, 0,
			    solver_tolerance, pullback_tolerance) &&
			    native_distance <= pullback_tolerance)
			uv.Set(native_uv.x, native_uv.y, 0.0);
		}
		/* Keep consecutive exact samples on one lift-equivalent domain branch.
		 * The supplied curve may legitimately cross a parameter seam; using its
		 * independently wrapped values would create a full-domain chord. */
		if (points.Count() > 0) {
		    const ON_3dPoint previous_uv = points[points.Count() - 1];
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((previous_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			    uv = shifted_uv;
		    }
		}
	    } else if (edge_driven) {
		/* Endpoints are shared with adjacent trims in the same p-space loop.
		 * Refine them to the authoritative edge endpoint when possible; a
		 * merely in-tolerance source endpoint can otherwise make the final
		 * polyline chord backtrack.  If exact refinement is unavailable,
		 * preserve an already valid source endpoint for the bounded join pass. */
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		const ON_3dPoint endpoint_lift = closed_surface_point_at(surface, uv);
		const bool source_endpoint_valid = endpoint_lift.IsValid() &&
		    endpoint_lift.DistanceTo(edge_point) <= tolerance;
		/* Keep numerical convergence tighter than the established geometric
		 * acceptance tolerance, but do not use that solver target as a second,
		 * undocumented rejection threshold.  A safely measured edge/surface
		 * mismatch can exceed one tenth of the local tolerance even though the
		 * exact closest surface point is well inside the fully validated bound.
		 * Conflating the two made closed edges retain an arbitrary cyclic pcurve
		 * start when their authoritative STEP endpoint could not satisfy the
		 * unnecessarily tighter threshold. */
		const double endpoint_acceptance_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    endpoint_acceptance_tolerance);
		ON_3dPoint refined_uv = uv;
		double pullback_distance = DBL_MAX;
		bool pulled = required_endpoint && source_endpoint_valid;
		if (pulled)
		    pullback_distance = endpoint_lift.DistanceTo(edge_point);
		else
		    pulled = refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, refined_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, refined_uv),
			endpoint_acceptance_tolerance);
		if (!pulled) {
		    ON_2dPoint pulled_uv;
		    ON_3dPoint pulled_lift;
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pullback_distance, 0,
			solver_tolerance, endpoint_acceptance_tolerance) &&
			pullback_distance <= endpoint_acceptance_tolerance;
		    if (pulled)
			refined_uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		}
		if (pulled)
		    uv = refined_uv;
		else if (!source_endpoint_valid) {
		    rejection = "exact edge endpoint pullback failed";
		    valid = false;
		    break;
		}
		if (pulled) {
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((source_uv[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() && shifted_lift.DistanceTo(
				edge_point) <= endpoint_acceptance_tolerance)
			    uv = shifted_uv;
		    }
		}
	    }
	    const bool preserve_required_image = preserve_required_uv_images &&
		((sample == 0 && required_start) ||
		 (sample == segment_count && required_end));
	    if (edge_driven && points.Count() > 0 && !preserve_required_image) {
		const ON_3dPoint previous_uv = points[points.Count() - 1];
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = uv;
		    shifted_uv[direction] += std::round((previous_uv[direction] -
			uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    /* A required loop endpoint is authoritative as a 3-D locus, not as
		     * a particular periodic image.  Prefer its lift-equivalent image
		     * adjacent to the regenerated interior; the subsequent bounded
		     * loop pass translates the neighboring complete pcurve to the same
		     * image and revalidates it. */
		    if (shifted_lift.IsValid() && shifted_lift.DistanceTo(edge_point) <=
			    tolerance)
			uv = shifted_uv;
		}
	    }
	    if (edge_driven && !required_endpoint) {
		const double edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		normalize_closed_surface_parameter(surface,
		    edge->PointAt(edge_parameter), tolerance, uv);
	    }
	    if (!uv.IsValid()) {
		rejection = edge_driven ? "invalid exact-edge pullback sample" :
		    "invalid source pcurve sample";
		valid = false;
		break;
	    }
	    points.Append(ON_3dPoint(uv.x, uv.y, 0.0));
	    normalized_parameters.push_back(normalized);
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " while sampling " +
		    std::to_string(segment_count) + " pcurve segments";
	    continue;
	}
	if (edge_driven && points.Count() >= 3) {
	    /* A surface singularity can give an edge endpoint several exact UV
	     * preimages.  Pulling it independently from the supplied endpoint may
	     * select a different branch than the exact interior samples, leaving an
	     * unsplittable parameter-space jump.  Re-pull from the adjacent exact
	     * sample and retain it only under a tighter edge-lift bound. */
	    for (int end = 0; end < 2; ++end) {
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		const bool required_endpoint = end == 0 ? required_start != NULL :
		    required_end != NULL;
		if (required_endpoint && preserve_required_uv_images)
		    continue;
		if (required_endpoint && !surface->IsClosed(0) &&
			!surface->IsClosed(1))
		    continue;
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const double pullback_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		const double solver_tolerance = pullback_solver_tolerance(
		    pullback_tolerance);
		ON_3dPoint continuous_uv = points[adjacent_index];
		double pullback_distance = DBL_MAX;
		if (!refine_surface_pullback_seeded(surface, edge_point,
			solver_tolerance, continuous_uv, &pullback_distance,
			!has_unwrapped_closed_parameter(surface, continuous_uv),
			pullback_tolerance))
		    continue;
		for (int direction = 0; direction < 2; ++direction) {
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    ON_3dPoint shifted_uv = continuous_uv;
		    shifted_uv[direction] += std::round((points[adjacent_index][direction] -
			continuous_uv[direction]) / period) * period;
		    const ON_3dPoint shifted_lift = closed_surface_point_at(
			surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			continuous_uv = shifted_uv;
		}
		const ON_3dPoint continuous_lift = closed_surface_point_at(
		    surface, continuous_uv);
		/* A required endpoint fixes the exact 3-D loop join, not a particular
		 * image of that point on a periodic parameter domain.  Adopt a newly
		 * refined image only when it is lift-equivalent and strictly nearer the
		 * exact interior branch.  This prevents a full-period final chord while
		 * leaving nonperiodic endpoint constraints untouched. */
		if (continuous_lift.IsValid() &&
			continuous_lift.DistanceTo(edge_point) <= pullback_tolerance &&
			(!required_endpoint || continuous_uv.DistanceTo(
			    points[adjacent_index]) < points[endpoint_index].DistanceTo(
			    points[adjacent_index])))
		    points[endpoint_index] = continuous_uv;
	    }
	    /* At a true surface pole the periodic parameter is undefined.  A
	     * closest-point solve can consequently return any longitude for the
	     * exact endpoint even though the adjacent, nonsingular edge samples
	     * converge on one unambiguous branch.  Leaving that arbitrary value in
	     * the pcurve creates a zero-area hook at the pole: the BREP remains
	     * structurally valid, but shaded-mesh repair can detach or omit the
	     * spherical cap.  Select the limiting branch of the already validated
	     * adjacent edge sample.  The candidate is accepted only when the
	     * surface advertises the boundary as singular and its 3-D lift still
	     * matches both the exact STEP edge endpoint and topology vertex within
	     * the established tolerance. */
	    for (int end = 0; end < 2; ++end) {
		const int endpoint_index = end == 0 ? 0 : points.Count() - 1;
		const int adjacent_index = end == 0 ? 1 : points.Count() - 2;
		ON_3dPoint candidate = points[endpoint_index];
		int singular_direction = -1;
		int singular_side = -1;
		for (int direction = 0; direction < 2 && singular_direction < 0;
			direction++) {
		    const ON_Interval domain = surface->Domain(direction);
		    if (!domain.IsIncreasing())
			continue;
		    const double guard = std::max(ON_ZERO_TOLERANCE *
			kNumericalToleranceScale,
			kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		    for (int side = 0; side < 2; ++side) {
			const int surface_side = direction == 0 ?
			    (side == 0 ? 3 : 1) : (side == 0 ? 0 : 2);
			if (fabs(candidate[direction] - domain[side]) <= guard &&
				surface->IsSingular(surface_side)) {
			    singular_direction = direction;
			    singular_side = side;
			    break;
			}
		    }
		}
		if (singular_direction < 0 ||
			!surface->IsClosed(1 - singular_direction))
		    continue;
		const int branch_direction = 1 - singular_direction;
		candidate[singular_direction] =
		    surface->Domain(singular_direction)[singular_side];
		candidate[branch_direction] =
		    points[adjacent_index][branch_direction];
		const double normalized = end == 0 ? 0.0 : 1.0;
		const ON_3dPoint edge_point = edge->PointAt(
		    edge->Domain().ParameterAt(trim.m_bRev3d ?
			1.0 - normalized : normalized));
		const int vertex_index = trim.m_vi[end];
		const ON_3dPoint lift = closed_surface_point_at(surface, candidate);
		if (lift.IsValid() && edge_point.IsValid() && vertex_index >= 0 &&
			vertex_index < brep->m_V.Count() &&
			lift.DistanceTo(edge_point) <= tolerance &&
			lift.DistanceTo(brep->m_V[vertex_index].point) <= tolerance)
		    points[endpoint_index] = candidate;
	    }
	    for (int point_index = 1; point_index < points.Count();) {
		if (points[point_index - 1].DistanceTo(points[point_index]) >
			ON_ZERO_TOLERANCE) {
		    ++point_index;
		    continue;
		}
		const int remove_index = point_index == points.Count() - 1 ?
		    point_index - 1 : point_index;
		points.Remove(remove_index);
		normalized_parameters.erase(normalized_parameters.begin() + remove_index);
		point_index = std::max(1, remove_index);
	    }
	    if (points.Count() < 2) {
		if (failure_reason)
		    *failure_reason = "continuous endpoint pullback collapsed the pcurve";
		continue;
	    }
	}

	/* Refine only the exact-edge pullback segments whose straight UV chord
	 * misses the edge.  Uniformly raising the global sample count performs
	 * poorly near surface singularities and wastes thousands of samples on
	 * already-flat portions of the curve. */
	std::set<std::pair<double, double> > singular_parameter_connectors;
	if (edge_driven) {
	    const size_t maximum_points = kMaximumAdaptivePullbackPoints;
	    bool replaced_required_endpoint_image[2] = {false, false};
	    /* Inserting a midpoint changes only the interval being split.  Advance
	     * monotonically after an interval passes; after a split, retain the
	     * current index to prove its left child and then its right child.  A
	     * restart from segment zero (even with an ordered cache of validated
	     * intervals) makes a 4096-point refinement quadratic in both scans and
	     * tree lookups.  No acceptance test changes: every resulting interval
	     * still passes the same three quarter-point lift checks and the complete
	     * candidate still receives the dense validation below. */
	    int segment = 0;
	    size_t refinement_iterations = 0;
	    while (valid && segment + 1 < points.Count()) {
		bool excessive_nonperiodic_branch = false;
		for (int endpoint = 0; endpoint < 2 &&
			excessive_nonperiodic_branch == false; ++endpoint) {
		    const ON_3dPoint &point = points[segment + endpoint];
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
		    const ON_Interval domain = surface->Domain(direction);
		    const double guard = std::max(ON_ZERO_TOLERANCE,
			    kPeriodicParameterSnapFraction *
				std::max(1.0, domain.Length()));
		    const bool outside_native =
			point[direction] < domain.Min() - guard ||
			point[direction] > domain.Max() + guard;
		    if (outside_native && periodic_crossing &&
			    !periodic_crossing->detected) {
			periodic_crossing->detected = true;
			periodic_crossing->trim_fraction = 0.5 *
			    (normalized_parameters[segment] +
			     normalized_parameters[segment + 1]);
			periodic_crossing->surface_direction = direction;
		    }
		    excessive_nonperiodic_branch =
			point[direction] < domain.Min() - domain.Length() - guard ||
			point[direction] > domain.Max() + domain.Length() + guard;
		    if (excessive_nonperiodic_branch) {
			std::ostringstream detail;
			detail << "closed nonperiodic surface pullback exceeded one "
			    << "unwrapped image in direction " << direction
			    << " at segment " << segment << " endpoint "
			    << endpoint << " (parameter " << point[direction]
			    << ", domain " << domain.Min() << ':' << domain.Max()
			    << ')';
			rejection = detail.str();
			break;
		    }
		}
	    }
	    if (excessive_nonperiodic_branch) {
		valid = false;
		break;
	    }
		if (wrapper && ((refinement_iterations++ & 31) == 0)) {
		    std::ostringstream detail;
		    detail << "trim=T" << trim.m_trim_index << " segment="
			<< segment << '/' << points.Count() - 1 << " points="
			<< points.Count() << '/' << maximum_points;
		    if (segment >= 0 && segment + 1 < points.Count())
			detail << " interval=" << normalized_parameters[segment]
			    << ':' << normalized_parameters[segment + 1]
			    << " uv=" << points[segment].x << ':'
			    << points[segment].y << "->" << points[segment + 1].x
			    << ':' << points[segment + 1].y;
		    wrapper->SetProgressDetail("refining exact edge pullback", 0,
			static_cast<uint64_t>(points.Count()),
			static_cast<uint64_t>(maximum_points), "points", detail.str());
		}
		if (brlcad::PullbackWorkCancelled()) {
		    if (failure_reason)
			*failure_reason =
			    "adaptive exact-edge pullback was cancelled";
		    return false;
		}
		bool split_segment = false;
		    for (int sub = 1; sub <= 3; ++sub) {
			const double fraction = static_cast<double>(sub) / 4.0;
			const double normalized = (1.0 - fraction) *
			    normalized_parameters[segment] + fraction *
			    normalized_parameters[segment + 1];
			const ON_3dPoint candidate_uv = (1.0 - fraction) * points[segment] +
			    fraction * points[segment + 1];
			const ON_3dPoint candidate_lift = surface->PointAt(
			    candidate_uv.x, candidate_uv.y);
			const double edge_parameter = edge->Domain().ParameterAt(
			    trim.m_bRev3d ? 1.0 - normalized : normalized);
			if (!candidate_lift.IsValid() || candidate_lift.DistanceTo(
				edge->PointAt(edge_parameter)) > tolerance) {
			    split_segment = true;
			    break;
			}
		    }
		    if (!split_segment) {
			++segment;
			continue;
		    }
		    bool collapsed_periodic_branch = false;
		    double rejected_periodic_shift_distance = DBL_MAX;
		    const double second_normalized =
			normalized_parameters[segment + 1];
		    const ON_3dPoint second_edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - second_normalized : second_normalized));
		    const bool preserve_required_end = required_end &&
			segment + 1 == points.Count() - 1;
		    for (int direction = 0; !preserve_required_end && direction < 2;
			    ++direction) {
			if (!surface->IsClosed(direction))
			    continue;
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted = points[segment + 1];
			shifted[direction] += std::round((points[segment][direction] -
			    shifted[direction]) / period) * period;
			if (fabs(shifted[direction] -
				points[segment + 1][direction]) <= ON_ZERO_TOLERANCE ||
				shifted.DistanceTo(points[segment]) >=
				points[segment + 1].DistanceTo(points[segment]))
			    continue;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted);
			double shifted_distance = shifted_lift.IsValid() &&
			    second_edge_point.IsValid() ?
			    shifted_lift.DistanceTo(second_edge_point) : DBL_MAX;
			if (shifted_distance > tolerance && second_edge_point.IsValid()) {
			    ON_3dPoint refined_shift = shifted;
			    double refined_distance = DBL_MAX;
				if (refine_surface_pullback_seeded(surface, second_edge_point,
					pullback_solver_tolerance(tolerance), refined_shift,
					&refined_distance, false, tolerance) &&
				    refined_shift.DistanceTo(points[segment]) <
					points[segment + 1].DistanceTo(points[segment])) {
				shifted = refined_shift;
				shifted_distance = refined_distance;
			    }
			}
			if (shifted_distance < rejected_periodic_shift_distance) {
			    rejected_periodic_shift_distance = shifted_distance;
			}
			if (shifted_distance <= tolerance) {
			    points[segment + 1] = shifted;
			    collapsed_periodic_branch = true;
			}
		    }
		    if (collapsed_periodic_branch) {
			/* Recheck this interval after selecting the nearer exact
			 * periodic image.  Its following interval has not yet been
			 * visited, so no accepted prefix is invalidated. */
			continue;
		    }
		    const double parameter_midpoint = 0.5 *
			(normalized_parameters[segment] +
			 normalized_parameters[segment + 1]);
		    if (!(parameter_midpoint > normalized_parameters[segment] &&
			    parameter_midpoint < normalized_parameters[segment + 1])) {
			/* A required loop endpoint can name a different periodic UV image
			 * of the same exact 3-D vertex.  Adaptive edge pullback approaches
			 * the continuous image from the interior, but eventually no double
			 * remains between its normalized parameter and 0 or 1.  At that
			 * machine-precision limit, retain the interior-side image only when
			 * its lift independently matches both the exact edge endpoint and
			 * topology vertex under a tighter tolerance.  The complete candidate
			 * is still densely validated below, and the adjacent-loop repair moves
			 * the neighboring endpoint to the same lift-equivalent image. */
			const bool closed_surface = surface->IsClosed(0) ||
			    surface->IsClosed(1);
			const bool collapsed_required_start = closed_surface &&
			    required_start && segment == 0;
			const bool collapsed_required_end = closed_surface &&
			    required_end && segment + 1 == points.Count() - 1;
			if (collapsed_required_start || collapsed_required_end) {
			    const int end = collapsed_required_start ? 0 : 1;
			    const int limit_index = collapsed_required_start ?
				segment + 1 : segment;
			    const ON_3dPoint limit_lift = closed_surface_point_at(
				surface, points[limit_index]);
			    const ON_3dPoint edge_endpoint = edge->PointAt(
				edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
			    const int vertex_index = trim.m_vi[end];
			    const double endpoint_tolerance = std::max(
				ON_ZERO_TOLERANCE * kNumericalToleranceScale,
				tolerance * 0.1);
			    if (limit_lift.IsValid() && edge_endpoint.IsValid() &&
				vertex_index >= 0 && vertex_index < brep->m_V.Count() &&
				limit_lift.DistanceTo(edge_endpoint) <= endpoint_tolerance &&
				limit_lift.DistanceTo(brep->m_V[vertex_index].point) <=
				    tolerance) {
				if (replaced_required_endpoint_image[end]) {
				    rejection = "adaptive exact-edge pullback repeatedly "
					"replaced the same required endpoint after exhausting "
					"representable parameter progress";
				    valid = false;
				    break;
				}
				replaced_required_endpoint_image[end] = true;
				if (collapsed_required_start) {
				    points.Remove(0);
				    normalized_parameters.erase(
					normalized_parameters.begin());
				    normalized_parameters.front() = 0.0;
				    segment = 0;
				} else {
				    points.Remove(points.Count() - 1);
				    normalized_parameters.pop_back();
				    normalized_parameters.back() = 1.0;
				    segment = std::max(0, segment - 1);
				}
				continue;
			    }
			}
			if (!valid)
			    break;
			const int varying_direction = fabs(points[segment + 1].y -
			    points[segment].y) > fabs(points[segment + 1].x -
			    points[segment].x) ? 1 : 0;
			const int fixed_direction = 1 - varying_direction;
			const ON_Interval fixed_domain = surface->Domain(fixed_direction);
			std::vector<double> fixed_candidates;
			fixed_candidates.reserve(134);
			const double fixed_first = points[segment][fixed_direction];
			const double fixed_second = points[segment + 1][fixed_direction];
			fixed_candidates.push_back(fixed_first);
			fixed_candidates.push_back(fixed_second);
			fixed_candidates.push_back(0.5 * (fixed_first + fixed_second));
			fixed_candidates.push_back(fixed_domain.Min());
			fixed_candidates.push_back(fixed_domain.Max());
			const double fixed_min = std::min(fixed_first, fixed_second);
			const double fixed_max = std::max(fixed_first, fixed_second);
			const double fixed_span = fixed_max - fixed_min;
			if (fixed_span > ON_ZERO_TOLERANCE) {
			    const double search_min = fixed_min - 4.0 * fixed_span;
			    const double search_max = fixed_max + 4.0 * fixed_span;
			    for (int search_sample = 0; search_sample <= 128;
				    ++search_sample)
				fixed_candidates.push_back(search_min +
				    (search_max - search_min) * search_sample / 128.0);
			}
			bool connected = false;
			double best_cost = DBL_MAX;
			ON_3dPoint best_start;
			ON_3dPoint best_end;
			/* A constant-parameter connector can represent the same 3-D edge
			 * while its UV endpoints differ only when that parameter direction
			 * has a genuinely collapsed surface side.  The former unconditional
			 * search evaluated up to 134 * 1025 expensive curve/surface pairs
			 * after an interval had already reached adjacent floating-point
			 * parameters, even on cylinders and other non-singular surfaces.
			 * OpenNURBS' singular-side classification is the required geometric
			 * proof; without it, reject the no-progress interval immediately. */
			const int first_singular_side = fixed_direction == 0 ? 3 : 0;
			const int second_singular_side = fixed_direction == 0 ? 1 : 2;
			const bool singular_connector_possible =
			    surface->IsSingular(first_singular_side) ||
			    surface->IsSingular(second_singular_side);
			for (std::vector<double>::const_iterator fixed_candidate =
				fixed_candidates.begin(); fixed_candidate !=
				fixed_candidates.end() && singular_connector_possible;
				++fixed_candidate) {
			    if (brlcad::PullbackWorkCancelled()) {
				if (failure_reason)
				    *failure_reason =
					"singular connector search was cancelled";
				return false;
			    }
			    ON_3dPoint connector_start = points[segment];
			    ON_3dPoint connector_end = points[segment + 1];
			    connector_start[fixed_direction] =
				*fixed_candidate;
			    connector_end[fixed_direction] =
				*fixed_candidate;
			    if (connector_start.DistanceTo(connector_end) <=
				    ON_ZERO_TOLERANCE)
				continue;
			    bool exact_connector = true;
			for (int connector_sample = 0;
				exact_connector &&
				connector_sample <= kDenseValidationSegments;
				++connector_sample) {
				if ((connector_sample & 63) == 0 &&
					brlcad::PullbackWorkCancelled()) {
				    if (failure_reason)
					*failure_reason =
					    "singular connector validation was cancelled";
				    return false;
				}
				const double fraction = static_cast<double>(connector_sample) /
				    kDenseValidationSegments;
				const double normalized = (1.0 - fraction) *
				    normalized_parameters[segment] + fraction *
				    normalized_parameters[segment + 1];
				const ON_3dPoint connector_uv = (1.0 - fraction) *
				    connector_start + fraction * connector_end;
				const ON_3dPoint connector_lift = closed_surface_point_at(
				    surface, connector_uv);
				const ON_3dPoint connector_edge = edge->PointAt(
				    edge->Domain().ParameterAt(trim.m_bRev3d ?
					1.0 - normalized : normalized));
				const double connector_tolerance =
				    (connector_sample == 0 ||
				     connector_sample == kDenseValidationSegments) ?
				    std::max(ON_ZERO_TOLERANCE * kNumericalToleranceScale,
					tolerance * 0.1) : tolerance;
				exact_connector = connector_lift.IsValid() &&
				    connector_edge.IsValid() && connector_lift.DistanceTo(
					connector_edge) <= connector_tolerance;
			    }
			    const double cost = fabs(*fixed_candidate -
				points[segment][fixed_direction]) +
				fabs(*fixed_candidate -
				points[segment + 1][fixed_direction]);
			    if (exact_connector && cost < best_cost) {
				connected = true;
				best_cost = cost;
				best_start = connector_start;
				best_end = connector_end;
			    }
			}
			if (connected) {
			    points[segment] = best_start;
			    points[segment + 1] = best_end;
			    singular_parameter_connectors.insert(std::make_pair(
				normalized_parameters[segment],
				normalized_parameters[segment + 1]));
			    /* The complete connector was densely proven above. */
			    ++segment;
			    continue;
			}
			if (periodic_crossing && !periodic_crossing->detected) {
			    for (int direction = 0; direction < 2; ++direction) {
				if (!surface->IsClosed(direction))
				    continue;
				const double period = surface->Domain(direction).Length();
				if (!(period > ON_ZERO_TOLERANCE) || fabs(
					points[segment + 1][direction] -
					points[segment][direction]) <= 0.5 * period)
				    continue;
				periodic_crossing->detected = true;
				periodic_crossing->trim_fraction = 0.5 *
				    (normalized_parameters[segment] +
				     normalized_parameters[segment + 1]);
				periodic_crossing->surface_direction = direction;
				break;
			    }
			}
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
			rejection = "adaptive exact-edge pullback made no parameter progress at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) + ", domains " +
			    std::to_string(surface->Domain(0).Min()) + ":" +
			    std::to_string(surface->Domain(0).Max()) + "," +
			    std::to_string(surface->Domain(1).Min()) + ":" +
			    std::to_string(surface->Domain(1).Max()) + ", closed " +
			    std::to_string(surface->IsClosed(0) ? 1 : 0) +
			    std::to_string(surface->IsClosed(1) ? 1 : 0) +
			    ", periodic " +
			    std::to_string(surface->IsPeriodic(0) ? 1 : 0) +
			    std::to_string(surface->IsPeriodic(1) ? 1 : 0) +
			    ", singular " +
			    std::to_string(surface->IsSingular(0) ? 1 : 0) +
			    std::to_string(surface->IsSingular(1) ? 1 : 0) +
			    std::to_string(surface->IsSingular(2) ? 1 : 0) +
			    std::to_string(surface->IsSingular(3) ? 1 : 0) +
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }
		    if (static_cast<size_t>(points.Count()) >= maximum_points) {
			const std::string periodic_shift_detail =
			    rejected_periodic_shift_distance < DBL_MAX ?
			    std::to_string(rejected_periodic_shift_distance) :
			    std::string("unavailable");
			rejection = "adaptive exact-edge pullback sample budget exceeded at " +
			    std::to_string(normalized_parameters[segment]) + ":" +
			    std::to_string(normalized_parameters[segment + 1]) +
			    " (uv " + std::to_string(points[segment].x) + ":" +
			    std::to_string(points[segment].y) + " -> " +
			    std::to_string(points[segment + 1].x) + ":" +
			    std::to_string(points[segment + 1].y) +
			    ", periodic shift distance " + periodic_shift_detail + ")";
			valid = false;
			break;
		    }

		    const double normalized = parameter_midpoint;
		    const double edge_parameter = edge->Domain().ParameterAt(
			trim.m_bRev3d ? 1.0 - normalized : normalized);
		    const ON_3dPoint edge_point = edge->PointAt(edge_parameter);
		    const double pullback_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale, tolerance);
		    const double solver_tolerance = pullback_solver_tolerance(
			pullback_tolerance);
		    const ON_3dPoint branch_reference = 0.5 *
			(points[segment] + points[segment + 1]);
		    ON_3dPoint uv = branch_reference;
		    bool confine_nonperiodic_closed_surface = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			confine_nonperiodic_closed_surface = true;
			const ON_Interval direction_domain =
			    surface->Domain(direction);
			const double direction_period = direction_domain.Length();
			if (!(direction_period > ON_ZERO_TOLERANCE))
			    continue;
			double wrapped = fmod(uv[direction] -
			    direction_domain.Min(), direction_period);
			if (wrapped < 0.0)
			    wrapped += direction_period;
			uv[direction] = direction_domain.Min() + wrapped;
		    }
		    double pullback_distance = DBL_MAX;
		    if (!refine_surface_pullback_seeded(surface, edge_point,
			    solver_tolerance, uv, &pullback_distance,
			    confine_nonperiodic_closed_surface ||
				!has_unwrapped_closed_parameter(surface, uv),
			    pullback_tolerance)) {
			ON_2dPoint pulled_uv;
			ON_3dPoint pulled_lift;
			if (!pullback_context.SurfaceClosestPoint(surface, edge_point, pulled_uv,
				pulled_lift, pullback_distance, 0,
				solver_tolerance,
				pullback_tolerance) || pullback_distance > pullback_tolerance) {
			    rejection = "adaptive exact-edge pullback failed";
			    valid = false;
			    break;
			}
			uv.Set(pulled_uv.x, pulled_uv.y, 0.0);
		    }
		    const ON_3dPoint source_uv = trim.PointAt(
			trim_domain.ParameterAt(normalized));
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			const double direction_period =
			    surface->Domain(direction).Length();
			if (!(direction_period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += round((source_uv[direction] -
			    uv[direction]) / direction_period) * direction_period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
			if (shifted_lift.IsValid() &&
				shifted_lift.DistanceTo(edge_point) <=
				    pullback_tolerance)
			    uv = shifted_uv;
		    }
		    bool excessive_period_drift = false;
		    for (int direction = 0; direction < 2; ++direction) {
			if (!surface->IsClosed(direction) ||
				surface->IsPeriodic(direction))
			    continue;
			const double direction_period =
			    surface->Domain(direction).Length();
			excessive_period_drift = excessive_period_drift ||
			    (direction_period > ON_ZERO_TOLERANCE &&
			     fabs(uv[direction] - source_uv[direction]) >
				 kMaximumSeededPullbackPeriodDrift * direction_period);
		    }
		    if (excessive_period_drift) {
			ON_2dPoint native_uv;
			ON_3dPoint native_lift;
			double native_distance = DBL_MAX;
			if (pullback_context.SurfaceClosestPoint(surface, edge_point,
				native_uv, native_lift, native_distance, 0,
				solver_tolerance, pullback_tolerance) &&
			    native_distance <= pullback_tolerance)
			    uv.Set(native_uv.x, native_uv.y, 0.0);
		    }
		    for (int direction = 0; direction < 2; ++direction) {
			const double period = surface->Domain(direction).Length();
			if (!(period > ON_ZERO_TOLERANCE))
			    continue;
			ON_3dPoint shifted_uv = uv;
			shifted_uv[direction] += std::round((branch_reference[direction] -
			    uv[direction]) / period) * period;
			const ON_3dPoint shifted_lift = closed_surface_point_at(
			    surface, shifted_uv);
		    if (shifted_lift.IsValid() &&
			    shifted_lift.DistanceTo(edge_point) <= pullback_tolerance)
			uv = shifted_uv;
		    }
		    normalize_closed_surface_parameter(surface, edge_point,
			pullback_tolerance, uv);
		    points.Insert(segment + 1, uv);
		    normalized_parameters.insert(normalized_parameters.begin() +
			segment + 1, normalized);
		    /* Prove the newly created left child before advancing. */
	    }
	    if (!valid) {
		if (failure_reason)
		    *failure_reason = rejection;
		continue;
	    }
	}
	/* A fitted closed pcurve, or an open pcurve whose endpoint was constrained
	 * to close a periodic loop, can have a poor one-sided derivative at that
	 * algebraic endpoint even when its interior follows the edge correctly.
	 * Map the exact 3D edge tangent into the surface parameter plane and add a
	 * short endpoint chord without moving an already validated interior sample.
	 * The validation below still
	 * bounds both the displacement from the source pcurve and the distance to
	 * the exact edge, so this cannot bridge an out-of-tolerance gap. */
	const bool closed_topology = trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	bool ill_conditioned_endpoint[2] = {false, false};
	for (int end = 0; valid && end < 2; ++end) {
	    if (!closed_topology && !(end == 0 ? required_start : required_end))
		continue;
	    const int current_segment_count = points.Count() - 1;
	    const int endpoint_index = end == 0 ? 0 : current_segment_count;
	    int adjacent_index = end == 0 ? 1 : current_segment_count - 1;
	    const ON_3dPoint endpoint_uv = points[endpoint_index];
	    /* Periodic endpoint limiting can produce several parameter samples at
	     * the same representable UV before the curve makes geometric progress.
	     * The cleanup below removes those duplicates, but the endpoint tangent
	     * proof must likewise use the first distinct interior sample instead
	     * of treating its initial zero-length chord as a failed pullback. */
	    const int adjacent_step = end == 0 ? 1 : -1;
	    while (adjacent_index > 0 &&
		    adjacent_index < current_segment_count &&
		    points[adjacent_index].IsCoincident(endpoint_uv))
		adjacent_index += adjacent_step;
	    if (adjacent_index < 0 ||
		    adjacent_index > current_segment_count ||
		    adjacent_index == endpoint_index ||
		    points[adjacent_index].IsCoincident(endpoint_uv)) {
		rejection =
		    "endpoint limiting collapsed every adjacent parameter sample";
		valid = false;
		break;
	    }
	    ON_3dPoint lifted;
	    ON_3dVector du, dv;
	    if (!closed_surface_ev1der(surface, endpoint_uv, lifted, du, dv)) {
		rejection = "endpoint surface derivative evaluation failed";
		valid = false;
		break;
	    }
	    ON_3dVector target = edge->TangentAt(edge->Domain()[
		trim.m_bRev3d ? 1 - end : end]);
	    if (trim.m_bRev3d)
		target.Reverse();
	    if (!target.Unitize()) {
		rejection = "endpoint edge tangent evaluation failed";
		valid = false;
		break;
	    }
	    const ON_3dVector current_uv_tangent = end == 0 ?
		points[adjacent_index] - endpoint_uv :
		endpoint_uv - points[adjacent_index];
	    ON_3dVector current_lifted_tangent = current_uv_tangent.x * du +
		current_uv_tangent.y * dv;
	    if (current_lifted_tangent.Unitize()) {
		const double current_alignment = current_lifted_tangent * target;
		if (current_alignment >= 0.0)
		    continue;
		/* OpenNURBS requires a nonnegative endpoint tangent alignment for a
		 * closed edge use.  Rotate only far enough to cross that sign boundary;
		 * replacing a nearly perpendicular supplied tangent with the complete
		 * edge tangent can move an otherwise exact pcurve beyond tolerance. */
		const double positive_margin = std::max(1.0e-6,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		target = current_lifted_tangent +
		    (-current_alignment + positive_margin) * target;
		if (!target.Unitize()) {
		    rejection = "endpoint tangent correction failed";
		    valid = false;
		    break;
		}
	    }
	    const double a = du * du;
	    const double b = du * dv;
	    const double c = dv * dv;
	    const double r0 = du * target;
	    const double r1 = dv * target;
	    const double determinant = a * c - b * b;
	    const double numerical_floor = ON_ZERO_TOLERANCE *
		std::max(1.0, a * c);
	    if (fabs(determinant) <= numerical_floor) {
		/* Some analytic revolution surfaces do not report their apex via
		 * IsAtSingularity(), but the rank-deficient Jacobian proves that an
		 * endpoint parameter tangent is not geometrically meaningful there.
		 * Preserve the exact endpoint and rely on the dense interior direction
		 * proof outside the guarded endpoint neighborhood. */
		ill_conditioned_endpoint[end] = true;
		continue;
	    }
	    ON_3dVector uv_tangent((c * r0 - b * r1) / determinant,
		(a * r1 - b * r0) / determinant, 0.0);
	    const ON_3dPoint adjacent_lift = closed_surface_point_at(surface,
		points[adjacent_index]);
	    const double chord_length = lifted.DistanceTo(adjacent_lift);
	    const ON_3dVector lifted_uv_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    const double tangent_scale = lifted_uv_tangent.Length();
	    if (!adjacent_lift.IsValid() || chord_length <= ON_ZERO_TOLERANCE ||
		    tangent_scale <= ON_ZERO_TOLERANCE) {
		rejection = "degenerate endpoint parameter tangent";
		valid = false;
		break;
	    }
	    uv_tangent *= 0.25 * chord_length / tangent_scale;
	    const ON_3dPoint tangent_point = end == 0 ?
		endpoint_uv + uv_tangent : endpoint_uv - uv_tangent;
	    const double tangent_parameter = end == 0 ?
		0.75 * normalized_parameters[endpoint_index] +
		    0.25 * normalized_parameters[adjacent_index] :
		0.25 * normalized_parameters[adjacent_index] +
		    0.75 * normalized_parameters[endpoint_index];
	    const int insertion_index = end == 0 ? 1 : endpoint_index;
	    points.Insert(insertion_index, tangent_point);
		    normalized_parameters.insert(normalized_parameters.begin() +
			insertion_index, tangent_parameter);
		}
		/* Periodic endpoint limiting can consume several successively refined
		 * samples whose normalized parameters differ but whose UV values have
		 * reached the same machine-precision limit.  Endpoint-tangent insertion
		 * may expose that duplicate pair.  Remove only zero-length consecutive
		 * samples, retaining a constrained algebraic endpoint when it is one of
		 * the pair; all remaining segments undergo the full validation below. */
		for (int point_index = 1; point_index < points.Count();) {
		    if (!points[point_index - 1].IsCoincident(
			    points[point_index])) {
			++point_index;
			continue;
		    }
		    const int remove_index = point_index == points.Count() - 1 ?
			point_index - 1 : point_index;
		    points.Remove(remove_index);
		    normalized_parameters.erase(normalized_parameters.begin() +
			remove_index);
		    point_index = std::max(1, remove_index);
		}
		if (points.Count() < 2) {
		    rejection = "periodic endpoint limiting collapsed the pcurve";
		    valid = false;
		}
		if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " segments";
	    continue;
	}
	const int candidate_segment_count = points.Count() - 1;
	bool used_directed_chord_direction = false;

	/* Validate both geometry and direction at every new vertex and at interior
	 * points of every segment.  Comparison with the original lift bounds the
	 * repair itself; comparison with the edge verifies that no narrow invalid
	 * source deviation can hide exactly at a sampling vertex.  Surface and
	 * curve evaluation dominates this proof and each sample is independent, so
	 * let an otherwise idle geometry helper evaluate samples in parallel.  All
	 * acceptance decisions remain in deterministic parameter order below. */
	struct DenseValidationSample {
	    double normalized = 0.0;
	    ON_3dPoint candidate_uv = ON_3dPoint::UnsetPoint;
	    ON_3dPoint candidate_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint original_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint edge_point = ON_3dPoint::UnsetPoint;
	    ON_3dVector du = ON_3dVector::UnsetVector;
	    ON_3dVector dv = ON_3dVector::UnsetVector;
	    bool evaluated = false;
	    bool edge_locus_proven = false;
	};
	const size_t dense_sample_count =
	    static_cast<size_t>(candidate_segment_count) * 4;
	std::vector<DenseValidationSample> dense_samples(dense_sample_count);
	const auto evaluate_dense_sample = [&](size_t dense_index) {
	    if (brlcad::PullbackWorkCancelled())
		return;
	    const int segment = static_cast<int>(dense_index / 4);
	    const int sub = static_cast<int>(dense_index % 4);
	    const double fraction = static_cast<double>(sub) / 4.0;
	    DenseValidationSample &sample = dense_samples[dense_index];
	    sample.normalized = (1.0 - fraction) *
		normalized_parameters[segment] + fraction *
		normalized_parameters[segment + 1];
	    sample.candidate_uv = (1.0 - fraction) * points[segment] +
		fraction * points[segment + 1];
	    if (!closed_surface_ev1der(surface, sample.candidate_uv,
		    sample.candidate_lift, sample.du, sample.dv))
		return;
	    const ON_3dPoint original_uv = trim.PointAt(
		trim_domain.ParameterAt(sample.normalized));
	    sample.original_lift = closed_surface_point_at(surface, original_uv);
	    sample.edge_point = edge->PointAt(edge->Domain().ParameterAt(
		trim.m_bRev3d ? 1.0 - sample.normalized : sample.normalized));
	    sample.evaluated = sample.candidate_lift.IsValid() &&
		sample.original_lift.IsValid() && sample.edge_point.IsValid();
	};
	if (wrapper)
	    wrapper->ParallelForGeometry(dense_sample_count, evaluate_dense_sample);
	else
	    for (size_t dense_index = 0; dense_index < dense_sample_count;
		    ++dense_index)
		evaluate_dense_sample(dense_index);
	if (brlcad::PullbackWorkCancelled()) {
	    if (failure_reason)
		*failure_reason = "dense regenerated-pcurve validation was cancelled";
	    return false;
	}
	/* Source-driven regeneration preserves the supplied pcurve's parameter
	 * speed, which need not match the independently parameterized STEP edge.
	 * The old validation called ON_NurbsCurve_GetClosestPoint separately for
	 * every mismatched dense sample.  On a long spline edge that repeatedly
	 * rebuilt and globally searched the same knot spans thousands of times
	 * after sampling had visibly reached 1024/1024.
	 *
	 * Prove the same curve-locus condition as one ordered batch.  The shared
	 * evaluator brackets every exact NURBS span once, uses conservative span
	 * boxes (and an R-tree for large curves), and reuses the preceding accepted
	 * span as the next seed.  No tolerance or sampling decision changes. */
	if (!edge_driven) {
	    std::vector<ON_3dPoint> locus_points;
	    std::vector<size_t> locus_dense_indices;
	    locus_points.reserve(dense_samples.size());
	    locus_dense_indices.reserve(dense_samples.size());
	    for (size_t dense_index = 0; dense_index < dense_samples.size();
		    ++dense_index) {
		DenseValidationSample &sample = dense_samples[dense_index];
		if (!sample.evaluated)
		    continue;
		const double direct_distance =
		    sample.candidate_lift.DistanceTo(sample.edge_point);
		if (direct_distance <= tolerance) {
		    sample.edge_locus_proven = true;
		    continue;
		}
		locus_points.push_back(sample.candidate_lift);
		locus_dense_indices.push_back(dense_index);
	    }
	    if (!locus_points.empty()) {
		if (wrapper) {
		    std::ostringstream detail;
		    detail << "trim=T" << trim.m_trim_index << " points="
			<< locus_points.size() << " edge=STEP#"
			<< edge->m_edge_user.i << " edge-degree="
			<< edge_nurbs.Degree() << " edge-cvs="
			<< edge_nurbs.CVCount() << " edge-spans="
			<< edge_nurbs.SpanCount();
		    wrapper->SetProgressDetail(
			"validating regenerated pcurve against exact edge locus",
			0, 0, static_cast<uint64_t>(locus_points.size()),
			"points", detail.str());
		}
		size_t rejected_index = 0;
		double rejected_distance = DBL_MAX;
		const std::function<void(size_t, size_t, const std::string &)>
		    locus_progress =
		    [wrapper, &trim, edge, &edge_nurbs](size_t completed,
			    size_t total, const std::string &stage) {
			if (!wrapper)
			    return;
			std::ostringstream detail;
			detail << "trim=T" << trim.m_trim_index
			    << " edge=STEP#" << edge->m_edge_user.i
			    << " edge-degree=" << edge_nurbs.Degree()
			    << " edge-cvs=" << edge_nurbs.CVCount()
			    << " edge-spans=" << edge_nurbs.SpanCount();
			if (!stage.empty())
			    detail << " stage=\"" << stage << '"';
			wrapper->SetProgressDetail(
			    "validating regenerated pcurve against exact edge locus",
			    0, static_cast<uint64_t>(completed),
			    static_cast<uint64_t>(total), "points",
			    detail.str());
		    };
		if (!step_curve_locus_contains_points(&edge_nurbs,
			locus_points.data(), locus_points.size(), tolerance,
			&rejected_index, &rejected_distance,
			locus_progress)) {
		    const size_t dense_index = rejected_index <
			locus_dense_indices.size() ?
			locus_dense_indices[rejected_index] : 0;
		    const double normalized = dense_index < dense_samples.size() ?
			dense_samples[dense_index].normalized : 0.0;
		    rejection = "regenerated lift exceeded edge tolerance "
			"(batched locus distance " +
			std::to_string(rejected_distance) + ", tolerance " +
			std::to_string(tolerance) + ", normalized " +
			std::to_string(normalized) + ")";
		    valid = false;
		} else {
		    for (std::vector<size_t>::const_iterator dense_index =
			    locus_dense_indices.begin();
			    dense_index != locus_dense_indices.end();
			    ++dense_index)
			dense_samples[*dense_index].edge_locus_proven = true;
		}
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " +
		    std::to_string(segment_count) + " initial segments (" +
		    std::to_string(candidate_segment_count) +
		    " final segments)";
	    continue;
	}
	for (int segment = 0; valid && segment < candidate_segment_count; ++segment) {
	    const ON_3dVector uv_tangent = points[segment + 1] - points[segment];
	    if (uv_tangent.Length() <= ON_ZERO_TOLERANCE) {
		rejection = "zero-length regenerated pcurve segment " +
		    std::to_string(segment) + " (normalized " +
		    std::to_string(normalized_parameters[segment]) + ":" +
		    std::to_string(normalized_parameters[segment + 1]) + ", uv " +
		    std::to_string(points[segment].x) + ":" +
		    std::to_string(points[segment].y) + " -> " +
		    std::to_string(points[segment + 1].x) + ":" +
		    std::to_string(points[segment + 1].y) + ")";
		valid = false;
		break;
	    }
	    for (int sub = 0; sub <= 3; ++sub) {
		const DenseValidationSample &sample = dense_samples[
		    static_cast<size_t>(segment) * 4 + sub];
		const double normalized = sample.normalized;
		const ON_3dPoint &candidate_uv = sample.candidate_uv;
		const ON_3dPoint &candidate_lift = sample.candidate_lift;
		const ON_3dPoint &original_lift = sample.original_lift;
		const ON_3dVector &du = sample.du;
		const ON_3dVector &dv = sample.dv;
		if (!sample.evaluated) {
		    rejection = "surface lift failed";
		    valid = false;
		    break;
		}
		const double source_lift_distance =
		    candidate_lift.DistanceTo(original_lift);
		if (!edge_driven && source_lift_distance > tolerance) {
		    rejection = "regenerated lift exceeded source pcurve tolerance"
			" (distance " + std::to_string(source_lift_distance) +
			", tolerance " + std::to_string(tolerance) +
			", normalized " + std::to_string(normalized) +
			", segment uv " + std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ")";
		    valid = false;
		    break;
		}
		double edge_parameter = 0.0;
		double edge_distance = DBL_MAX;
		edge_parameter = edge->Domain().ParameterAt(
		    trim.m_bRev3d ? 1.0 - normalized : normalized);
		edge_distance = candidate_lift.DistanceTo(sample.edge_point);
		bool edge_proxy_parameter = true;
		bool edge_locus_proven = sample.edge_locus_proven ||
		    edge_distance <= tolerance;
		/* A trim and its 3-D edge describe the same directed locus, but are not
		 * required to advance at the same normalized parameter speed.  The
		 * regenerated polyline changes that speed locally, so a synchronized
		 * comparison can report a false geometric gap.  A synchronized point
		 * already inside the acceptance tolerance is also a constructive proof
		 * of edge-locus membership, however, and the global NURBS optimizer
		 * cannot change that decision.  Run the expensive closest-point solve
		 * only when the direct proof fails; the tangent test below still proves
		 * traversal direction. */
		if (!edge_locus_proven && edge_driven) {
		    double closest_parameter = 0.0;
		    if (ON_NurbsCurve_GetClosestPoint(&closest_parameter, &edge_nurbs,
			    candidate_lift)) {
			const double closest_distance = candidate_lift.DistanceTo(
			    edge_nurbs.PointAt(closest_parameter));
			if (closest_distance < edge_distance) {
			    edge_parameter = closest_parameter;
			    edge_distance = closest_distance;
			    edge_proxy_parameter = false;
			}
		    }
		    edge_locus_proven = edge_distance <= tolerance;
		}
		if (!edge_locus_proven) {
		    double original_parameter = 0.0;
		    double original_edge_distance = ON_UNSET_VALUE;
		    if (ON_NurbsCurve_GetClosestPoint(&original_parameter, &edge_nurbs,
			    original_lift))
		original_edge_distance = original_lift.DistanceTo(
			edge_nurbs.PointAt(original_parameter));
		    rejection = "regenerated lift exceeded edge tolerance (distance " +
			std::to_string(edge_distance) + ", source " +
			std::to_string(original_edge_distance) + ", tolerance " +
			std::to_string(tolerance) + ", normalized " +
			std::to_string(normalized) + ", segment uv " +
			std::to_string(points[segment].x) + ":" +
			std::to_string(points[segment].y) + " -> " +
			std::to_string(points[segment + 1].x) + ":" +
			std::to_string(points[segment + 1].y) + ", candidate uv " +
			std::to_string(candidate_uv.x) + ":" +
			std::to_string(candidate_uv.y) + ")";
		    valid = false;
		    break;
		}
		if (singular_parameter_connectors.find(std::make_pair(
			normalized_parameters[segment],
			normalized_parameters[segment + 1])) !=
			singular_parameter_connectors.end())
		    continue;
		if (normalized <= kEndpointDirectionGuardFraction ||
			normalized >= 1.0 - kEndpointDirectionGuardFraction)
		    continue;
		ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
		if (!edge_driven) {
		    ON_3dPoint source_uv, source_lift;
		    ON_3dVector source_uv_tangent, source_du, source_dv;
		    const double source_parameter = trim_domain.ParameterAt(normalized);
		    if (!trim.Ev1Der(source_parameter, source_uv, source_uv_tangent) ||
			    !closed_surface_ev1der(surface, source_uv, source_lift,
				source_du, source_dv)) {
			rejection = "source pcurve tangent evaluation failed";
			valid = false;
			break;
		    }
		    ON_3dVector source_tangent = source_uv_tangent.x * source_du +
			source_uv_tangent.y * source_dv;
		    if (!lifted_tangent.Unitize() || !source_tangent.Unitize() ||
			    lifted_tangent * source_tangent <= 0.0) {
			rejection = "regenerated interior direction disagreed with source pcurve";
			valid = false;
			break;
		    }
		    continue;
		}
		ON_3dVector edge_tangent = edge_proxy_parameter ?
		    edge->TangentAt(edge_parameter) : edge_nurbs.TangentAt(edge_parameter);
		if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		    rejection = "regenerated tangent evaluation failed";
		    valid = false;
		    break;
		}
		double alignment = lifted_tangent * edge_tangent;
		if (trim.m_bRev3d)
		    alignment = -alignment;
		if (alignment <= 0.0) {
		    /* A bounded analytic edge proxy may report the tangent of its
		     * underlying or complementary circle branch at an interior
		     * parameter, even though the bounded edge traversal itself is
		     * unambiguous.  Do not simply waive the direction test:
		     * corroborate it with the directed chord between this adaptive
		     * segment's two exact edge samples.  The candidate lift has
		     * already passed dense locus validation, and a genuinely
		     * reversed/complementary candidate gives a negative chord
		     * alignment and remains rejected. */
		    const double first_normalized =
			normalized_parameters[segment];
		    const double second_normalized =
			normalized_parameters[segment + 1];
		    const ON_3dPoint first_lift =
			closed_surface_point_at(surface, points[segment]);
		    const ON_3dPoint second_lift =
			closed_surface_point_at(surface, points[segment + 1]);
		    const ON_3dPoint first_edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - first_normalized : first_normalized));
		    const ON_3dPoint second_edge_point = edge->PointAt(
			edge->Domain().ParameterAt(trim.m_bRev3d ?
			    1.0 - second_normalized : second_normalized));
		    ON_3dVector lifted_chord = second_lift - first_lift;
		    ON_3dVector directed_edge_chord =
			second_edge_point - first_edge_point;
		    const bool chord_valid = first_lift.IsValid() &&
			second_lift.IsValid() && first_edge_point.IsValid() &&
			second_edge_point.IsValid() &&
			lifted_chord.Unitize() && directed_edge_chord.Unitize();
		    const double chord_alignment = chord_valid ?
			lifted_chord * directed_edge_chord : -DBL_MAX;
		    if (!chord_valid || chord_alignment <= 0.0) {
			rejection =
			    "regenerated interior direction disagreed with edge"
			    " (segment " + std::to_string(segment) +
			    ", normalized " + std::to_string(normalized) +
			    ", tangent alignment " + std::to_string(alignment) +
			    ", directed chord alignment " +
			    std::to_string(chord_alignment) + ", reversed " +
			    std::to_string(trim.m_bRev3d ? 1 : 0) + ")";
			valid = false;
			break;
		    }
		    used_directed_chord_direction = true;
		}
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    continue;
	}

	ON_SimpleArray<double> candidate_parameters;
	candidate_parameters.Reserve(static_cast<int>(normalized_parameters.size()));
	for (std::vector<double>::const_iterator normalized =
		normalized_parameters.begin(); normalized != normalized_parameters.end();
	     ++normalized)
	    candidate_parameters.Append(trim_domain.ParameterAt(*normalized));
	ON_PolylineCurve *candidate = new ON_PolylineCurve(points,
	    candidate_parameters);
	if (!candidate->ChangeDimension(2) || !candidate->IsValid()) {
	    delete candidate;
	    /* Endpoint limiting and duplicate removal can leave a valid UV locus
	     * with inherited edge parameters that are no longer strictly
	     * increasing at machine precision.  Reparameterize that unchanged
	     * locus monotonically over the authoritative trim domain.  The dense
	     * surface/edge and directed endpoint validation below still decides
	     * whether this representation is admissible. */
	    candidate = new ON_PolylineCurve(points);
	    if (!candidate->ChangeDimension(2) ||
		    !candidate->SetDomain(trim_domain.Min(),
			trim_domain.Max()) ||
		    !candidate->IsValid()) {
		if (failure_reason)
		    *failure_reason =
			"regenerated polyline locus or parameterization is invalid";
		delete candidate;
		continue;
	    }
	}
	/* Callers which request a particular periodic image are constructing an
	 * atomic loop or seam candidate.  Returning a lift-equivalent endpoint on
	 * another image silently moves the noncontractible cut to the adjacent
	 * join and defeats that transaction.  The adaptive solver may replace an
	 * unconstrained endpoint by its continuous limiting image, but an
	 * explicitly preserved image is a literal postcondition. */
	if (preserve_required_uv_images &&
		((required_start && candidate->PointAtStart().DistanceTo(
		    *required_start) > ON_ZERO_TOLERANCE) ||
		 (required_end && candidate->PointAtEnd().DistanceTo(
		    *required_end) > ON_ZERO_TOLERANCE))) {
	    if (failure_reason) {
		const ON_3dPoint actual_start = candidate->PointAtStart();
		const ON_3dPoint actual_end = candidate->PointAtEnd();
		std::ostringstream detail;
		detail << "regenerated pcurve did not preserve its required "
		    "periodic endpoint image";
		if (required_start)
		    detail << " start=" << actual_start.x << ':'
			<< actual_start.y << " required=" << required_start->x
			<< ':' << required_start->y;
		if (required_end)
		    detail << " end=" << actual_end.x << ':' << actual_end.y
			<< " required=" << required_end->x << ':'
			<< required_end->y;
		*failure_reason = detail.str();
	    }
	    delete candidate;
	    continue;
	}
	if (trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "regenerated open-topology pcurve remained closed";
	    delete candidate;
	    continue;
	}
	const ON_Interval candidate_domain = candidate->Domain();
	for (int end = 0; valid && end < 2; ++end) {
	    ON_3dPoint uv, lifted_point;
	    ON_3dVector uv_tangent, du, dv;
	    if (!candidate->Ev1Der(candidate_domain[end], uv, uv_tangent) ||
		    !closed_surface_ev1der(surface, uv, lifted_point, du, dv)) {
		rejection = "regenerated endpoint evaluation failed";
		valid = false;
		break;
	    }
	    const int vertex_index = trim.m_vi[end];
	    const ON_3dPoint edge_endpoint = edge->PointAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!edge_endpoint.IsValid() ||
		    lifted_point.DistanceTo(edge_endpoint) > tolerance ||
		    vertex_index < 0 || vertex_index >= brep->m_V.Count() ||
		    lifted_point.DistanceTo(brep->m_V[vertex_index].point) > tolerance) {
		rejection = "regenerated endpoint exceeded exact topology tolerance";
		valid = false;
		break;
	    }
	    /* At a collapsed surface boundary the endpoint lift is exact, but its
	     * differential is not unique.  Interior samples above already prove
	     * traversal direction outside the guarded pole neighborhood. */
	    const ON_3dPoint native_uv = closed_surface_native_parameter(surface,
		uv);
	    if ((native_uv.IsValid() && surface->IsAtSingularity(native_uv.x,
		    native_uv.y, false)) ||
		    ill_conditioned_endpoint[end])
		continue;
	    ON_3dVector lifted_tangent = uv_tangent.x * du + uv_tangent.y * dv;
	    ON_3dVector edge_tangent = edge->TangentAt(
		edge->Domain()[trim.m_bRev3d ? 1 - end : end]);
	    if (!lifted_tangent.Unitize() || !edge_tangent.Unitize()) {
		rejection = "regenerated endpoint tangent evaluation failed";
		valid = false;
		break;
	    }
	    double alignment = lifted_tangent * edge_tangent;
	    if (trim.m_bRev3d)
		alignment = -alignment;
	    if (alignment < 0.0) {
		rejection = "regenerated endpoint direction disagreed with edge "
		    "(endpoint " + std::to_string(end) + ", alignment " +
		    std::to_string(alignment) + ", uv " + std::to_string(uv.x) +
		    ":" + std::to_string(uv.y) + ", singular " +
		    std::to_string(native_uv.IsValid() && surface->IsAtSingularity(
			native_uv.x, native_uv.y, false) ?
			1 : 0) + ")";
		valid = false;
	    }
	}
	if (!valid) {
	    if (failure_reason)
		*failure_reason = rejection + " at " + std::to_string(segment_count) +
		    " initial segments (" + std::to_string(candidate_segment_count) +
		    " final segments)";
	    delete candidate;
	    continue;
	}
	/* Installing an open, full-period pcurve into a one-trim closed-topology
	 * loop leaves that loop open in Euclidean UV and is structurally invalid in
	 * OpenNURBS, even though the endpoint lifts coincide on the closed surface.
	 * Periodic-band and pole-cut builders request the generated curve through
	 * generated_curve and install the required paired seam topology atomically;
	 * an ordinary in-place orientation repair has no authority to do so. */
	const ON_BrepLoop *owner_loop = trim.Loop();
	if (!generated_curve && trim.m_vi[0] == trim.m_vi[1] && owner_loop &&
		owner_loop->TrimCount() == 1 && !candidate->IsClosed()) {
	    if (failure_reason)
		*failure_reason = "regenerated standalone closed-topology pcurve "
		    "requires explicit periodic seam topology";
	    delete candidate;
	    continue;
	}
	const auto record_directed_chord_proof = [wrapper, edge,
		used_directed_chord_direction]() {
	    if (wrapper && edge && used_directed_chord_direction)
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Information,
		    edge->m_edge_user.i, "EDGE_CURVE", "edge_geometry",
		    "resolved a contradictory analytic proxy tangent using "
		    "directed exact-edge chords");
	};
	if (generated_curve) {
	    *generated_curve = candidate;
	    record_directed_chord_proof();
	    return true;
	}
	const int c2_index = brep->AddTrimCurve(candidate);
	if (c2_index < 0 || !brep->SetTrimCurve(trim, c2_index))
	    return false;
	const ON_Interval proxy_domain = trim.ProxyCurveDomain();
	trim.m_iso = surface->IsIsoparametric(*candidate, &proxy_domain);
	record_directed_chord_proof();
	return true;
    }
    if (prefer_edge_driven && edge_driven && failure_reason)
	edge_driven_failure = *failure_reason;
    }
    if (prefer_edge_driven && failure_reason && !edge_driven_failure.empty() &&
	    *failure_reason != edge_driven_failure)
	*failure_reason = "exact-edge mode: " + edge_driven_failure +
	    "; supplied-pcurve mode: " + *failure_reason;
    return false;
}


bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool record_repair)
{
    if (!brep || !wrapper || loop_index < 0 ||
	    loop_index >= brep->m_L.Count())
	return false;
    const ON_BrepLoop &source_loop = brep->m_L[loop_index];
    const ON_BrepFace *source_face = source_loop.Face();
    const ON_Surface *source_surface = source_face ?
	source_face->SurfaceOf() : NULL;
    if (!source_surface || !source_surface->IsClosed(closed_direction) ||
	    source_loop.TrimCount() < 2)
	return false;
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = source_surface->Domain(closed_direction);
    if (!closed_domain.IsIncreasing())
	return false;
    const double period = closed_domain.Length();
    /* A source pcurve may cross a periodic seam inside one trim and store its
     * final endpoint on an equivalent wrapped image.  Looking only at raw
     * endpoints then turns a real +/-period chain into a zero-net chain, and
     * unconstrained edge pullback can choose the shorter complementary arc.
     *
     * Measure each supplied trim's continuous parameter travel before
     * regenerating anything.  Use those per-trim endpoint images only when
     * their complete chain proves exactly one winding.  They are constraints,
     * not acceptance evidence: regenerate_trim_polyline() must still densely
     * prove every constrained pcurve against its directed immutable 3-D edge
     * and surface before this transaction can commit. */
    std::map<int, ON_3dVector> supplied_trim_travel;
    ON_3dVector supplied_chain_travel(0.0, 0.0, 0.0);
    bool supplied_travel_valid = true;
    for (int lti = 0; supplied_travel_valid &&
	    lti < source_loop.TrimCount(); ++lti) {
	const ON_BrepTrim *trim = source_loop.Trim(lti);
	const ON_Interval trim_domain = trim ? trim->Domain() :
	    ON_Interval::EmptyInterval;
	if (!trim || !trim_domain.IsIncreasing()) {
	    supplied_travel_valid = false;
	    break;
	}
	const int samples = std::min(256, std::max(
	    kPcurveLocusScreeningSegments, trim->SpanCount() * 4));
	ON_3dPoint first = ON_3dPoint::UnsetPoint;
	ON_3dPoint previous = ON_3dPoint::UnsetPoint;
	for (int sample = 0; sample <= samples; ++sample) {
	    ON_3dPoint point = trim->PointAt(trim_domain.ParameterAt(
		static_cast<double>(sample) / samples));
	    if (!point.IsValid()) {
		supplied_travel_valid = false;
		break;
	    }
	    if (sample > 0) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!source_surface->IsClosed(direction))
			continue;
		    const double direction_period =
			source_surface->Domain(direction).Length();
		    if (direction_period > ON_ZERO_TOLERANCE)
			point[direction] += round((previous[direction] -
			    point[direction]) / direction_period) *
			    direction_period;
		}
	    } else {
		first = point;
	    }
	    previous = point;
	}
	if (!supplied_travel_valid)
	    break;
	const ON_3dVector travel = previous - first;
	supplied_trim_travel[trim->m_trim_index] = travel;
	supplied_chain_travel += travel;
    }
    const bool supplied_full_period_winding = supplied_travel_valid &&
	fabs(fabs(supplied_chain_travel[closed_direction]) - period) <=
	    parameter_tolerance;
    /* Most segmented periodic boundaries already have an exact pcurve; only
     * its integral-period image is inconsistent with the neighboring trim.
     * Re-pulling such a curve point by point is both unnecessary and very
     * expensive on rational spline surfaces.  Translate and, when required,
     * snap its constrained endpoints in UV, then accept it only if dense
     * parameter-corresponding samples still follow the authoritative 3-D
     * edge within topology tolerance.  A failed proof falls through to the
     * general edge-driven pullback below. */
    const auto translated_exact_pcurve = [surface = source_surface,
	    closed_direction, parameter_tolerance](ON_BrepTrim &trim,
	    const ON_BrepEdge &edge,
	    double tolerance, const ON_3dPoint &required_start,
	    const ON_3dPoint *required_end, ON_Curve **candidate_out,
	    std::string *failure_out) {
	if (candidate_out) *candidate_out = NULL;
	if (failure_out) failure_out->clear();
	const auto reject = [failure_out](const char *reason) {
	    if (failure_out) *failure_out = reason;
	    return false;
	};
	std::unique_ptr<ON_Curve> candidate(trim.DuplicateCurve());
	if (!candidate || !candidate->Domain().IsIncreasing() ||
		!edge.Domain().IsIncreasing())
	    return reject("invalid trim or edge domain");
	/* DuplicateCurve() may retain an ON_CurveProxy.  Materialize its exact
	 * NURBS form before applying a private period translation or endpoint
	 * edit; transforming a proxy can fail or mutate its shared target. */
	ON_NurbsCurve detached_candidate;
	if (!candidate->GetNurbForm(detached_candidate) ||
		!detached_candidate.IsValid())
	    return reject("could not materialize the trim pcurve");
	candidate.reset(new ON_NurbsCurve(detached_candidate));
	ON_Xform translation(ON_Xform::IdentityTransformation);
	bool translated = false;
	const ON_3dPoint original_start = candidate->PointAtStart();
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double direction_period = surface->Domain(direction).Length();
	    if (!(direction_period > ON_ZERO_TOLERANCE))
		continue;
	    const double shift = round((required_start[direction] -
		original_start[direction]) / direction_period) * direction_period;
	    if (fabs(shift) <= ON_ZERO_TOLERANCE)
		continue;
	    translation.m_xform[direction][3] = shift;
	    translated = true;
	}
	if (translated && !candidate->Transform(translation))
	    return reject("could not apply an integral-period translation");
	if (!candidate->ChangeDimension(2) || !candidate->IsValid())
	    return reject("translated trim pcurve was invalid");
	const ON_Interval edge_domain = edge.Domain();
	const ON_3dPoint required_start_lift =
	    closed_surface_point_at(surface, required_start);
	const ON_3dPoint edge_start = edge.PointAt(
	    edge_domain[trim.m_bRev3d ? 1 : 0]);
	if (!required_start.IsValid() || !required_start_lift.IsValid() ||
		!edge_start.IsValid() ||
		required_start_lift.DistanceTo(edge_start) > tolerance)
	    return reject("required start did not lift to the edge endpoint");
	const ON_3dPoint candidate_start = candidate->PointAtStart();
	if (!candidate_start.IsValid())
	    return reject("could not constrain the pcurve start");
	ON_3dPoint candidate_end = candidate->PointAtEnd();
	if (!candidate_end.IsValid())
	    return reject("could not evaluate the pcurve end");
	const bool prepend_connector = candidate_start.DistanceTo(required_start) >
	    ON_ZERO_TOLERANCE;
	bool append_connector = false;
	if (required_end) {
	    const ON_3dPoint required_end_lift =
		closed_surface_point_at(surface, *required_end);
	    const ON_3dPoint edge_end = edge.PointAt(
		edge_domain[trim.m_bRev3d ? 0 : 1]);
	    if (!required_end->IsValid() || !required_end_lift.IsValid() ||
		    !edge_end.IsValid() ||
		    required_end_lift.DistanceTo(edge_end) > tolerance)
		return reject("required end did not lift to the edge endpoint");
	    append_connector = candidate_end.DistanceTo(*required_end) >
		ON_ZERO_TOLERANCE;
	}
	if (prepend_connector || append_connector) {
	    std::unique_ptr<ON_PolyCurve> joined(new ON_PolyCurve());
	    joined->Reserve(1 + (prepend_connector ? 1 : 0) +
		(append_connector ? 1 : 0));
	    if (prepend_connector) {
		ON_LineCurve *connector = new ON_LineCurve(required_start,
		    candidate_start);
		if (!connector->ChangeDimension(2) || !connector->IsValid() ||
			!joined->Append(connector)) {
		    delete connector;
		    return reject("could not prepend an exact endpoint connector");
		}
	    }
	    ON_Curve *middle = candidate.release();
	    if (!joined->Append(middle)) {
		delete middle;
		return reject("could not append the existing pcurve segment");
	    }
	    if (append_connector) {
		ON_LineCurve *connector = new ON_LineCurve(candidate_end,
		    *required_end);
		if (!connector->ChangeDimension(2) || !connector->IsValid() ||
			!joined->Append(connector)) {
		    delete connector;
		    return reject("could not append an exact endpoint connector");
		}
	    }
	    if (!joined->SetDomain(trim.Domain().Min(), trim.Domain().Max()) ||
		    !joined->IsValid())
		return reject("joined pcurve connector was invalid");
	    candidate.reset(joined.release());
	}
	/* Model tolerance is an acceptance bound for the supplied locus, not
	 * permission to erase a short but intentional topological edge.  In
	 * particular, prepending a tolerance-sized connector to an otherwise
	 * open child pcurve can make the resulting UV curve exactly closed even
	 * though the native-seam split gave the trim two distinct vertices.
	 * OpenNURBS rejects that representation, and accepting it here would
	 * collapse source geometry merely because the edge is shorter than the
	 * asserted uncertainty.  Let the edge-driven generator seek a genuinely
	 * open parameterization instead. */
	if (trim.m_vi[0] >= 0 && trim.m_vi[1] >= 0 &&
		trim.m_vi[0] != trim.m_vi[1] && candidate->IsClosed())
	    return reject("candidate collapsed an open topology edge");
	ON_NurbsCurve edge_nurbs;
	if (!edge.GetNurbForm(edge_nurbs))
	    return reject("could not materialize the edge curve");
	const auto validate_exact_locus = [surface, &trim, &edge, &edge_nurbs,
		&edge_domain, tolerance](const ON_Curve &curve,
		std::string *locus_failure) {
	    const ON_Interval curve_domain = curve.Domain();
	    double previous_directed_parameter = 0.0;
	    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
		    if (locus_failure)
			*locus_failure = "dense exact-locus validation was cancelled";
		    return false;
		}
		const double fraction = static_cast<double>(sample) /
		    kDenseValidationSegments;
		const ON_3dPoint uv = curve.PointAt(
		    curve_domain.ParameterAt(fraction));
		const ON_3dPoint lift = uv.IsValid() ?
		    closed_surface_point_at(surface, uv) :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint edge_point = edge.PointAt(
		    edge_domain.ParameterAt(trim.m_bRev3d ?
			1.0 - fraction : fraction));
		if (!lift.IsValid() || !edge_point.IsValid()) {
		    if (locus_failure)
			*locus_failure = "dense exact-locus evaluation was invalid";
		    return false;
		}
		double distance = lift.DistanceTo(edge_point);
		double directed_parameter = fraction;
		if (distance > tolerance) {
		    double closest_parameter = 0.0;
		    if (!ON_NurbsCurve_GetClosestPoint(&closest_parameter,
			    &edge_nurbs, lift)) {
			if (locus_failure)
			    *locus_failure = "closest edge-locus validation failed";
			return false;
		    }
		    distance = lift.DistanceTo(edge_nurbs.PointAt(closest_parameter));
		    directed_parameter = edge_nurbs.Domain().NormalizedParameterAt(
			closest_parameter);
		    if (trim.m_bRev3d)
			directed_parameter = 1.0 - directed_parameter;
		}
		if (distance > tolerance ||
			directed_parameter + kPeriodicParameterSnapFraction <
			previous_directed_parameter) {
		    if (locus_failure) {
			std::ostringstream reason;
			if (distance > tolerance)
			    reason << "candidate pcurve left the exact edge locus at "
				<< sample << '/' << kDenseValidationSegments
				<< " distance=" << distance << " tolerance="
				<< tolerance << " uv=" << uv.x << ':' << uv.y;
			else
			    reason << "candidate pcurve reversed along the edge locus at "
				<< sample << '/' << kDenseValidationSegments
				<< " parameter=" << directed_parameter
				<< " previous=" << previous_directed_parameter;
			*locus_failure = reason.str();
		    }
		    return false;
		}
		previous_directed_parameter = directed_parameter;
	    }
	    return true;
	};
	std::string locus_failure;
	if (!validate_exact_locus(*candidate, &locus_failure)) {
	    const ON_3dPoint exact_end = required_end ? *required_end :
		candidate_end;
	    const int fixed_direction = 1 - closed_direction;
	    if (!exact_end.IsValid() || fabs(required_start[fixed_direction] -
		    exact_end[fixed_direction]) > parameter_tolerance)
		return reject(locus_failure.c_str());
	    std::unique_ptr<ON_LineCurve> exact_iso(new ON_LineCurve(
		required_start, exact_end));
	    if (!exact_iso->ChangeDimension(2) ||
		    !exact_iso->SetDomain(trim.Domain().Min(), trim.Domain().Max()) ||
		    !exact_iso->IsValid() ||
		    !validate_exact_locus(*exact_iso, &locus_failure))
		return reject(locus_failure.empty() ?
		    "exact isoparametric pcurve was invalid" :
		    locus_failure.c_str());
	    candidate.reset(exact_iso.release());
	}
	if (candidate_out)
	    *candidate_out = candidate.release();
	return true;
    };

    for (int cut = 0; cut < source_loop.TrimCount(); ++cut) {
	const ON_BrepTrim *source_before = source_loop.Trim(cut);
	const ON_BrepTrim *source_after = source_loop.Trim(
	    (cut + 1) % source_loop.TrimCount());
	if (!source_before || !source_after || source_before->m_vi[1] < 0 ||
		source_before->m_vi[1] != source_after->m_vi[0] ||
		source_before->m_vi[1] >= brep->m_V.Count())
	    continue;
	const ON_3dPoint source_end = source_before->PointAtEnd();
	const ON_3dPoint source_start = source_after->PointAtStart();
	const double adjacent_start = source_start[closed_direction] + round(
	    (source_end[closed_direction] - source_start[closed_direction]) /
	    period) * period;
	/* On a doubly closed surface, the topology cut in closed_direction can
	 * also meet opposite but equivalent images in the nominal open
	 * direction.  Compare and seed that coordinate on one coherent periodic
	 * branch.  Treating 0 and one full period literally prevented an exact
	 * toroidal boundary chain from ever reaching the edge-driven proof below. */
	ON_3dPoint aligned_source_start = source_start;
	const double open_span =
	    source_surface->Domain(open_direction).Length();
	double open_period = 0.0;
	if (source_surface->IsClosed(open_direction)) {
	    open_period = open_span;
	    if (open_period > ON_ZERO_TOLERANCE)
		aligned_source_start[open_direction] += round(
		    (source_end[open_direction] -
			aligned_source_start[open_direction]) / open_period) *
		    open_period;
	}
	const double native_image = closed_domain.Min() + round(
	    (source_start[closed_direction] - closed_domain.Min()) / period) *
	    period;

	const auto endpoint_tolerance = [brep](const ON_BrepTrim &trim,
		int endpoint) {
	    double tolerance = LocalUnits::tolerance;
	    if (trim.Edge())
		tolerance = std::max(tolerance, trim.Edge()->m_tolerance);
	    tolerance = std::max(tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]));
	    const int vertex_index = trim.m_vi[endpoint];
	    if (vertex_index >= 0 && vertex_index < brep->m_V.Count())
		tolerance = std::max(tolerance,
		    brep->m_V[vertex_index].m_tolerance);
	    return tolerance;
	};
	const double before_tolerance = endpoint_tolerance(*source_before, 1);
	const double after_tolerance = endpoint_tolerance(*source_after, 0);
	const double cut_tolerance = std::max(before_tolerance, after_tolerance);
	const ON_3dPoint &cut_vertex =
	    brep->m_V[source_before->m_vi[1]].point;
	const ON_3dPoint source_end_lift =
	    closed_surface_point_at(source_surface, source_end);
	const ON_3dPoint source_start_lift =
	    closed_surface_point_at(source_surface, source_start);
	ON_3dPoint native_cut_uv = aligned_source_start;
	native_cut_uv[closed_direction] = native_image;
	const ON_3dPoint native_cut_lift =
	    closed_surface_point_at(source_surface, native_cut_uv);
	const ON_BrepEdge *before_edge = source_before->Edge();
	const ON_BrepEdge *after_edge = source_after->Edge();
	const ON_Interval before_domain = before_edge ?
	    before_edge->Domain() : ON_Interval::EmptyInterval;
	const ON_Interval after_domain = after_edge ?
	    after_edge->Domain() : ON_Interval::EmptyInterval;
	const ON_3dPoint before_endpoint =
	    before_edge && before_domain.IsIncreasing() ?
	    before_edge->PointAt(before_domain[source_before->m_bRev3d ? 0 : 1]) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint after_endpoint =
	    after_edge && after_domain.IsIncreasing() ?
	    after_edge->PointAt(after_domain[source_after->m_bRev3d ? 1 : 0]) :
	    ON_3dPoint::UnsetPoint;
	const double closed_parameter_residual =
	    fabs(source_end[closed_direction] - adjacent_start);
	const double open_parameter_residual =
	    fabs(source_end[open_direction] -
		aligned_source_start[open_direction]);
	const bool numerical_endpoint_continuity =
	    source_end.IsValid() && source_start.IsValid() &&
	    closed_parameter_residual <= parameter_tolerance &&
	    open_parameter_residual <= parameter_tolerance;
	const bool local_endpoint_topology =
	    source_end_lift.IsValid() && source_start_lift.IsValid() &&
	    before_endpoint.IsValid() && after_endpoint.IsValid() &&
	    source_end_lift.DistanceTo(before_endpoint) <= before_tolerance &&
	    before_endpoint.DistanceTo(cut_vertex) <= before_tolerance &&
	    source_end_lift.DistanceTo(cut_vertex) <= before_tolerance &&
	    source_start_lift.DistanceTo(after_endpoint) <= after_tolerance &&
	    after_endpoint.DistanceTo(cut_vertex) <= after_tolerance &&
	    source_start_lift.DistanceTo(cut_vertex) <= after_tolerance &&
	    source_end_lift.DistanceTo(source_start_lift) <= cut_tolerance;
	/* Exporters can terminate adjacent pcurves on equivalent images of one
	 * periodic STEP vertex while leaving a small fitted residual in the other
	 * parameter.  This occurs on singly-periodic cones and cylinders as well
	 * as doubly-periodic tori.  A parameter-only test cannot distinguish that
	 * measured corner from an unrelated UV gap.  In safe mode, admit it only
	 * when both adjacent immutable edge endpoints, both lifted pcurve
	 * endpoints, and their shared topology vertex agree within those edges'
	 * already-established local tolerances.  The residual in the other
	 * parameter must also select the unique nearer point by at least the
	 * established parameter tolerance.  Values at (or numerically just below)
	 * half a period are ambiguous.  --exact and --repair none deliberately
	 * retain the source rejection. */
	const bool bounded_periodic_endpoint_repair =
	    !numerical_endpoint_continuity &&
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe &&
	    closed_parameter_residual + parameter_tolerance < 0.5 * period &&
	    open_span > ON_ZERO_TOLERANCE &&
	    open_parameter_residual < 0.5 * open_span &&
	    local_endpoint_topology;
	if (!numerical_endpoint_continuity &&
		!bounded_periodic_endpoint_repair) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": full-period boundary L" << loop_index << " cut "
		    << cut << " rejected by periodic endpoint continuity: end="
		    << source_end.x << ':' << source_end.y << " start="
		    << source_start.x << ':' << source_start.y << " aligned="
		    << aligned_source_start.x << ':' << aligned_source_start.y
		    << " residuals=" << closed_parameter_residual << '/'
		    << open_parameter_residual << " tolerance="
		    << parameter_tolerance << " local-topology="
		    << (local_endpoint_topology ? "yes" : "no")
		    << " local-tolerances=" << before_tolerance << '/'
		    << after_tolerance << std::endl;
	    continue;
	}
	/* A supplied pcurve endpoint can miss the numerical native seam while its
	 * STEP vertex lies on that seam within the edge's asserted tolerance.  This
	 * occurs after an exporter approximates a periodic spline boundary: a
	 * parameter-only test rejects the only topologically valid cut even though
	 * both periodic images lift to the same source vertex.  Admit that cut only
	 * with this independent 3-D proof.  The candidate pcurves below are still
	 * regenerated from the authoritative edges and densely validated before
	 * any topology is committed. */
	const bool numerical_native_seam =
	    fabs(source_start[closed_direction] - native_image) <=
		parameter_tolerance;
	const bool geometric_native_seam = native_cut_lift.IsValid() &&
	    native_cut_lift.DistanceTo(cut_vertex) <= cut_tolerance;
	if (!source_end_lift.IsValid() || !source_start_lift.IsValid() ||
		(!numerical_native_seam && !geometric_native_seam) ||
		source_end_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		source_start_lift.DistanceTo(cut_vertex) > cut_tolerance) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": full-period boundary L" << loop_index << " cut "
		    << cut << " rejected by native-seam topology proof: numerical="
		    << (numerical_native_seam ? "yes" : "no") << " geometric="
		    << (geometric_native_seam ? "yes" : "no") << " distances="
		    << source_end_lift.DistanceTo(cut_vertex) << '/'
		    << source_start_lift.DistanceTo(cut_vertex) << '/'
		    << native_cut_lift.DistanceTo(cut_vertex) << " tolerance="
		    << cut_tolerance << std::endl;
	    continue;
	}

	const double periodic_corner_lift_gap =
	    source_end_lift.DistanceTo(source_start_lift);
	const auto record_periodic_corner_repair =
	    [wrapper, entity_id, &entity_type, loop_index, cut,
	    bounded_periodic_endpoint_repair, record_repair,
	    closed_parameter_residual, open_parameter_residual,
	    periodic_corner_lift_gap, cut_tolerance]() {
		if (!bounded_periodic_endpoint_repair || !record_repair)
		    return;
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "aligned a periodic source endpoint using its shared "
		    "STEP vertex and measured local edge tolerances");
		if (periodic_corner_lift_gap > LocalUnits::tolerance)
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning,
			entity_id, entity_type, "trim_pcurve",
			"adjacent periodic pcurve endpoints exceeded the declared "
			"model tolerance; aligned their parameter images using the "
			"shared STEP vertex and measured local edge tolerances");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": accepted tolerance-proven periodic endpoint for L"
			<< loop_index << " cut " << cut << " parameter-residuals="
			<< closed_parameter_residual << '/'
			<< open_parameter_residual << " lift-gap="
			<< periodic_corner_lift_gap << " topology-tolerance="
			<< cut_tolerance << std::endl;
	    };

	for (int winding = 0; winding < 2; ++winding) {
	    const double required_chain_travel =
		winding == 0 ? period : -period;
	    if (supplied_full_period_winding &&
		    supplied_chain_travel[closed_direction] *
			required_chain_travel <= 0.0)
		continue;
	    ON_BrepLoop &loop = brep->m_L[loop_index];
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface)
		continue;
	    std::vector<std::unique_ptr<ON_Curve> > regenerated_curves(
		loop.TrimCount());
	    std::vector<ON_BrepTrim *> regenerated_trims;
	    regenerated_trims.reserve(loop.TrimCount());
	    ON_3dPoint required_start = aligned_source_start;
	    required_start[closed_direction] = winding == 0 ?
		closed_domain.Min() : closed_domain.Max();
	    ON_3dPoint required_end = required_start;
	    required_end[closed_direction] = winding == 0 ?
		closed_domain.Max() : closed_domain.Min();
	    const ON_3dPoint start_lift =
		closed_surface_point_at(surface, required_start);
	    const ON_3dPoint end_lift =
		closed_surface_point_at(surface, required_end);
	    if (!start_lift.IsValid() || !end_lift.IsValid() ||
		    start_lift.DistanceTo(cut_vertex) > cut_tolerance ||
		    end_lift.DistanceTo(cut_vertex) > cut_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected by native endpoint lift: "
			<< start_lift.DistanceTo(cut_vertex) << '/'
			<< end_lift.DistanceTo(cut_vertex) << " tolerance="
			<< cut_tolerance << " start=" << required_start.x << ':'
			<< required_start.y << " end=" << required_end.x << ':'
			<< required_end.y << std::endl;
		continue;
	    }

	    bool regenerated = true;
	    std::string failure;
	    int failed_trim_index = -1;
	    for (int offset = 0; regenerated && offset < loop.TrimCount();
		    ++offset) {
		ON_BrepTrim *trim = loop.Trim(
		    (cut + 1 + offset) % loop.TrimCount());
		if (wrapper) {
		    std::ostringstream detail;
		    detail << entity_type << " loop=L" << loop_index
			<< " cut=" << cut << " winding=" << winding;
		    if (trim)
			detail << " trim=T" << trim->m_trim_index;
		    wrapper->SetProgressDetail(
			"regenerating exact full-period boundary", entity_id,
			static_cast<uint64_t>(offset + 1),
			static_cast<uint64_t>(loop.TrimCount()), "trims",
			detail.str());
		}
		ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		ON_NurbsCurve edge_nurbs;
		/* Topology discovery above intentionally uses the conservative
		 * session tolerance.  This loop is different: every regenerated
		 * curve is checked densely against its immutable STEP edge and
		 * surface before the complete BREP transaction can commit.  Admit
		 * the uncertainty asserted by this item's owning representation
		 * here, without letting an unrelated file context alter branch
		 * selection throughout the repair pipeline. */
		double tolerance = std::max(LocalUnits::tolerance,
		    LocalUnits::representation_tolerance);
		if (edge)
		    tolerance = std::max(tolerance, edge->m_tolerance);
		if (trim)
		    tolerance = std::max(tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		if (trim && trim->m_vi[0] >= 0 &&
			trim->m_vi[0] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[0]].m_tolerance);
		if (trim && trim->m_vi[1] >= 0 &&
			trim->m_vi[1] < brep->m_V.Count())
		    tolerance = std::max(tolerance,
			brep->m_V[trim->m_vi[1]].m_tolerance);
		ON_3dPoint constrained_end = ON_3dPoint::UnsetPoint;
		const ON_3dPoint *last_endpoint =
		    offset + 1 == loop.TrimCount() ? &required_end : NULL;
		if (supplied_full_period_winding && trim) {
		    const std::map<int, ON_3dVector>::const_iterator travel =
			supplied_trim_travel.find(trim->m_trim_index);
		    if (travel == supplied_trim_travel.end()) {
			regenerated = false;
			failure = "missing supplied trim travel for a proven "
			    "full-period chain";
			failed_trim_index = trim->m_trim_index;
			break;
		    }
		    constrained_end = required_start + travel->second;
		    constrained_end.z = 0.0;
		    if (offset + 1 == loop.TrimCount()) {
			bool terminal_image_matches = true;
			for (int direction = 0; direction < 2; ++direction) {
			    double residual = constrained_end[direction] -
				required_end[direction];
			    if (surface->IsClosed(direction)) {
				const double direction_period =
				    surface->Domain(direction).Length();
				if (direction_period > ON_ZERO_TOLERANCE)
				    residual -= round(residual /
					direction_period) * direction_period;
			    }
			    terminal_image_matches = terminal_image_matches &&
				fabs(residual) <= parameter_tolerance;
			}
			if (!terminal_image_matches) {
			    regenerated = false;
			    failure = "supplied unwrapped trim travel did not "
				"reach the required full-period endpoint";
			    failed_trim_index = trim->m_trim_index;
			    break;
			}
			constrained_end = required_end;
		    }
		    last_endpoint = &constrained_end;
		}
		ON_Curve *regenerated_curve = NULL;
		const std::chrono::steady_clock::time_point trim_started =
		    std::chrono::steady_clock::now();
		std::string fast_path_failure;
		regenerated = trim && edge && translated_exact_pcurve(*trim, *edge,
		    tolerance, required_start, last_endpoint, &regenerated_curve,
		    &fast_path_failure);
		if (!regenerated) {
		    if (wrapper) {
			std::ostringstream detail;
			detail << entity_type << " loop=L" << loop_index
			    << " cut=" << cut << " winding=" << winding
			    << " trim=T" << (trim ? trim->m_trim_index : -1)
			    << " fallback=" << fast_path_failure;
			wrapper->SetProgressDetail(
			    "pulling exact full-period boundary from its edge",
			    entity_id, static_cast<uint64_t>(offset + 1),
			    static_cast<uint64_t>(loop.TrimCount()), "trims",
			    detail.str());
		    }
		    regenerated = trim && edge && edge->GetNurbForm(edge_nurbs) &&
			regenerate_trim_polyline(brep, *trim, surface,
			    edge_nurbs, tolerance, &failure, NULL, &required_start,
			    last_endpoint, true, wrapper, true, &regenerated_curve);
		}
		const long long trim_microseconds =
		    std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - trim_started).count();
		if (wrapper->Verbose() && trim_microseconds >= 100000)
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index << "/T"
			<< (trim ? trim->m_trim_index : -1) << " cut/winding "
			<< cut << '/' << winding << " offset " << offset + 1 << '/'
			<< loop.TrimCount() << " elapsed="
			<< trim_microseconds / 1000.0 << "ms result="
			<< (regenerated ? "accepted" : "rejected") << std::endl;
		if (regenerated) {
		    regenerated_curves[offset].reset(regenerated_curve);
		    regenerated_trims.push_back(trim);
		    required_start = regenerated_curve->PointAtEnd();
		} else {
		    failed_trim_index = trim ? trim->m_trim_index : -1;
		}
	    }
	    if (!regenerated) {
		if (wrapper->Verbose()) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary regeneration L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected at T" << failed_trim_index << ": "
			<< (failure.empty() ? "no exact candidate" : failure)
			<< std::endl;
		    for (size_t offset = 0; offset < regenerated_curves.size();
			    ++offset) {
			const ON_Curve *curve = regenerated_curves[offset].get();
			const ON_BrepTrim *trim = offset <
			    regenerated_trims.size() ?
			    regenerated_trims[offset] : NULL;
			if (!curve)
			    continue;
			std::cerr << "  accepted prefix T"
			    << (trim ? trim->m_trim_index : -1)
			    << "/STEP edge "
			    << (trim && trim->Edge() ?
				trim->Edge()->m_edge_user.i : -1)
			    << ' ' << curve->PointAtStart().x << ':'
			    << curve->PointAtStart().y << "->"
			    << curve->PointAtEnd().x << ':'
			    << curve->PointAtEnd().y << std::endl;
		    }
		}
		continue;
	    }

	    const auto native_boundary_pair = [&closed_domain, closed_direction,
		    open_direction, parameter_tolerance](
		    const ON_3dPoint &start, const ON_3dPoint &end) {
		const bool forward =
		    fabs(start[closed_direction] - closed_domain.Min()) <=
			parameter_tolerance &&
		    fabs(end[closed_direction] - closed_domain.Max()) <=
			parameter_tolerance;
		const bool reverse =
		    fabs(start[closed_direction] - closed_domain.Max()) <=
			parameter_tolerance &&
		    fabs(end[closed_direction] - closed_domain.Min()) <=
			parameter_tolerance;
		return start.IsValid() && end.IsValid() &&
		    (forward || reverse) &&
		    fabs(start[open_direction] - end[open_direction]) <=
			parameter_tolerance;
	    };
	    const ON_Curve *raw_first = regenerated_curves.empty() ? NULL :
		regenerated_curves.front().get();
	    const ON_Curve *raw_last = regenerated_curves.empty() ? NULL :
		regenerated_curves.back().get();
	    if (raw_first && raw_last &&
		    !native_boundary_pair(raw_first->PointAtStart(),
			raw_last->PointAtEnd())) {
		/* Exact edge pullback can choose Min()->Min()-period (or the
		 * symmetric Max()->Max()+period) when approaching the final
		 * endpoint from the continuous interior branch.  That is the
		 * correct winding but not the native Min()/Max() pair needed by
		 * the explicit seam constructor.  Translate the complete chain
		 * by one integral period only when every shifted pcurve still
		 * densely follows its directed immutable edge. */
		for (int turns = -2; turns <= 2; ++turns) {
		    if (turns == 0)
			continue;
		    ON_3dPoint shifted_start = raw_first->PointAtStart();
		    ON_3dPoint shifted_end = raw_last->PointAtEnd();
		    const double shift = turns * period;
		    shifted_start[closed_direction] += shift;
		    shifted_end[closed_direction] += shift;
		    if (!native_boundary_pair(shifted_start, shifted_end))
			continue;
		    std::vector<std::unique_ptr<ON_Curve> > shifted_curves;
		    shifted_curves.reserve(regenerated_curves.size());
		    bool exact_shift = true;
		    ON_Xform translation(ON_Xform::IdentityTransformation);
		    translation.m_xform[closed_direction][3] = shift;
		    for (size_t offset = 0; exact_shift &&
			    offset < regenerated_curves.size(); ++offset) {
			const ON_BrepTrim *trim = offset <
			    regenerated_trims.size() ?
			    regenerated_trims[offset] : NULL;
			const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
			std::unique_ptr<ON_Curve> shifted(
			    regenerated_curves[offset] ?
			    regenerated_curves[offset]->DuplicateCurve() : NULL);
			if (!trim || !edge || !shifted ||
				!shifted->Transform(translation) ||
				!shifted->ChangeDimension(2) ||
				!shifted->IsValid()) {
			    exact_shift = false;
			    break;
			}
			double tolerance = std::max(LocalUnits::tolerance,
			    std::max(edge->m_tolerance,
				std::max(trim->m_tolerance[0],
				    trim->m_tolerance[1])));
			const ON_Interval curve_domain = shifted->Domain();
			const ON_Interval edge_domain = edge->Domain();
			for (int sample = 0; exact_shift &&
				sample <= kDenseValidationSegments; ++sample) {
			    const double fraction =
				static_cast<double>(sample) /
				kDenseValidationSegments;
			    const ON_3dPoint uv = shifted->PointAt(
				curve_domain.ParameterAt(fraction));
			    const ON_3dPoint lift = uv.IsValid() ?
				closed_surface_point_at(surface, uv) :
				ON_3dPoint::UnsetPoint;
			    const ON_3dPoint edge_point = edge->PointAt(
				edge_domain.ParameterAt(trim->m_bRev3d ?
				    1.0 - fraction : fraction));
			    exact_shift = lift.IsValid() &&
				edge_point.IsValid() &&
				lift.DistanceTo(edge_point) <= tolerance;
			}
			if (exact_shift)
			    shifted_curves.push_back(std::move(shifted));
		    }
		    if (!exact_shift ||
			    shifted_curves.size() != regenerated_curves.size())
			continue;
		    regenerated_curves.swap(shifted_curves);
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": full-period boundary L" << loop_index
			    << " canonicalized an exact " << turns
			    << "-period chain translation" << std::endl;
		    break;
		}
	    }

	    /* A source edge can terminate a small measured distance from its
	     * declared topology vertex.  Exact edge pullback then approaches the
	     * correct native seam from one side but cannot land on it without
	     * leaving the edge's literal locus.  This is distinct from an arbitrary
	     * UV gap: the first boundary endpoint already selects one native side,
	     * the opposite native endpoint and the pulled endpoint must both lift
	     * to the same STEP vertex within the established local edge tolerance,
	     * and the complete short connector must remain on the immutable 3-D
	     * edge locus in the directed sense.  Preserve the source geometry and
	     * express that measured endpoint inconsistency as OpenNURBS tolerance,
	     * as required for other bounded source edge/vertex mismatches.  Under
	     * --exact the local tolerance is never enlarged, so the same proof
	     * remains strict. */
	    bool bounded_terminal_correction = false;
	    double terminal_correction_distance = 0.0;
	    raw_first = regenerated_curves.empty() ? NULL :
		regenerated_curves.front().get();
	    raw_last = regenerated_curves.empty() ? NULL :
		regenerated_curves.back().get();
	    if (raw_first && raw_last &&
		    !native_boundary_pair(raw_first->PointAtStart(),
			raw_last->PointAtEnd()) && !regenerated_trims.empty()) {
		const ON_3dPoint boundary_start = raw_first->PointAtStart();
		const ON_3dPoint boundary_end = raw_last->PointAtEnd();
		ON_3dPoint required_terminal = boundary_start;
		bool have_native_start = false;
		if (fabs(boundary_start[closed_direction] -
			closed_domain.Min()) <= parameter_tolerance) {
		    required_terminal[closed_direction] = closed_domain.Max();
		    have_native_start = true;
		} else if (fabs(boundary_start[closed_direction] -
			closed_domain.Max()) <= parameter_tolerance) {
		    required_terminal[closed_direction] = closed_domain.Min();
		    have_native_start = true;
		}
		const double parameter_residual = have_native_start ?
		    fabs(boundary_end[closed_direction] -
			required_terminal[closed_direction]) : DBL_MAX;
		ON_BrepTrim *terminal_trim = regenerated_trims.back();
		ON_BrepEdge *terminal_edge = terminal_trim ?
		    terminal_trim->Edge() : NULL;
		double terminal_tolerance = LocalUnits::tolerance;
		if (terminal_trim)
		    terminal_tolerance = std::max(terminal_tolerance,
			std::max(terminal_trim->m_tolerance[0],
			    terminal_trim->m_tolerance[1]));
		if (terminal_edge)
		    terminal_tolerance = std::max(terminal_tolerance,
			terminal_edge->m_tolerance);
		if (terminal_trim && terminal_trim->m_vi[1] >= 0 &&
			terminal_trim->m_vi[1] < brep->m_V.Count())
		    terminal_tolerance = std::max(terminal_tolerance,
			brep->m_V[terminal_trim->m_vi[1]].m_tolerance);
		const ON_3dPoint boundary_lift = boundary_end.IsValid() ?
		    closed_surface_point_at(surface, boundary_end) :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint required_lift = required_terminal.IsValid() ?
		    closed_surface_point_at(surface, required_terminal) :
		    ON_3dPoint::UnsetPoint;
		const ON_Interval terminal_edge_domain = terminal_edge ?
		    terminal_edge->Domain() : ON_Interval::EmptyInterval;
		const ON_3dPoint terminal_edge_endpoint =
		    terminal_edge && terminal_edge_domain.IsIncreasing() ?
		    terminal_edge->PointAt(terminal_edge_domain[
			terminal_trim->m_bRev3d ? 0 : 1]) :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint terminal_vertex =
		    terminal_trim && terminal_trim->m_vi[1] >= 0 &&
		    terminal_trim->m_vi[1] < brep->m_V.Count() ?
		    brep->m_V[terminal_trim->m_vi[1]].point :
		    ON_3dPoint::UnsetPoint;
		terminal_correction_distance =
		    boundary_lift.IsValid() && required_lift.IsValid() ?
		    boundary_lift.DistanceTo(required_lift) : DBL_MAX;
		const double boundary_edge_distance =
		    boundary_lift.IsValid() && terminal_edge_endpoint.IsValid() ?
		    boundary_lift.DistanceTo(terminal_edge_endpoint) : DBL_MAX;
		const double required_edge_distance =
		    required_lift.IsValid() && terminal_edge_endpoint.IsValid() ?
		    required_lift.DistanceTo(terminal_edge_endpoint) : DBL_MAX;
		const double boundary_vertex_distance =
		    boundary_lift.IsValid() && terminal_vertex.IsValid() ?
		    boundary_lift.DistanceTo(terminal_vertex) : DBL_MAX;
		const double required_vertex_distance =
		    required_lift.IsValid() && terminal_vertex.IsValid() ?
		    required_lift.DistanceTo(terminal_vertex) : DBL_MAX;
		bool exact_connector = have_native_start &&
		    parameter_residual > parameter_tolerance &&
		    parameter_residual < 0.5 * period &&
		    fabs(boundary_end[open_direction] -
			required_terminal[open_direction]) <= parameter_tolerance &&
		    boundary_lift.IsValid() && required_lift.IsValid() &&
		    terminal_edge_endpoint.IsValid() && terminal_vertex.IsValid() &&
		    boundary_lift.DistanceTo(terminal_edge_endpoint) <=
			terminal_tolerance &&
		    required_lift.DistanceTo(terminal_edge_endpoint) <=
			terminal_tolerance &&
		    boundary_lift.DistanceTo(terminal_vertex) <=
			terminal_tolerance &&
		    required_lift.DistanceTo(terminal_vertex) <=
			terminal_tolerance &&
		    terminal_correction_distance <= terminal_tolerance;
		bool connector_locus_proven = false;
		double connector_direction_alignment = -DBL_MAX;
		std::vector<ON_3dPoint> connector_lifts;
		if (exact_connector) {
		    connector_lifts.reserve(kDenseValidationSegments + 1);
		    for (int sample = 0; sample <= kDenseValidationSegments;
			    ++sample) {
			const double fraction = static_cast<double>(sample) /
			    kDenseValidationSegments;
			const ON_3dPoint uv = (1.0 - fraction) * boundary_end +
			    fraction * required_terminal;
			const ON_3dPoint lift = closed_surface_point_at(surface, uv);
			if (!lift.IsValid()) {
			    exact_connector = false;
			    break;
			}
			connector_lifts.push_back(lift);
		    }
		}
		if (exact_connector) {
		    connector_locus_proven = terminal_edge &&
			step_curve_locus_contains_points(terminal_edge,
			    connector_lifts.data(), connector_lifts.size(),
			    terminal_tolerance);
		    exact_connector = connector_locus_proven;
		}
		if (exact_connector) {
		    ON_3dVector connector_direction =
			required_lift - boundary_lift;
		    ON_3dVector edge_direction = terminal_edge->TangentAt(
			terminal_edge_domain[
			    terminal_trim->m_bRev3d ? 0 : 1]);
		    if (terminal_trim->m_bRev3d)
			edge_direction.Reverse();
		    const bool directions_valid = connector_direction.Unitize() &&
			edge_direction.Unitize();
		    if (directions_valid)
			connector_direction_alignment =
			    connector_direction * edge_direction;
		    exact_connector = directions_valid &&
			connector_direction_alignment >= 0.0;
		}
		std::unique_ptr<ON_PolyCurve> corrected;
		if (exact_connector) {
		    corrected.reset(new ON_PolyCurve());
		    ON_Curve *prefix = raw_last->DuplicateCurve();
		    ON_LineCurve *connector = new ON_LineCurve(boundary_end,
			required_terminal);
		    if (!prefix || !connector->ChangeDimension(2) ||
			    !connector->IsValid() || !corrected->Append(prefix)) {
			delete prefix;
			delete connector;
			corrected.reset();
		    } else if (!corrected->Append(connector)) {
			delete connector;
			corrected.reset();
		    } else if (!corrected->SetDomain(raw_last->Domain().Min(),
			    raw_last->Domain().Max()) ||
			    !corrected->ChangeDimension(2) ||
			    !corrected->IsValid()) {
			corrected.reset();
		    }
		}
		if (corrected) {
		    regenerated_curves.back().reset(corrected.release());
		    bounded_terminal_correction = true;
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": full-period boundary L" << loop_index
			    << " closed a tolerance-proven terminal seam residual "
			    << parameter_residual << " (lift "
			    << terminal_correction_distance << ", tolerance "
			    << terminal_tolerance << ')' << std::endl;
		} else if (wrapper->Verbose() && have_native_start &&
			parameter_residual > parameter_tolerance &&
			parameter_residual < 0.5 * period) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " rejected terminal seam correction residual="
			<< parameter_residual << " lift="
			<< terminal_correction_distance << " edge-distances="
			<< boundary_edge_distance << '/' << required_edge_distance
			<< " vertex-distances=" << boundary_vertex_distance << '/'
			<< required_vertex_distance << " tolerance="
			<< terminal_tolerance << " locus="
			<< (connector_locus_proven ? "yes" : "no")
			<< " direction=" << connector_direction_alignment
			<< " curve=" << (exact_connector ? "rejected" :
			    "not-constructed") << std::endl;
		}
	    }

	    /* Dense locus validation above permits periodic normalization and
	     * closest-point correspondence.  Before a pole cut makes every
	     * physical-boundary join internal, require each directly evaluated
	     * pcurve endpoint to match its directed STEP edge endpoint and topology
	     * vertex.  Otherwise a later isoparametric repair can expose a
	     * one-period gap at the join which was forced continuous on the wrong
	     * image (the periodic triangular-cap regression).
	     *
	     * The only exception is proved per endpoint: the other parameter must
	     * be within the existing parameter tolerance of this full-period
	     * boundary's already established constant coordinate, and substituting
	     * exactly that coordinate must lift to both the immutable directed edge
	     * endpoint and topology vertex within their established tolerance.
	     * This preserves NIST MBE PMI 6's rational cap, whose fitted pcurve is
	     * a few parameter ulps off its exact isocurve, without treating an
	     * arbitrary one-period mismatch as an analytic special case. */
	    const double exact_boundary_parameter =
		required_start[open_direction];
	    bool directed_endpoint_chain = true;
	    bool endpoint_boundary_coordinate_proven = false;
	    int endpoint_mismatch_trim_index = -1;
	    int endpoint_mismatch_end = -1;
	    double endpoint_edge_distance = 0.0;
	    double endpoint_vertex_distance = 0.0;
	    double endpoint_proof_tolerance = std::max(LocalUnits::tolerance,
		LocalUnits::representation_tolerance);
	    double endpoint_boundary_parameter_residual = DBL_MAX;
	    double endpoint_boundary_edge_distance = DBL_MAX;
	    double endpoint_boundary_vertex_distance = DBL_MAX;
	    ON_3dPoint endpoint_mismatch_uv = ON_3dPoint::UnsetPoint;
	    ON_3dPoint endpoint_mismatch_lift = ON_3dPoint::UnsetPoint;
	    ON_3dPoint endpoint_mismatch_edge = ON_3dPoint::UnsetPoint;
	    ON_3dPoint endpoint_mismatch_vertex = ON_3dPoint::UnsetPoint;
	    double endpoint_opposite_edge_distance = DBL_MAX;
	    std::map<int, double> representation_endpoint_adjustments;
	    for (size_t offset = 0; directed_endpoint_chain &&
		    offset < regenerated_curves.size(); ++offset) {
		const ON_Curve *curve = regenerated_curves[offset].get();
		const ON_BrepTrim *trim = offset < regenerated_trims.size() ?
		    regenerated_trims[offset] : NULL;
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		const ON_Interval curve_domain = curve ?
		    curve->Domain() : ON_Interval::EmptyInterval;
		const ON_Interval edge_domain = edge ?
		    edge->Domain() : ON_Interval::EmptyInterval;
		double established_tolerance = LocalUnits::tolerance;
		if (edge)
		    established_tolerance = std::max(established_tolerance,
			edge->m_tolerance);
		if (trim)
		    established_tolerance = std::max(established_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
		for (int end = 0; end < 2; ++end) {
		    const int vertex_index = trim ? trim->m_vi[end] : -1;
		    double endpoint_established_tolerance =
			established_tolerance;
		    if (vertex_index >= 0 && vertex_index < brep->m_V.Count())
			endpoint_established_tolerance = std::max(
			    endpoint_established_tolerance,
			    brep->m_V[vertex_index].m_tolerance);
		    const double directed_tolerance = std::max(
			endpoint_established_tolerance,
			LocalUnits::representation_tolerance);
		    const ON_3dPoint uv = curve && curve_domain.IsIncreasing() ?
			curve->PointAt(curve_domain[end]) :
			ON_3dPoint::UnsetPoint;
		    const ON_3dPoint lift = uv.IsValid() ?
			surface->PointAt(uv.x, uv.y) :
			ON_3dPoint::UnsetPoint;
		    const ON_3dPoint edge_endpoint =
			edge && edge_domain.IsIncreasing() ?
			edge->PointAt(edge_domain[
			    trim->m_bRev3d ? 1 - end : end]) :
			ON_3dPoint::UnsetPoint;
		    const ON_3dPoint vertex =
			vertex_index >= 0 && vertex_index < brep->m_V.Count() ?
			brep->m_V[vertex_index].point :
			ON_3dPoint::UnsetPoint;
		    const double edge_distance = lift.IsValid() &&
			edge_endpoint.IsValid() ?
			lift.DistanceTo(edge_endpoint) : DBL_MAX;
		    const double vertex_distance = lift.IsValid() &&
			vertex.IsValid() ? lift.DistanceTo(vertex) : DBL_MAX;
		    if (edge_distance <= directed_tolerance &&
			    vertex_distance <= directed_tolerance) {
			const double measured = std::max(edge_distance,
			    vertex_distance);
			if (trim &&
				measured > endpoint_established_tolerance &&
				LocalUnits::representation_tolerance >
				    endpoint_established_tolerance)
			    representation_endpoint_adjustments[
				trim->m_trim_index] = std::max(
				    representation_endpoint_adjustments[
					trim->m_trim_index],
				    measured * kRegenerationToleranceSafety);
			continue;
		    }
		    endpoint_mismatch_trim_index =
			trim ? trim->m_trim_index : -1;
		    endpoint_mismatch_end = end;
		    endpoint_edge_distance = edge_distance;
		    endpoint_vertex_distance = vertex_distance;
		    endpoint_proof_tolerance = directed_tolerance;
		    endpoint_mismatch_uv = uv;
		    endpoint_mismatch_lift = lift;
		    endpoint_mismatch_edge = edge_endpoint;
		    endpoint_mismatch_vertex = vertex;
		    if (edge && edge_domain.IsIncreasing() && lift.IsValid())
			endpoint_opposite_edge_distance = lift.DistanceTo(
			    edge->PointAt(edge_domain[
				trim->m_bRev3d ? end : 1 - end]));
		    endpoint_boundary_coordinate_proven = false;
		    if (uv.IsValid() && edge_endpoint.IsValid() &&
			    vertex.IsValid()) {
			endpoint_boundary_parameter_residual =
			    fabs(uv[open_direction] -
				exact_boundary_parameter);
			ON_3dPoint corrected_uv = uv;
			corrected_uv[open_direction] =
			    exact_boundary_parameter;
			const ON_3dPoint corrected_lift =
			    closed_surface_point_at(surface, corrected_uv);
			endpoint_boundary_edge_distance =
			    corrected_lift.IsValid() ?
			    corrected_lift.DistanceTo(edge_endpoint) : DBL_MAX;
			endpoint_boundary_vertex_distance =
			    corrected_lift.IsValid() ?
			    corrected_lift.DistanceTo(vertex) : DBL_MAX;
			endpoint_boundary_coordinate_proven =
			    endpoint_boundary_parameter_residual <=
				parameter_tolerance &&
			    endpoint_boundary_edge_distance <=
				directed_tolerance &&
			    endpoint_boundary_vertex_distance <=
				directed_tolerance;
		    }
		    if (!endpoint_boundary_coordinate_proven)
			directed_endpoint_chain = false;
		    break;
		}
	    }
	    const auto apply_representation_endpoint_adjustments =
		[&representation_endpoint_adjustments](ON_Brep *candidate) {
		    if (!candidate)
			return;
		    for (std::map<int, double>::const_iterator adjustment =
			    representation_endpoint_adjustments.begin();
			 adjustment !=
			    representation_endpoint_adjustments.end();
			 ++adjustment) {
			if (adjustment->first < 0 ||
				adjustment->first >= candidate->m_T.Count())
			    continue;
			ON_BrepTrim &adjusted =
			    candidate->m_T[adjustment->first];
			adjusted.m_tolerance[0] = std::max(
			    adjusted.m_tolerance[0], adjustment->second);
			adjusted.m_tolerance[1] = std::max(
			    adjusted.m_tolerance[1], adjustment->second);
		    }
		};
	    const auto record_representation_tolerance =
		[wrapper, entity_id, &entity_type, record_repair,
		    &representation_endpoint_adjustments]() {
		    if (!record_repair ||
			    representation_endpoint_adjustments.empty())
			return;
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"validated periodic boundary endpoints against the owning "
			"representation uncertainty");
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning,
			entity_id, entity_type, "trim_pcurve",
			"periodic boundary endpoints exceeded the conservative "
			"file tolerance; used the owning representation's "
			"uncertainty after dense exact-locus validation");
		};
	    if (endpoint_mismatch_trim_index >= 0 && wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": full-period boundary L" << loop_index
		    << " cut/winding " << cut << '/' << winding << ' '
		    << (directed_endpoint_chain ?
			"retained tolerance-proven boundary coordinate" :
			"rejected")
		    << " directed endpoint T" << endpoint_mismatch_trim_index
		    << '[' << endpoint_mismatch_end
		    << "] edge/vertex distances=" << endpoint_edge_distance << '/'
		    << endpoint_vertex_distance << " tolerance="
		    << endpoint_proof_tolerance << " boundary-proof="
		    << (endpoint_boundary_coordinate_proven ? "yes" : "no")
		    << " parameter/edge/vertex="
		    << endpoint_boundary_parameter_residual << '/'
		    << endpoint_boundary_edge_distance << '/'
		    << endpoint_boundary_vertex_distance
		    << " opposite-edge=" << endpoint_opposite_edge_distance
		    << " uv=" << endpoint_mismatch_uv.x << ':'
		    << endpoint_mismatch_uv.y << " lift="
		    << endpoint_mismatch_lift.x << ':'
		    << endpoint_mismatch_lift.y << ':'
		    << endpoint_mismatch_lift.z << " edge="
		    << endpoint_mismatch_edge.x << ':'
		    << endpoint_mismatch_edge.y << ':'
		    << endpoint_mismatch_edge.z << " vertex="
		    << endpoint_mismatch_vertex.x << ':'
		    << endpoint_mismatch_vertex.y << ':'
		    << endpoint_mismatch_vertex.z << std::endl;

	    bool exact_chain = directed_endpoint_chain;
	    for (int offset = 0; exact_chain && offset < loop.TrimCount() - 1;
		    ++offset) {
		const ON_BrepTrim *previous = regenerated_trims[offset];
		const ON_BrepTrim *next = regenerated_trims[offset + 1];
		const double join_gap = regenerated_curves[offset] &&
		    regenerated_curves[offset + 1] ?
		    regenerated_curves[offset]->PointAtEnd().DistanceTo(
			regenerated_curves[offset + 1]->PointAtStart()) : DBL_MAX;
		exact_chain = previous && next && regenerated_curves[offset] &&
		    regenerated_curves[offset + 1] &&
		    previous->m_vi[1] == next->m_vi[0] &&
		    join_gap <= parameter_tolerance;
		if (!exact_chain && wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding << " join "
			<< offset << '/' << offset + 1 << " topology="
			<< (previous && next ? previous->m_vi[1] : -1) << '/'
			<< (previous && next ? next->m_vi[0] : -1) << " gap="
			<< join_gap << " tolerance=" << parameter_tolerance
			<< std::endl;
	    }
	    const ON_Curve *first_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.front().get();
	    const ON_Curve *last_curve = regenerated_curves.empty() ? NULL :
		regenerated_curves.back().get();
	    const bool boundary_endpoints_valid = first_curve && last_curve &&
		native_boundary_pair(first_curve->PointAtStart(),
		    last_curve->PointAtEnd());

	    /* A segmented edge-driven boundary can retain a small interior UV
	     * residual even though every endpoint lift matches the same STEP
	     * topology vertex.  Reconcile only those interior joins on a temporary
	     * BREP.  The final-to-first full-period cut must remain open here so the
	     * face-band builder can materialize it as two explicit seam uses. */
	    if (!exact_chain && directed_endpoint_chain &&
		    boundary_endpoints_valid) {
		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		bool candidate_valid = loop_index >= 0 &&
		    loop_index < candidate->m_L.Count();
		ON_BrepLoop *candidate_loop = candidate_valid ?
		    &candidate->m_L[loop_index] : NULL;
		std::vector<int> ordered_trim_indices;
		ordered_trim_indices.reserve(regenerated_trims.size());
		for (size_t offset = 0; candidate_valid &&
			offset < regenerated_trims.size(); ++offset) {
		    const int trim_index = regenerated_trims[offset] ?
			regenerated_trims[offset]->m_trim_index : -1;
		    ON_Curve *curve = regenerated_curves[offset] ?
			regenerated_curves[offset]->DuplicateCurve() : NULL;
		    const int c2_index = curve ?
			candidate->AddTrimCurve(curve) : -1;
		    if (trim_index < 0 || trim_index >= candidate->m_T.Count() ||
			    c2_index < 0 ||
			    !candidate->SetTrimCurve(candidate->m_T[trim_index],
				c2_index)) {
			if (curve && c2_index < 0)
			    delete curve;
			candidate_valid = false;
			break;
		    }
		    candidate->SetTrimIsoFlags(candidate->m_T[trim_index]);
		    ordered_trim_indices.push_back(trim_index);
		}
		if (candidate_valid)
		    apply_representation_endpoint_adjustments(candidate.get());
		if (candidate_valid) {
		    candidate_loop->m_ti.SetCount(0);
		    for (std::vector<int>::const_iterator trim_index =
			    ordered_trim_indices.begin();
			    trim_index != ordered_trim_indices.end(); ++trim_index)
			candidate_loop->m_ti.Append(*trim_index);
		    repair_adjacent_trim_endpoints(candidate.get(), wrapper,
			entity_id, entity_type, loop_index, false, true);
		    candidate_loop = &candidate->m_L[loop_index];
		    for (int lti = 0; candidate_valid &&
			    lti + 1 < candidate_loop->TrimCount(); ++lti) {
			const ON_BrepTrim *previous = candidate_loop->Trim(lti);
			const ON_BrepTrim *next = candidate_loop->Trim(lti + 1);
			candidate_valid = previous && next &&
			    previous->PointAtEnd().DistanceTo(
				next->PointAtStart()) <= ON_ZERO_TOLERANCE;
		    }
		    const ON_BrepTrim *candidate_first =
			candidate_loop->TrimCount() > 0 ?
			candidate_loop->Trim(0) : NULL;
		    const ON_BrepTrim *candidate_last =
			candidate_loop->TrimCount() > 0 ?
			candidate_loop->Trim(candidate_loop->TrimCount() - 1) :
			NULL;
		    candidate_valid = candidate_valid && candidate_first &&
			candidate_last &&
			native_boundary_pair(candidate_first->PointAtStart(),
			    candidate_last->PointAtEnd());
		    for (int lti = 0; candidate_valid &&
			    lti < candidate_loop->TrimCount(); ++lti) {
			const ON_BrepTrim *trim = candidate_loop->Trim(lti);
			ON_wString trim_messages;
			ON_TextLog trim_log(trim_messages);
			candidate_valid = trim && trim->IsValid(&trim_log);
		    }
		}
		if (candidate_valid) {
		    *brep = *candidate;
		    if (record_repair) {
			wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			    "unwrapped an exact full-period boundary from its 3-D STEP edge chain");
			wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
			    "closed residual joins in an exact full-period boundary chain");
			if (!numerical_native_seam && geometric_native_seam)
			    wrapper->RecordRepair(entity_id, entity_type,
				"trim_pcurve",
				"selected a periodic boundary cut from a tolerance-proven native seam vertex");
		    }
		    record_periodic_corner_repair();
		    record_representation_tolerance();
		    return true;
		}
	    }

	    if (!exact_chain || !boundary_endpoints_valid) {
		if (wrapper->Verbose()) {
		    std::cerr << entity_type << " #" << entity_id
			<< ": full-period boundary L" << loop_index
			<< " cut/winding " << cut << '/' << winding
			<< " rejected by final chain proof: exact="
			<< (exact_chain ? "yes" : "no") << " first="
			<< (first_curve ? first_curve->PointAtStart().x :
			    ON_UNSET_VALUE) << ':'
			<< (first_curve ? first_curve->PointAtStart().y :
			    ON_UNSET_VALUE) << " last="
			<< (last_curve ? last_curve->PointAtEnd().x :
			    ON_UNSET_VALUE) << ':'
			<< (last_curve ? last_curve->PointAtEnd().y :
			    ON_UNSET_VALUE) << " tolerance="
			<< parameter_tolerance << std::endl;
		    for (size_t offset = 0; offset < regenerated_curves.size();
			    ++offset) {
			const ON_Curve *curve = regenerated_curves[offset].get();
			const ON_BrepTrim *trim = offset <
			    regenerated_trims.size() ?
			    regenerated_trims[offset] : NULL;
			if (!curve || !curve->Domain().IsIncreasing())
			    continue;
			double minimum = DBL_MAX;
			double maximum = -DBL_MAX;
			double previous = 0.0;
			double net = 0.0;
			for (int sample = 0; sample <=
				kPcurveLocusScreeningSegments; ++sample) {
			    const ON_3dPoint uv = curve->PointAt(
				curve->Domain().ParameterAt(
				    static_cast<double>(sample) /
				    kPcurveLocusScreeningSegments));
			    if (!uv.IsValid())
				continue;
			    double unwrapped = uv[closed_direction];
			    if (sample > 0)
				unwrapped += round((previous - unwrapped) /
				    period) * period;
			    else
				net = unwrapped;
			    previous = unwrapped;
			    minimum = std::min(minimum, unwrapped);
			    maximum = std::max(maximum, unwrapped);
			}
			net = previous - net;
			std::cerr << " T" << (trim ? trim->m_trim_index : -1)
			    << "/STEP edge "
			    << (trim && trim->Edge() ?
				trim->Edge()->m_edge_user.i : -1)
			    << '=' << curve->PointAtStart().x << ':'
			    << curve->PointAtStart().y << "->"
			    << curve->PointAtEnd().x << ':'
			    << curve->PointAtEnd().y << " closed-net/span="
			    << net << '/' << maximum - minimum;
		    }
		    std::cerr << std::endl;
		}
		continue;
	    }

	    /* Generate and prove every replacement before modifying the BREP.  The
	     * former transaction copied the complete solid for each candidate loop;
	     * a production CRM solid with roughly eleven thousand faces performed
	     * this copy more than a thousand times.  Curve ownership is committed
	     * only after every member of this one loop has validated. */
	    struct OriginalTrimCurveState {
		int c2_index;
		ON_Interval proxy_domain;
		ON_Interval trim_domain;
		ON_Surface::ISO iso;
		ON_BoundingBox pbox;
	    };
	    std::vector<OriginalTrimCurveState> original_states;
	    original_states.reserve(regenerated_trims.size());
	    for (std::vector<ON_BrepTrim *>::const_iterator trim =
		    regenerated_trims.begin(); trim != regenerated_trims.end(); ++trim) {
		OriginalTrimCurveState state = {
		    (*trim)->m_c2i, (*trim)->ProxyCurveDomain(), (*trim)->Domain(),
		    (*trim)->m_iso, (*trim)->m_pbox
		};
		original_states.push_back(state);
	    }
	    const int original_c2_count = brep->m_C2.Count();
	    std::vector<int> new_c2_indices;
	    new_c2_indices.reserve(regenerated_curves.size());
	    bool curves_added = true;
	    for (std::vector<std::unique_ptr<ON_Curve> >::iterator curve =
		    regenerated_curves.begin(); curve != regenerated_curves.end();
		    ++curve) {
		const int c2_index = curve->get() ?
		    brep->AddTrimCurve(curve->get()) : -1;
		if (c2_index < 0) {
		    curves_added = false;
		    break;
		}
		curve->release();
		new_c2_indices.push_back(c2_index);
	    }
	    const auto discard_added_curves = [brep, original_c2_count]() {
		for (int c2_index = original_c2_count;
			c2_index < brep->m_C2.Count(); ++c2_index)
		    delete brep->m_C2[c2_index];
		brep->m_C2.SetCount(original_c2_count);
	    };
	    if (!curves_added) {
		discard_added_curves();
		continue;
	    }
	    bool curves_installed = true;
	    size_t installed_count = 0;
	    for (; installed_count < regenerated_trims.size(); ++installed_count) {
		if (!brep->SetTrimCurve(*regenerated_trims[installed_count],
			new_c2_indices[installed_count])) {
		    curves_installed = false;
		    break;
		}
		brep->SetTrimIsoFlags(*regenerated_trims[installed_count]);
	    }
	    if (!curves_installed) {
		for (size_t restored = 0; restored < regenerated_trims.size();
			++restored) {
		    ON_BrepTrim &trim = *regenerated_trims[restored];
		    const OriginalTrimCurveState &state = original_states[restored];
		    brep->SetTrimCurve(trim, state.c2_index);
		    trim.SetProxyCurveDomain(state.proxy_domain);
		    trim.SetDomain(state.trim_domain);
		    trim.m_iso = state.iso;
		    trim.m_pbox = state.pbox;
		}
		discard_added_curves();
		continue;
	    }
	    apply_representation_endpoint_adjustments(brep);

	    /* The regenerated chain begins immediately after the proven native
	     * seam cut and ends immediately before it.  Preserve that traversal in
	     * the loop's cyclic storage as well: downstream pole-cut construction
	     * appends its two seam uses and singular trim at the last-to-first gap.
	     * Leaving the original rotation in place puts the one-period gap inside
	     * the array and makes an otherwise exact loop structurally invalid. */
	    std::vector<int> ordered_trim_indices;
	    ordered_trim_indices.reserve(loop.TrimCount());
	    for (int offset = 0; offset < loop.TrimCount(); ++offset)
		ordered_trim_indices.push_back(loop.m_ti[
		    (cut + 1 + offset) % loop.TrimCount()]);
	    loop.m_ti.SetCount(0);
	    for (std::vector<int>::const_iterator trim_index =
		    ordered_trim_indices.begin();
		    trim_index != ordered_trim_indices.end(); ++trim_index)
		loop.m_ti.Append(*trim_index);

	    if (record_repair)
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "unwrapped an exact full-period boundary from its 3-D STEP edge chain");
	    if (record_repair && bounded_terminal_correction) {
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "closed a source endpoint residual at a periodic seam within its measured topology tolerance");
		if (terminal_correction_distance > LocalUnits::tolerance)
		    wrapper->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Warning,
			entity_id, entity_type, "trim_pcurve",
			"source edge endpoint missed its declared topology vertex; "
			"closed the periodic seam within a densely measured local "
			"OpenNURBS tolerance");
	    }
	    if (!numerical_native_seam && geometric_native_seam) {
		if (record_repair)
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"selected a periodic boundary cut from a tolerance-proven native seam vertex");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": accepted tolerance-proven native seam for L"
			<< loop_index << "/STEP" << source_loop.m_loop_user.i
			<< " cut/winding " << cut << '/' << winding
			<< " parameter-residual="
			<< fabs(source_start[closed_direction] - native_image)
		    << " topology-tolerance=" << cut_tolerance << std::endl;
	    }
	    record_periodic_corner_repair();
	    record_representation_tolerance();
	    return true;
	}
    }
    return false;
}


bool
align_closed_surface_seam_from_trim_pair(ON_Brep *brep,
	const ON_BrepEdge &edge, const ON_BrepLoop &loop,
	const ON_Surface *surface, double tolerance,
	std::string *failure_reason)
{
    if (failure_reason)
	*failure_reason = "no closed surface direction admitted the seam pair";
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(
	const_cast<ON_Surface *>(surface));
    ON_RevSurface *revolution = ON_RevSurface::Cast(
	const_cast<ON_Surface *>(surface));
    const ON_BrepFace *source_face = loop.Face();
	if (!brep || (!nurbs && !revolution) || !source_face ||
	    edge.m_ti.Count() != 2 ||
	    !(tolerance > 0.0))
	return false;

    const ON_BrepTrim *pair[2] = {
	brep->Trim(edge.m_ti[0]), brep->Trim(edge.m_ti[1])
    };
    if (!pair[0] || !pair[1] || pair[0]->m_li != loop.m_loop_index ||
	    pair[1]->m_li != loop.m_loop_index)
	return false;

    const auto fixed_iso_direction = [](ON_Surface::ISO iso) {
	if (iso == ON_Surface::W_iso || iso == ON_Surface::E_iso)
	    return 0;
	if (iso == ON_Surface::S_iso || iso == ON_Surface::N_iso)
	    return 1;
	return -1;
    };
    int preferred_direction = -1;
    {
	/* Invalid pcurves can report the wrong iso direction.  Infer the seam's
	 * fixed coordinate from immutable 3D edge samples pulled back onto the
	 * surface.  Whole-period unwrapping prevents a legitimate seam crossing
	 * from looking like a varying coordinate. */
	brlcad::PullbackContext context;
	double parameters[5][2];
	bool pulled = true;
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    const ON_3dPoint target = edge.PointAt(
		edge.Domain().ParameterAt(fraction));
	    ON_2dPoint uv(surface->Domain(0).Mid(), surface->Domain(1).Mid());
	    ON_3dPoint lift;
	    double distance = DBL_MAX;
	    pulled = context.SurfaceClosestPoint(surface, target, uv, lift,
		distance, 0, std::max(1.0e-10, tolerance * 1.0e-6),
		tolerance) && distance <= tolerance;
	    if (!pulled)
		break;
	    for (int direction = 0; direction < 2; ++direction) {
		double parameter = uv[direction];
		if (sample > 0 && surface->IsClosed(direction)) {
		    const double period = surface->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			parameter += round((parameters[0][direction] - parameter) /
			    period) * period;
		}
		parameters[sample][direction] = parameter;
	    }
	}
	if (pulled) {
	    double normalized_range[2] = {DBL_MAX, DBL_MAX};
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		double minimum = parameters[0][direction];
		double maximum = parameters[0][direction];
		for (int sample = 1; sample < 5; ++sample) {
		    minimum = std::min(minimum, parameters[sample][direction]);
		    maximum = std::max(maximum, parameters[sample][direction]);
		}
		const double period = surface->Domain(direction).Length();
		if (period > ON_ZERO_TOLERANCE)
		    normalized_range[direction] = (maximum - minimum) / period;
	    }
	    const int exact_direction = normalized_range[0] <=
		normalized_range[1] ? 0 : 1;
	    const int varying_direction = 1 - exact_direction;
	    const double numerical_range = std::max(1.0e-8,
		1.0e-4 * normalized_range[varying_direction]);
	    if (normalized_range[exact_direction] <= numerical_range)
		preferred_direction = exact_direction;
	}
    }

    if (preferred_direction < 0) {
	preferred_direction = fixed_iso_direction(pair[0]->m_iso);
	const int second_iso_direction = fixed_iso_direction(pair[1]->m_iso);
	if (preferred_direction < 0)
	    preferred_direction = second_iso_direction;
	else if (second_iso_direction >= 0 &&
		second_iso_direction != preferred_direction)
	    return false;
    }

    /* Before iso flags have been recovered, a short seam can look nearly
     * constant in both coordinates on a doubly closed surface.  Its true
     * fixed direction is the coordinate with the smaller normalized span;
     * considering the other direction can rotate an unrelated surface seam
     * and invalidate every adjacent pcurve. */
    if (preferred_direction < 0) {
	double normalized_span[2] = {DBL_MAX, DBL_MAX};
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double period = surface->Domain(direction).Length();
	    if (!(period > ON_ZERO_TOLERANCE))
		continue;
	    double maximum_span = 0.0;
	    for (int member = 0; member < 2; ++member) {
		const ON_Interval trim_domain = pair[member]->Domain();
		const int samples = std::min(256,
		    std::max(32, pair[member]->SpanCount() * 4));
		double minimum = DBL_MAX;
		double maximum = -DBL_MAX;
		for (int sample = 0; sample <= samples; ++sample) {
		    const ON_3dPoint uv = pair[member]->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		    minimum = std::min(minimum, uv[direction]);
		    maximum = std::max(maximum, uv[direction]);
		}
		maximum_span = std::max(maximum_span, maximum - minimum);
	    }
	    normalized_span[direction] = maximum_span / period;
	}
	if (normalized_span[0] < DBL_MAX || normalized_span[1] < DBL_MAX)
	    preferred_direction = normalized_span[0] <= normalized_span[1] ? 0 : 1;
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	if (preferred_direction >= 0 && direction != preferred_direction)
	    continue;
	if (revolution) {
	    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
	    if (direction != profile_direction || !revolution->m_curve ||
		    !revolution->m_curve->IsClosed())
		continue;
	}
	const ON_Interval domain = surface->Domain(direction);
	const double period = domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

	double minimum[2] = {DBL_MAX, DBL_MAX};
	double maximum[2] = {-DBL_MAX, -DBL_MAX};
	double center[2] = {0.0, 0.0};
	for (int member = 0; member < 2; ++member) {
	    const ON_Interval trim_domain = pair[member]->Domain();
	    const int samples = std::min(256,
		std::max(32, pair[member]->SpanCount() * 4));
	    for (int sample = 0; sample <= samples; ++sample) {
		const ON_3dPoint uv = pair[member]->PointAt(
		    trim_domain.ParameterAt(static_cast<double>(sample) / samples));
		minimum[member] = std::min(minimum[member], uv[direction]);
		maximum[member] = std::max(maximum[member], uv[direction]);
		center[member] += uv[direction];
	    }
	    center[member] /= static_cast<double>(samples + 1);
	}
	const double branch_tolerance = 0.05 * period;
	if (maximum[0] - minimum[0] > branch_tolerance ||
		maximum[1] - minimum[1] > branch_tolerance ||
		fabs(fabs(center[0] - center[1]) - period) > branch_tolerance) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "trim pair did not describe opposite branches in direction "
		    << direction << " (spans " << maximum[0] - minimum[0]
		    << "/" << maximum[1] - minimum[1] << ", centers "
		    << center[0] << "/" << center[1] << ", period " << period
		    << ")";
		*failure_reason = reason.str();
	    }
	    continue;
	}

	double seam = std::max(center[0], center[1]);
	/* The supplied pcurves can be displaced by nearly the full asserted model
	 * uncertainty.  Using their average as a new surface seam then places the
	 * exact STEP edge just outside that boundary.  Refine interior edge samples
	 * from the supplied branch and use their constant exact surface parameter
	 * when the independent samples agree numerically. */
	double exact_parameters[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
	bool exact_constant = true;
	const double numerical_tolerance = std::max(1.0e-10,
	    std::min(1.0e-7, tolerance * 1.0e-6));
	brlcad::PullbackContext pullback_context;
	const ON_Interval first_domain = pair[0]->Domain();
	const ON_Interval exact_edge_domain = edge.Domain();
	for (int sample = 0; sample < 5; ++sample) {
	    const double fraction = (static_cast<double>(sample) + 1.0) / 6.0;
	    ON_3dPoint uv = pair[0]->PointAt(first_domain.ParameterAt(fraction));
	    const double edge_fraction = pair[0]->m_bRev3d ?
		1.0 - fraction : fraction;
	    const ON_3dPoint target = edge.PointAt(
		exact_edge_domain.ParameterAt(edge_fraction));
	    double distance = DBL_MAX;
	    bool pulled = refine_surface_pullback_seeded(surface, target,
		numerical_tolerance, uv, &distance);
	    if (!pulled) {
		ON_2dPoint global_uv;
		ON_3dPoint global_lift;
		pulled = pullback_context.SurfaceClosestPoint(surface, target,
		    global_uv, global_lift, distance, 0, numerical_tolerance,
		    tolerance) && distance <= tolerance;
		if (pulled) {
		    uv.Set(global_uv.x, global_uv.y, 0.0);
		    double refined_distance = DBL_MAX;
		    if (refine_surface_pullback_seeded(surface, target,
			    numerical_tolerance, uv, &refined_distance))
			distance = refined_distance;
		}
	    }
	    if (!pulled) {
		exact_constant = false;
		break;
	    }
	    exact_parameters[sample] = uv[direction] +
		round((center[0] - uv[direction]) / period) * period;
	}
	if (exact_constant) {
	    double exact_minimum = exact_parameters[0];
	    double exact_maximum = exact_parameters[0];
	    double exact_sum = exact_parameters[0];
	    for (int sample = 1; sample < 5; ++sample) {
		exact_minimum = std::min(exact_minimum, exact_parameters[sample]);
		exact_maximum = std::max(exact_maximum, exact_parameters[sample]);
		exact_sum += exact_parameters[sample];
	    }
	    const double constant_tolerance = std::max(1.0e-9,
		1.0e-7 * period);
	    if (exact_maximum - exact_minimum <= constant_tolerance)
		seam = exact_sum / 5.0;
	    else if (failure_reason)
		*failure_reason = "exact edge pullbacks were not constant in the fixed direction";
	} else if (failure_reason) {
	    *failure_reason = "exact edge pullback could not locate the candidate surface seam";
	}
	while (seam < domain.Min()) seam += period;
	while (seam > domain.Max()) seam -= period;
	const double endpoint_guard = 1.0e-7 * std::max(1.0, period);
	if (seam <= domain.Min() + endpoint_guard ||
		seam >= domain.Max() - endpoint_guard) {
	    if (failure_reason)
		*failure_reason = "candidate seam was already on the surface domain boundary";
	    continue;
	}

	ON_NurbsSurface nurbs_candidate;
	ON_RevSurface revolution_candidate;
	const ON_Surface *candidate = NULL;
	if (nurbs) {
	    nurbs_candidate = *nurbs;
	    if (!nurbs_candidate.ChangeSurfaceSeam(direction, seam)) {
		if (failure_reason)
		    *failure_reason = "NURBS surface rejected the candidate seam parameter";
		continue;
	    }
	    candidate = &nurbs_candidate;
	} else {
	    revolution_candidate = *revolution;
	    if (!revolution_candidate.m_curve ||
		    !revolution_candidate.m_curve->ChangeClosedCurveSeam(seam))
		continue;
	    candidate = &revolution_candidate;
	}
	const ON_Interval candidate_domain = candidate->Domain(direction);
	ON_Xform shift(ON_Xform::IdentityTransformation);
	shift.m_xform[direction][3] = period;

	std::vector<int> trim_indices;
	std::vector<ON_Curve *> transformed_curves;
	bool all_valid = true;
	for (int fi = 0; fi < brep->m_F.Count() && all_valid; ++fi) {
	    const ON_BrepFace &face = brep->m_F[fi];
	    if (face.m_si != source_face->m_si)
		continue;
	    for (int fli = 0; fli < face.LoopCount() && all_valid; ++fli) {
		const ON_BrepLoop *affected_loop = face.Loop(fli);
		if (!affected_loop)
		    continue;
		for (int lti = 0; lti < affected_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *trim = affected_loop->Trim(lti);
		    ON_Curve *curve = trim ? trim->DuplicateCurve() : NULL;
		    ON_BoundingBox curve_box;
		    if (!trim || !curve || !curve->Transform(shift) ||
			    !curve->GetBoundingBox(curve_box)) {
			delete curve;
			all_valid = false;
			break;
		    }
		    const double curve_center = 0.5 *
			(curve_box.m_min[direction] + curve_box.m_max[direction]);
		    if (curve_center < candidate_domain.Min() - endpoint_guard ||
			    curve_center > candidate_domain.Max() + endpoint_guard) {
			const double wrap = round((candidate_domain.Mid() - curve_center) /
			    period) * period;
			if (fabs(wrap) > ON_ZERO_TOLERANCE) {
			    ON_Xform wrap_transform(ON_Xform::IdentityTransformation);
			    wrap_transform.m_xform[direction][3] = wrap;
			    if (!curve->Transform(wrap_transform)) {
				delete curve;
				all_valid = false;
				break;
			    }
			}
		    }
		    if (!curve->ChangeDimension(2) || !curve->IsValid()) {
			delete curve;
			all_valid = false;
			break;
		    }

		    const ON_Interval trim_domain = trim->Domain();
		    const int samples = std::min(256,
			std::max(32, trim->SpanCount() * 4));
		    for (int sample = 0; sample <= samples; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) / samples);
			const ON_3dPoint original_uv = trim->PointAt(parameter);
			const ON_3dPoint transformed_uv = curve->PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint transformed_lift = candidate->PointAt(
			    transformed_uv.x, transformed_uv.y);
			if (!original_lift.IsValid() || !transformed_lift.IsValid() ||
				original_lift.DistanceTo(transformed_lift) > tolerance) {
			    all_valid = false;
			    break;
			}
		    }
		    if (!all_valid) {
			delete curve;
			break;
		    }
		    trim_indices.push_back(trim->m_trim_index);
		    transformed_curves.push_back(curve);
		}
	    }
	}
	if (!all_valid) {
	    if (failure_reason)
		*failure_reason = "surface seam change did not preserve every affected pcurve lift";
	    for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
		    curve != transformed_curves.end(); ++curve)
		delete *curve;
	    continue;
	}

	std::vector<int> c2_indices;
	c2_indices.reserve(transformed_curves.size());
	for (std::vector<ON_Curve *>::iterator curve = transformed_curves.begin();
		curve != transformed_curves.end(); ++curve) {
	    const int c2_index = brep->AddTrimCurve(*curve);
	    if (c2_index < 0)
		return false;
	    c2_indices.push_back(c2_index);
	}
	if (nurbs)
	    *nurbs = nurbs_candidate;
	else
	    *revolution = revolution_candidate;
	for (size_t i = 0; i < trim_indices.size(); ++i) {
	    if (!brep->SetTrimCurve(brep->m_T[trim_indices[i]], c2_indices[i]))
		return false;
	    brep->SetTrimIsoFlags(brep->m_T[trim_indices[i]]);
	}
	return true;
    }
    return false;
}


bool
relocate_closed_surface_loop_seam(ON_Brep *brep, int loop_index,
	int direction, double tolerance, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type, std::string *failure_reason,
	double requested_seam,
	const std::set<int> *validated_trim_loci)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count() ||
	(direction != 0 && direction != 1) || !(tolerance > 0.0))
	return false;
    ON_BrepLoop &source_loop = brep->m_L[loop_index];
    ON_BrepFace *source_face = source_loop.Face();
    ON_Surface *surface = source_face ?
	const_cast<ON_Surface *>(source_face->SurfaceOf()) : NULL;
    ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(surface);
    ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    ON_SumSurface *sum = ON_SumSurface::Cast(surface);
    const int revolution_profile_direction = revolution ?
	(revolution->m_bTransposed ? 0 : 1) : -1;
    const int revolution_angle_direction = revolution ?
	(revolution->m_bTransposed ? 1 : 0) : -1;
    const bool supported_revolution_profile = revolution &&
	direction == revolution_profile_direction && revolution->m_curve &&
	revolution->m_curve->IsClosed();
    const bool supported_revolution_angle = revolution &&
	direction == revolution_angle_direction &&
	fabs(revolution->m_angle.Length() - ON_2PI) <= ON_ZERO_TOLERANCE;
    /* SurfaceOfLinearExtrusion is represented exactly by ON_SumSurface.
     * When its swept profile is closed, moving that curve's seam is the same
     * exact parameterization operation as moving the profile seam of an
     * ON_RevSurface.  Supporting it here avoids splitting both sides of an
     * otherwise ordinary face merely because its private extrusion seam
     * crosses the supplied boundary. */
    const bool supported_sum_curve = sum && direction >= 0 && direction < 2 &&
	sum->m_curve[direction] && sum->m_curve[direction]->IsClosed();
    if ((!nurbs && !supported_revolution_profile &&
	    !supported_revolution_angle && !supported_sum_curve) || !surface ||
	    !surface->IsClosed(direction)) {
	if (failure_reason)
	    *failure_reason =
		"the loop surface does not expose a movable closed parameter seam";
	return false;
    }
    const int surface_index = source_face->m_si;
    const ON_Interval old_domain = surface->Domain(direction);
    const double period = old_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    double seam = requested_seam;
    if (std::isfinite(seam)) {
	while (seam < old_domain.Min()) seam += period;
	while (seam > old_domain.Max()) seam -= period;
    }

    /* Select the empty parameter interval from the authoritative 3-D STEP
     * edges.  A supplied pcurve can make an occupied exact interval appear
     * empty when it uses a branch which extrapolates a geometrically closed,
     * non-periodic NURBS surface.  Native closest-point projections provide
     * cyclic phases only.  On an analytically regular surface of revolution,
     * a supplied pcurve densely proven by the immediately preceding repair
     * pass against its immutable 3-D edge supplies the same occupied phases
     * without repeating dozens of global projections.  The subsequent
     * all-trim regeneration and dense validation remain the transactional
     * acceptance proof.
     *
     * A periodic face-band repair can instead request a seam which has been
     * independently proven to coincide with a supplied topology vertex.  In
     * that case every ordinary edge is required to stay on one side of the
     * requested seam by the same transactional projection below, while a
     * full-period singleton is rebuilt explicitly on the new native sides. */
    if (!std::isfinite(seam)) {
    std::vector<double> phases;
    /* Mark every cyclic phase interval traversed between adjacent exact-edge
     * samples.  Looking only at the sample points makes the largest sampling
     * gap indistinguishable from a genuinely boundary-free interval and can
     * place the new seam directly through the same edge. */
    std::vector<bool> occupied(kBoundaryParameterSearchSegments, false);
    brlcad::PullbackContext phase_pullback_context;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || !edge->EdgeCurveOf())
		    return false;
		/* A one-trim closed edge which winds the complete surface period
		 * necessarily intersects every possible seam.  It is reconstructed
		 * as an exact native-domain boundary after the seam move and must not
		 * hide the genuinely empty interval between ordinary open edges. */
		if (trim->m_vi[0] == trim->m_vi[1] &&
			edge->m_vi[0] == edge->m_vi[1])
		    continue;
		const int samples = std::min(256,
		    std::max(32, edge->SpanCount() * 4));
		const ON_Interval edge_domain = edge->Domain();
		const ON_Interval trim_domain = trim->Domain();
		const double projection_tolerance = std::max(tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		/* The immediately preceding pcurve pass either densely proved the
		 * supplied locus against the exact 3D edge or installed a regenerated
		 * pcurve which passed regenerate_trim_polyline()'s complete dense
		 * validation.  Reuse that exact parameter locus when identifying
		 * occupied phases instead of solving the same global projections. */
		const bool use_validated_pcurve = validated_trim_loci &&
		    validated_trim_loci->find(trim->m_trim_index) !=
			validated_trim_loci->end() && trim_domain.IsIncreasing();
		bool have_previous_phase = false;
		double previous_phase = 0.0;
		for (int sample = 0; sample <= samples; ++sample) {
		    if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) / samples;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trim->m_bRev3d ?
			    1.0 - fraction : fraction));
		    const ON_3dPoint validated_uv = use_validated_pcurve ?
			trim->PointAt(trim_domain.ParameterAt(fraction)) :
			ON_3dPoint::UnsetPoint;
		    ON_2dPoint uv = validated_uv.IsValid() ?
			ON_2dPoint(validated_uv.x, validated_uv.y) :
			ON_2dPoint::UnsetPoint;
		    ON_3dPoint lift;
		    double distance = DBL_MAX;
		    if (!edge_point.IsValid() ||
			(!use_validated_pcurve &&
			 (!phase_pullback_context.SurfaceClosestPoint(surface,
			     edge_point, uv, lift, distance, 0,
			     std::max(ON_ZERO_TOLERANCE,
				 projection_tolerance * 0.1),
			     projection_tolerance) ||
			  distance > projection_tolerance)) || !uv.IsValid())
			return false;
		    double phase = fmod(uv[direction] - old_domain.Min(), period);
		    if (phase < 0.0)
			phase += period;
		    phases.push_back(phase);
		    if (!have_previous_phase) {
			const int bin = std::min(kBoundaryParameterSearchSegments - 1,
			    std::max(0, static_cast<int>(floor(phase / period *
				kBoundaryParameterSearchSegments))));
			occupied[bin] = true;
			have_previous_phase = true;
			previous_phase = phase;
			continue;
		    }
		    double delta = phase - previous_phase;
		    delta -= round(delta / period) * period;
		    const int interval_samples = std::max(1,
			static_cast<int>(ceil(fabs(delta) / period *
			    kBoundaryParameterSearchSegments * 2.0)));
		    for (int interval_sample = 0;
			    interval_sample <= interval_samples; ++interval_sample) {
			double occupied_phase = previous_phase + delta *
			    static_cast<double>(interval_sample) / interval_samples;
			occupied_phase = fmod(occupied_phase, period);
			if (occupied_phase < 0.0)
			    occupied_phase += period;
			const int bin = std::min(kBoundaryParameterSearchSegments - 1,
			    std::max(0, static_cast<int>(floor(occupied_phase /
				period * kBoundaryParameterSearchSegments))));
			occupied[bin] = true;
		    }
		    previous_phase = phase;
		}
	    }
	}
    }
    if (phases.size() < 2)
	return false;
    int largest_gap_bins = 0;
    int largest_gap_end = -1;
    int current_gap_bins = 0;
    for (int bin = 0; bin < 2 * kBoundaryParameterSearchSegments; ++bin) {
	if (!occupied[bin % kBoundaryParameterSearchSegments] &&
		current_gap_bins < kBoundaryParameterSearchSegments) {
	    ++current_gap_bins;
	    if (current_gap_bins > largest_gap_bins) {
		largest_gap_bins = current_gap_bins;
		largest_gap_end = bin;
	    }
	} else {
	    current_gap_bins = 0;
	}
    }
    const double largest_gap = period * largest_gap_bins /
	static_cast<double>(kBoundaryParameterSearchSegments);
    if (largest_gap < kMinimumSeamRelocationGapFraction * period) {
	if (failure_reason)
	    *failure_reason = "the affected boundary has no empty seam interval";
	return false;
    }
    const int largest_gap_start = largest_gap_end - largest_gap_bins + 1;
    double seam_phase = fmod((largest_gap_start + 0.5 * largest_gap_bins) *
	period / kBoundaryParameterSearchSegments, period);
    if (seam_phase < 0.0)
	seam_phase += period;
    seam = old_domain.Min() + seam_phase;
    }
    const double parameter_guard = std::max(ON_ZERO_TOLERANCE,
	period * 1.0e-10);
    if (seam <= old_domain.Min() + parameter_guard ||
	    seam >= old_domain.Max() - parameter_guard)
	return false;
    /* ON_RevSurface angle relocation explicitly changes the physical angle
     * represented by the unchanged m_t domain, so its existing pcurves need
     * the matching affine parameter shift.  ChangeSurfaceSeam() and a closed
     * profile curve's ChangeClosedCurveSeam() perform their own knot/domain
     * reparameterization; applying the same shift there would move exact
     * profile coordinates a second time. */
    const double seam_parameter_shift = supported_revolution_angle ?
	old_domain.Min() - seam : 0.0;

    ON_NurbsSurface nurbs_candidate;
    ON_RevSurface revolution_candidate;
    ON_SumSurface sum_candidate;
    ON_Surface *candidate = NULL;
    if (nurbs) {
	nurbs_candidate = *nurbs;
	if (nurbs_candidate.ChangeSurfaceSeam(direction, seam) &&
		nurbs_candidate.IsValid())
	    candidate = &nurbs_candidate;
    } else if (supported_revolution_profile) {
	revolution_candidate = *revolution;
	if (revolution_candidate.m_curve &&
		revolution_candidate.m_curve->ChangeClosedCurveSeam(seam) &&
		revolution_candidate.IsValid())
	    candidate = &revolution_candidate;
    } else if (supported_sum_curve) {
	sum_candidate = *sum;
	if (sum_candidate.m_curve[direction] &&
		sum_candidate.m_curve[direction]->ChangeClosedCurveSeam(seam) &&
		sum_candidate.IsValid())
	    candidate = &sum_candidate;
    } else {
	revolution_candidate = *revolution;
	const double angle = revolution->m_angle.ParameterAt(
	    old_domain.NormalizedParameterAt(seam));
	revolution_candidate.m_angle.Set(angle, angle + ON_2PI);
	revolution_candidate.m_t.Set(old_domain.Min(), old_domain.Max());
	if (revolution_candidate.IsValid())
	    candidate = &revolution_candidate;
    }
    if (!candidate) {
	if (failure_reason)
	    *failure_reason = "openNURBS rejected the boundary-free seam";
	return false;
    }
    const ON_Interval candidate_domain = candidate->Domain(direction);
    struct Replacement {
	int trim_index;
	int original_c2_index;
	ON_Curve *seed;
	ON_Surface::ISO iso;
	ON_BrepTrim::TYPE type;
    };
    std::vector<Replacement> replacements;
    bool normalized_relocated_endpoint = false;
    bool valid = true;
    for (int fi = 0; valid && fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_si != surface_index)
	    continue;
	for (int fli = 0; valid && fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    if (!loop)
		continue;
	    for (int lti = 0; valid && lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *trim = loop->Trim(lti);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		ON_Curve *source = trim ? trim->DuplicateCurve() : NULL;
		ON_BoundingBox box;
		if (!trim || !edge || !source || !source->GetBoundingBox(box)) {
		    delete source;
		    valid = false;
		    break;
		}
		ON_Curve *seed = NULL;
		std::string seed_failure_detail;
		const bool singleton_closed_boundary = loop->TrimCount() == 1 &&
		    trim->m_vi[0] == trim->m_vi[1] &&
		    edge->m_vi[0] == edge->m_vi[1];
		if (singleton_closed_boundary) {
		    /* A full-period singleton is rebuilt by the dedicated native-seam
		     * constructor after the candidate surface is installed.  Its open
		     * coordinate is unaffected by this seam move.  Translate its
		     * complete winding by the seam-coordinate delta first, then select
		     * the nearest whole-period image.  Merely wrapping the old pcurve
		     * leaves its topology vertex on the old revolution angle after an
		     * ON_RevSurface seam move. */
		    const double center = 0.5 *
			(box.m_min[direction] + box.m_max[direction]);
		    double base_shift = seam_parameter_shift + round(
			(candidate_domain.Mid() -
			    (center + seam_parameter_shift)) / period) * period;
		    if (std::isfinite(requested_seam)) {
			/* ChangeSurfaceSeam maps the requested old parameter to the
			 * unchanged native-domain minimum.  A full-period pcurve is
			 * already unwrapped, so apply that affine shift to the whole
			 * curve and choose only the endpoint image inside the new native
			 * interval.  Centering the curve instead can select the opposite
			 * extrapolated image of a closed-but-nonperiodic NURBS surface,
			 * leaving its STEP topology vertex at the antipode. */
			const ON_3dPoint source_start = source->PointAtStart();
			double mapped_start = source_start[direction] -
			    seam + old_domain.Min();
			const int vertex_index = trim->m_vi[0];
			const ON_3dPoint topology_vertex =
			    vertex_index >= 0 && vertex_index < brep->m_V.Count() ?
			    brep->m_V[vertex_index].point :
			    ON_3dPoint::UnsetPoint;
			brlcad::PullbackContext topology_context;
			ON_2dPoint topology_uv = ON_2dPoint::UnsetPoint;
			ON_3dPoint topology_lift;
			double topology_distance = DBL_MAX;
			if (topology_vertex.IsValid() &&
				topology_context.SurfaceClosestPoint(candidate,
				    topology_vertex, topology_uv, topology_lift,
				    topology_distance, 0,
				    std::max(ON_ZERO_TOLERANCE, tolerance * 0.1),
				    tolerance) &&
				topology_distance <= tolerance)
			    mapped_start = topology_uv[direction];
			while (mapped_start < candidate_domain.Min())
			    mapped_start += period;
			while (mapped_start > candidate_domain.Max())
			    mapped_start -= period;
			base_shift = mapped_start - source_start[direction];
			if (wrapper && wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": relocating singleton T" << trim->m_trim_index
				<< " source-start=" << source_start[direction]
				<< " old-seam=" << seam << " mapped-start="
				<< mapped_start << " shift=" << base_shift
				<< " domain=" << candidate_domain.Min() << ':'
				<< candidate_domain.Max() << std::endl;
		    }
		    ON_Curve *curve = source->DuplicateCurve();
		    ON_Xform shift(ON_Xform::IdentityTransformation);
		    shift.m_xform[direction][3] = base_shift;
		    if (curve && curve->Transform(shift) &&
			    curve->ChangeDimension(2) && curve->IsValid())
			seed = curve;
		    else
			delete curve;
		} else {
		    /* ChangeSurfaceSeam is piecewise in parameter space: points below
		     * the new seam move by one period while points above it do not.  A
		     * uniform translation of a bad source pcurve therefore cannot be
		     * a reliable seed.  Project the immutable directed 3-D edge onto
		     * the candidate surface instead.  The seam was selected from an
		     * exact empty interval, so every ordinary edge must remain a
		     * continuous native-domain sequence; otherwise reject the whole
		     * transaction. */
		    const int samples = std::min(256,
			std::max(64, edge->SpanCount() * 4));
		    const ON_Interval edge_domain = edge->Domain();
		    const ON_Interval trim_domain = trim->Domain();
		    const double projection_tolerance = std::max(tolerance,
			std::max(edge->m_tolerance,
			    std::max(trim->m_tolerance[0],
				trim->m_tolerance[1])));
		    ON_3dPointArray points;
		    ON_SimpleArray<double> parameters;
		    points.Reserve(samples + 1);
		    parameters.Reserve(samples + 1);
		    brlcad::PullbackContext seed_pullback_context;
		    double previous_parameter = 0.0;
		    bool continuous = edge_domain.IsIncreasing() &&
			trim_domain.IsIncreasing();
		    for (int sample = 0; continuous && sample <= samples; ++sample) {
			if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled()) {
			    continuous = false;
			    break;
			}
			const double fraction = static_cast<double>(sample) / samples;
			const ON_3dPoint edge_point = edge->PointAt(
			    edge_domain.ParameterAt(trim->m_bRev3d ?
				1.0 - fraction : fraction));
			ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
			ON_3dPoint pulled_lift;
			double pulled_distance = DBL_MAX;
			continuous = edge_point.IsValid() &&
			    seed_pullback_context.SurfaceClosestPoint(candidate,
				edge_point, pulled_uv, pulled_lift,
				pulled_distance, 0,
				std::max(ON_ZERO_TOLERANCE,
				    projection_tolerance * 0.1),
				projection_tolerance) &&
			    pulled_distance <= projection_tolerance;
			if (!continuous) {
			    std::ostringstream detail;
			    detail << "STEP edge #" << edge->m_edge_user.i
				<< " projection failed at sample " << sample << '/'
				<< samples << " (distance " << pulled_distance
				<< ", tolerance " << projection_tolerance << ')';
			    seed_failure_detail = detail.str();
			    break;
			}
			double candidate_parameter = pulled_uv[direction];
			if (sample > 0)
			    candidate_parameter += round((previous_parameter -
				candidate_parameter) / period) * period;
			const double guard = std::max(ON_ZERO_TOLERANCE *
			    kNumericalToleranceScale, period * 1.0e-10);
			/* A requested topology-driven seam is allowed to coincide
			 * with an ordinary edge endpoint.  Its first sample has two
			 * equivalent native images; a closest-point solve commonly
			 * reports Min() even when the edge immediately departs into
			 * the image ending at Max().  Delay that one endpoint branch
			 * choice until the first interior sample proves the side.
			 * This changes only the parameter image of the same seam
			 * point.  An interior crossing still fails the transaction
			 * below and is left for the explicit shared-edge splitter. */
			if (sample == 1 && points.Count() == 1 &&
				std::isfinite(requested_seam)) {
			    ON_3dPoint *first = points.At(0);
			    if (candidate_parameter <
				    candidate_domain.Min() - guard && first) {
				(*first)[direction] = candidate_domain.Max();
				previous_parameter = candidate_domain.Max();
				candidate_parameter += period;
			    } else if (candidate_parameter >
				    candidate_domain.Max() + guard && first) {
				(*first)[direction] = candidate_domain.Min();
				previous_parameter = candidate_domain.Min();
				candidate_parameter -= period;
			    }
			}
			/* The last sample of an ordinary split boundary can be the
			 * topology vertex used to request this surface seam.  A closest
			 * point solve may report it a few parameter ulps beyond Max()
			 * after unwrapping the preceding samples, even though the native
			 * endpoint, directed edge endpoint, and STEP vertex are the same
			 * measured 3-D point.  Treat that as the native endpoint rather
			 * than an interior seam crossing, but only with all three
			 * model-space witnesses.  Do not impose a parameter-distance
			 * threshold here: a distorted but valid surface parameterization
			 * can turn a small model-space endpoint discrepancy into an
			 * arbitrary UV residual.  This is still only an endpoint branch
			 * selection; a genuinely crossing edge fails at an interior sample,
			 * and the regenerated curve is densely revalidated below. */
			const bool outside_candidate_domain =
			    candidate_parameter < candidate_domain.Min() - guard ||
			    candidate_parameter > candidate_domain.Max() + guard;
			if (outside_candidate_domain &&
				(sample == 0 || sample == samples) &&
				std::isfinite(requested_seam)) {
			    const double native_endpoint =
				candidate_parameter < candidate_domain.Min() ?
				candidate_domain.Min() : candidate_domain.Max();
			    const int endpoint = sample == 0 ? 0 : 1;
			    const int vertex_index = trim->m_vi[endpoint];
			    double endpoint_tolerance = projection_tolerance;
			    if (vertex_index >= 0 &&
				    vertex_index < brep->m_V.Count())
				endpoint_tolerance = std::max(endpoint_tolerance,
				    brep->m_V[vertex_index].m_tolerance);
			    ON_3dPoint native_uv(pulled_uv.x, pulled_uv.y, 0.0);
			    native_uv[direction] = native_endpoint;
			    const ON_3dPoint native_lift =
				candidate->PointAt(native_uv.x, native_uv.y);
			    const ON_3dPoint topology_vertex =
				vertex_index >= 0 &&
				vertex_index < brep->m_V.Count() ?
				brep->m_V[vertex_index].point :
				ON_3dPoint::UnsetPoint;
			    if (native_lift.IsValid() && edge_point.IsValid() &&
				    topology_vertex.IsValid() &&
				    native_lift.DistanceTo(edge_point) <=
					endpoint_tolerance &&
				    edge_point.DistanceTo(topology_vertex) <=
					endpoint_tolerance &&
				    native_lift.DistanceTo(topology_vertex) <=
					endpoint_tolerance) {
				candidate_parameter = native_endpoint;
				normalized_relocated_endpoint = true;
			    }
			}
			if (candidate_parameter < candidate_domain.Min() - guard ||
				candidate_parameter > candidate_domain.Max() + guard) {
			    std::ostringstream detail;
			    detail << "STEP edge #" << edge->m_edge_user.i
				<< " crossed the candidate native seam at sample "
				<< sample << '/' << samples << " (parameter "
				<< candidate_parameter << ", domain "
				<< candidate_domain.Min() << ':'
				<< candidate_domain.Max() << ')';
			    seed_failure_detail = detail.str();
			    continuous = false;
			    break;
			}
			pulled_uv[direction] = std::max(candidate_domain.Min(),
			    std::min(candidate_domain.Max(), candidate_parameter));
			previous_parameter = pulled_uv[direction];
			points.Append(ON_3dPoint(pulled_uv.x, pulled_uv.y, 0.0));
			parameters.Append(trim_domain.ParameterAt(fraction));
		    }
		    if (continuous && points.Count() >= 2) {
			ON_PolylineCurve *curve = new ON_PolylineCurve(points,
			    parameters);
			if (curve && curve->ChangeDimension(2) && curve->IsValid())
			    seed = curve;
			else
			    delete curve;
		    }
		}
		if (!seed) {
		    delete source;
		    valid = false;
		    if (failure_reason)
			*failure_reason = "an exact edge could not be seeded continuously inside the relocated surface domain" +
			    (seed_failure_detail.empty() ? std::string() :
				std::string(": ") + seed_failure_detail);
		    break;
		}
		/* SetTrimCurve() changes only the trim's curve index; it does not
		 * mutate or remove the existing m_C2 entry.  Retain that index for
		 * transactional rollback instead of keeping a complete duplicate of
		 * every source pcurve and adding another duplicate on failure.  On
		 * giant multi-loop faces the former rollback path retained thousands
		 * of unreferenced curves until final BREP compaction. */
		const int original_c2_index = trim->m_c2i;
		delete source;
		replacements.push_back({trim->m_trim_index, original_c2_index, seed,
		    trim->m_iso, trim->m_type});
	    }
	}
    }
    if (!valid) {
	for (std::vector<Replacement>::iterator replacement = replacements.begin();
		replacement != replacements.end(); ++replacement)
	{
	    delete replacement->seed;
	}
	return false;
    }

    std::vector<int> curve_indices(replacements.size(), -1);
    for (size_t index = 0; index < replacements.size(); ++index) {
	curve_indices[index] = brep->AddTrimCurve(replacements[index].seed);
	if (curve_indices[index] < 0)
	    return false;
	replacements[index].seed = NULL;
    }
    ON_NurbsSurface original_nurbs;
    ON_RevSurface original_revolution;
    ON_SumSurface original_sum;
    if (nurbs) {
	original_nurbs = *nurbs;
	*nurbs = nurbs_candidate;
    } else if (sum) {
	original_sum = *sum;
	*sum = sum_candidate;
    } else {
	original_revolution = *revolution;
	*revolution = revolution_candidate;
    }
    for (size_t index = 0; index < replacements.size(); ++index) {
	if (!brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		curve_indices[index]))
	    return false;
	brep->SetTrimIsoFlags(brep->m_T[replacements[index].trim_index]);
    }
    bool regenerated = true;
    std::string regeneration_failure;
    int failed_trim_index = -1;
    int failed_step_edge = 0;
    for (size_t index = 0; regenerated && index < replacements.size(); ++index) {
	ON_BrepTrim &trim = brep->m_T[replacements[index].trim_index];
	ON_BrepEdge *edge = trim.Edge();
	ON_NurbsCurve edge_nurbs;
	const double trim_tolerance = edge ? std::max(tolerance,
	    std::max(edge->m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1]))) : tolerance;
	ON_BrepLoop *owner_loop = trim.Loop();
	const bool singleton_closed_boundary = edge && owner_loop &&
	    owner_loop->TrimCount() == 1 &&
	    trim.m_vi[0] == trim.m_vi[1] &&
	    edge->m_vi[0] == edge->m_vi[1];
	regenerated = false;
	if (singleton_closed_boundary) {
	    const ON_3dPoint seed = trim.PointAt(trim.Domain().Mid());
	    for (int closed_direction = 0; !regenerated &&
		    closed_direction < 2; ++closed_direction) {
		if (!surface->IsClosed(closed_direction) || !seed.IsValid())
		    continue;
		regenerated = regenerate_native_seam_periodic_boundary(brep,
		    *owner_loop, surface, closed_direction, wrapper, entity_id,
		    entity_type, seed[1 - closed_direction], false);
	    }
	    if (!regenerated) {
		/* ChangeSurfaceSeam() is piecewise in parameter space.  A closed
		 * singleton whose arbitrary STEP vertex differs from the requested
		 * seam therefore cannot be retained by a uniform translation of its
		 * old pcurve: that can leave the endpoint on the old surface phase
		 * even when the interior locus is lift-equivalent.  Project the
		 * authoritative topology vertex onto the relocated surface and try
		 * both directed one-period endpoint images.  The shared regenerator
		 * pulls the immutable 3-D edge, preserves those exact endpoint
		 * images, and densely validates the complete directed curve before
		 * installing it. */
		const int topology_vertex_index = trim.m_vi[0];
		const ON_3dPoint topology_vertex =
		    topology_vertex_index >= 0 &&
		    topology_vertex_index < brep->m_V.Count() ?
		    brep->m_V[topology_vertex_index].point :
		    ON_3dPoint::UnsetPoint;
		brlcad::PullbackContext vertex_context;
		ON_2dPoint topology_uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint topology_lift;
		double topology_distance = DBL_MAX;
		const bool projected_vertex = topology_vertex.IsValid() &&
		    vertex_context.SurfaceClosestPoint(surface, topology_vertex,
			topology_uv, topology_lift, topology_distance, 0,
			std::max(ON_ZERO_TOLERANCE, trim_tolerance * 0.1),
			trim_tolerance) &&
		    topology_distance <= trim_tolerance;
		const ON_Interval relocated_domain = surface->Domain(direction);
		if (projected_vertex && relocated_domain.IsIncreasing() && edge &&
			edge->GetNurbForm(edge_nurbs)) {
		    ON_3dPoint required_start(topology_uv.x, topology_uv.y, 0.0);
		    const ON_Interval trim_domain = trim.Domain();
		    for (int winding = 0; !regenerated && winding < 2; ++winding) {
			ON_3dPoint required_end = required_start;
			required_end[direction] += winding == 0 ?
			    relocated_domain.Length() : -relocated_domain.Length();
			std::unique_ptr<ON_Curve> isocurve(
			    new ON_LineCurve(required_start, required_end));
			bool exact_isocurve = trim_domain.IsIncreasing() &&
			    isocurve && isocurve->ChangeDimension(2) &&
			    isocurve->SetDomain(trim_domain.Min(), trim_domain.Max()) &&
			    isocurve->IsValid();
			const ON_3dPoint start_lift = exact_isocurve ?
			    surface->PointAt(required_start.x, required_start.y) :
			    ON_3dPoint::UnsetPoint;
			const ON_3dPoint end_lift = exact_isocurve ?
			    surface->PointAt(required_end.x, required_end.y) :
			    ON_3dPoint::UnsetPoint;
			exact_isocurve = exact_isocurve &&
			    start_lift.IsValid() && end_lift.IsValid() &&
			    start_lift.DistanceTo(topology_vertex) <= trim_tolerance &&
			    end_lift.DistanceTo(topology_vertex) <= trim_tolerance;
			double maximum_locus_distance = 0.0;
			int failed_locus_sample = -1;
			ON_Arc exact_arc;
			const ON_Curve *edge_curve = edge->EdgeCurveOf();
			const bool exact_circle = edge_curve &&
			    edge_curve->IsArc(NULL, &exact_arc, trim_tolerance) &&
			    exact_arc.IsCircle();
			for (int sample = 0; exact_isocurve &&
				sample <= kDenseValidationSegments; ++sample) {
			    if ((sample & 63) == 0 &&
				    brlcad::PullbackWorkCancelled()) {
				exact_isocurve = false;
				break;
			    }
			    const double fraction = static_cast<double>(sample) /
				kDenseValidationSegments;
			    const ON_3dPoint uv = isocurve->PointAt(
				trim_domain.ParameterAt(fraction));
			    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
			    const ON_Interval edge_domain = edge_nurbs.Domain();
			    const ON_3dPoint corresponding =
				edge_nurbs.PointAt(edge_domain.ParameterAt(
				    trim.m_bRev3d ? 1.0 - fraction : fraction));
			    double locus_distance =
				lift.IsValid() && corresponding.IsValid() ?
				lift.DistanceTo(corresponding) : DBL_MAX;
			    double edge_parameter = 0.0;
			    bool closest = locus_distance <= trim_tolerance;
			    if (!closest && exact_circle && lift.IsValid()) {
				locus_distance = std::min(locus_distance,
				    lift.DistanceTo(exact_arc.ClosestPointTo(lift)));
				closest = locus_distance <= trim_tolerance;
			    }
			    if (!closest && lift.IsValid() &&
				    ON_NurbsCurve_GetClosestPoint(&edge_parameter,
					&edge_nurbs, lift)) {
				locus_distance = std::min(locus_distance,
				    lift.DistanceTo(
					edge_nurbs.PointAt(edge_parameter)));
				closest = locus_distance <= trim_tolerance;
			    }
			    maximum_locus_distance = std::max(
				maximum_locus_distance, locus_distance);
			    exact_isocurve = closest;
			    if (!exact_isocurve)
				failed_locus_sample = sample;
			}
			if (!exact_isocurve) {
			    if (wrapper->Verbose())
				std::cerr << entity_type << " #" << entity_id
				    << ": relocated singleton exact isocurve T"
				    << trim.m_trim_index << " winding "
				    << (winding == 0 ? 1 : -1)
				    << " rejected at sample " << failed_locus_sample
				    << '/' << kDenseValidationSegments
				    << " maximum-distance="
				    << maximum_locus_distance << " endpoint-distances="
				    << start_lift.DistanceTo(topology_vertex) << '/'
				    << end_lift.DistanceTo(topology_vertex)
				    << " tolerance=" << trim_tolerance << std::endl;
			    continue;
			}
			const double measured_isocurve_distance = std::max(
			    maximum_locus_distance,
			    std::max(start_lift.DistanceTo(topology_vertex),
				end_lift.DistanceTo(topology_vertex)));
			const double existing_tolerance = std::max(
			    LocalUnits::tolerance,
			    std::max(edge->m_tolerance,
				std::max(trim.m_tolerance[0],
				    trim.m_tolerance[1])));
			if (!wrapper->ImportOptions().exact &&
				measured_isocurve_distance > existing_tolerance) {
			    const double adjusted_tolerance =
				measured_isocurve_distance *
				    kRegenerationToleranceSafety;
			    if (adjusted_tolerance > trim_tolerance)
				continue;
			    edge->m_tolerance = std::max(edge->m_tolerance,
				adjusted_tolerance);
			    trim.m_tolerance[0] = std::max(trim.m_tolerance[0],
				adjusted_tolerance);
			    trim.m_tolerance[1] = std::max(trim.m_tolerance[1],
				adjusted_tolerance);
			    if (topology_vertex_index >= 0 &&
				    topology_vertex_index < brep->m_V.Count())
				brep->m_V[topology_vertex_index].m_tolerance =
				    std::max(
					brep->m_V[topology_vertex_index].m_tolerance,
					adjusted_tolerance);
			    wrapper->RecordDiagnostic(
				brlcad::step::DiagnosticSeverity::Warning,
				entity_id, entity_type, "trim_pcurve",
				"source periodic edge/surface endpoint exceeded the "
				"declared tolerance; used a densely measured local "
				"OpenNURBS tolerance");
			    wrapper->RecordRepair(entity_id, entity_type,
				"trim_pcurve",
				"adjusted one periodic boundary tolerance to measured "
				"source geometry");
			}
			ON_Curve *curve = isocurve.release();
			const int c2 = brep->AddTrimCurve(curve);
			if (c2 < 0 || !brep->SetTrimCurve(trim, c2)) {
			    if (c2 < 0)
				delete curve;
			    continue;
			}
			brep->SetTrimIsoFlags(trim);
			regenerated = true;
		    }
		    for (int winding = 0; !regenerated && winding < 2; ++winding) {
			ON_3dPoint required_end = required_start;
			required_end[direction] += winding == 0 ?
			    relocated_domain.Length() : -relocated_domain.Length();
			std::string exact_edge_failure;
			ON_Curve *generated_curve = NULL;
			/* This temporary curve is split again at the native seam.
			 * Generate it well inside the already measured acceptance
			 * tolerance so splitting its polyline cannot expose an
			 * interpolation error at the tolerance boundary. */
			const double generation_tolerance = LocalUnits::tolerance;
			regenerated = regenerate_trim_polyline(brep, trim, surface,
			    edge_nurbs, generation_tolerance, &exact_edge_failure, NULL,
			    &required_start, &required_end, true, wrapper, true,
			    &generated_curve);
			if (regenerated && generated_curve) {
			    const int c2 = brep->AddTrimCurve(generated_curve);
			    if (c2 < 0 || !brep->SetTrimCurve(trim, c2)) {
				if (c2 < 0)
				    delete generated_curve;
				regenerated = false;
				exact_edge_failure =
				    "could not install the validated temporary pcurve";
			    } else {
				brep->SetTrimIsoFlags(trim);
			    }
			}
			if (!regenerated && wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": relocated full-period singleton T"
				<< trim.m_trim_index << " winding "
				<< (winding == 0 ? 1 : -1)
				<< " exact-edge regeneration rejected: "
				<< exact_edge_failure << std::endl;
		    }
		}
	    }
	    if (!regenerated) {
		/* The seam was intentionally aligned to a vertex of the sibling
		 * boundary.  A closed singleton can have a different arbitrary STEP
		 * topology vertex, so it cannot yet be represented as one native-side
		 * pcurve.  Retain its exact translated full-period winding only when
		 * every directed sample still lifts to the immutable edge.  The
		 * periodic-band pass immediately restarts and materializes the native
		 * seam crossing as an explicit shared edge split; this temporary
		 * state is never accepted as a final OpenNURBS loop. */
		const ON_Curve *temporary = trim.TrimCurveOf();
		const ON_Curve *original = replacements[index].original_c2_index >= 0 &&
		    replacements[index].original_c2_index < brep->m_C2.Count() ?
		    brep->m_C2[replacements[index].original_c2_index] : NULL;
		const ON_Surface *original_surface = nurbs ?
		    static_cast<const ON_Surface *>(&original_nurbs) :
		    (sum ? static_cast<const ON_Surface *>(&original_sum) :
		    static_cast<const ON_Surface *>(&original_revolution));
		const ON_Interval temporary_domain = temporary ?
		    temporary->Domain() : ON_Interval::EmptyInterval;
		const ON_Interval original_domain = original ?
		    original->Domain() : ON_Interval::EmptyInterval;
		int winding_direction = -1;
		if (temporary && original && temporary_domain.IsIncreasing() &&
			original_domain.IsIncreasing()) {
		    const ON_3dPoint start = temporary->PointAtStart();
		    const ON_3dPoint end = temporary->PointAtEnd();
		    for (int candidate_direction = 0;
			    candidate_direction < 2; ++candidate_direction) {
			if (!surface->IsClosed(candidate_direction))
			    continue;
			const double candidate_period =
			    surface->Domain(candidate_direction).Length();
			const double parameter_tolerance = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    kPeriodicParameterSnapFraction *
				std::max(1.0, candidate_period));
			if (candidate_period > ON_ZERO_TOLERANCE &&
				fabs(fabs(end[candidate_direction] -
				    start[candidate_direction]) -
				    candidate_period) <= parameter_tolerance &&
				fabs(end[1 - candidate_direction] -
				    start[1 - candidate_direction]) <=
				    parameter_tolerance) {
			    winding_direction = candidate_direction;
			    break;
			}
		    }
		}
		/* This is an affine parameterization change on one immutable
		 * surface.  Prove the retained pcurve by comparing its new-surface
		 * lift to the original pcurve's old-surface lift at dense identical
		 * parameters.  Comparing both to the edge at the same normalized
		 * parameter is incorrect here: fitted pullbacks preserve the edge
		 * locus but are not required to share its parameterization. */
		const double exact_reparameterization_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    LocalUnits::tolerance);
		/* A one-edge closed loop is not necessarily a full-period boundary.
		 * A contractible hole can use one closed STEP edge as well.  Moving a
		 * surface seam through an empty parameter interval maps that loop by
		 * the same exact piecewise parameterization as every ordinary edge;
		 * rejecting it merely because its net winding is zero rolls back an
		 * otherwise valid topology-driven seam move.
		 *
		 * Admit that case only when the relocated endpoints still lift to the
		 * closed STEP topology vertex, their parameter gap is numerical, and
		 * the complete curve stays inside one native interval with a span
		 * strictly smaller than a period.  Dense identical-parameter lift
		 * comparison below then proves that no geometry moved.  A wrapped or
		 * ambiguous full-period curve still follows the existing explicit
		 * native-seam split path. */
		const ON_Interval moved_domain = surface->Domain(direction);
		const double moved_period = moved_domain.Length();
		const double moved_parameter_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    kPeriodicParameterSnapFraction *
			std::max(1.0, moved_period));
		const ON_3dPoint temporary_start = temporary ?
		    temporary->PointAtStart() : ON_3dPoint::UnsetPoint;
		const ON_3dPoint temporary_end = temporary ?
		    temporary->PointAtEnd() : ON_3dPoint::UnsetPoint;
		const int topology_vertex_index = trim.m_vi[0];
		const ON_3dPoint topology_vertex =
		    topology_vertex_index >= 0 &&
		    topology_vertex_index < brep->m_V.Count() ?
		    brep->m_V[topology_vertex_index].point :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint temporary_start_lift =
		    temporary_start.IsValid() ?
		    surface->PointAt(temporary_start.x, temporary_start.y) :
		    ON_3dPoint::UnsetPoint;
		const ON_3dPoint temporary_end_lift =
		    temporary_end.IsValid() ?
		    surface->PointAt(temporary_end.x, temporary_end.y) :
		    ON_3dPoint::UnsetPoint;
		const bool contractible_candidate = winding_direction < 0 &&
		    moved_domain.IsIncreasing() && moved_period > ON_ZERO_TOLERANCE &&
		    temporary_start.IsValid() && temporary_end.IsValid() &&
		    topology_vertex.IsValid() &&
		    fabs(temporary_end[direction] -
			temporary_start[direction]) <= moved_parameter_tolerance &&
		    temporary_start_lift.IsValid() &&
		    temporary_end_lift.IsValid() &&
		    temporary_start_lift.DistanceTo(topology_vertex) <=
			trim_tolerance &&
		    temporary_end_lift.DistanceTo(topology_vertex) <=
			trim_tolerance;
		bool exact_temporary =
		    winding_direction >= 0 || contractible_candidate;
		double maximum_temporary_distance = 0.0;
		int maximum_temporary_sample = -1;
		double minimum_moved_parameter = DBL_MAX;
		double maximum_moved_parameter = -DBL_MAX;
		for (int sample = 0; exact_temporary &&
			sample <= kDenseValidationSegments; ++sample) {
		    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled()) {
			exact_temporary = false;
			break;
		    }
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint original_uv = original->PointAt(
			original_domain.ParameterAt(fraction));
		    const ON_3dPoint uv = temporary->PointAt(
			temporary_domain.ParameterAt(fraction));
		    const ON_3dPoint original_lift = original_surface->PointAt(
			original_uv.x, original_uv.y);
		    const ON_3dPoint relocated_lift = surface->PointAt(uv.x, uv.y);
		    const double distance =
			original_lift.IsValid() && relocated_lift.IsValid() ?
			original_lift.DistanceTo(relocated_lift) : DBL_MAX;
		    minimum_moved_parameter = std::min(minimum_moved_parameter,
			uv[direction]);
		    maximum_moved_parameter = std::max(maximum_moved_parameter,
			uv[direction]);
		    if (distance > maximum_temporary_distance) {
			maximum_temporary_distance = distance;
			maximum_temporary_sample = sample;
		    }
		    const bool contractible_native_domain =
			!contractible_candidate ||
			(uv.IsValid() &&
			 uv[direction] >=
			    moved_domain.Min() - moved_parameter_tolerance &&
			 uv[direction] <=
			    moved_domain.Max() + moved_parameter_tolerance);
		    exact_temporary = contractible_native_domain &&
			distance <= exact_reparameterization_tolerance;
		}
		const bool contractible_temporary =
		    contractible_candidate && exact_temporary &&
		    maximum_moved_parameter - minimum_moved_parameter <
			moved_period - moved_parameter_tolerance;
		if (winding_direction < 0)
		    exact_temporary = contractible_temporary;
		regenerated = exact_temporary;
		if (regenerated && contractible_temporary) {
		    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
			"preserved an exact contractible closed boundary during "
			"periodic surface seam relocation");
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": preserved contractible singleton T"
			    << trim.m_trim_index << " during periodic surface seam "
			    << "relocation; span="
			    << maximum_moved_parameter - minimum_moved_parameter
			    << " period=" << moved_period << std::endl;
		}
		if (!regenerated) {
		    regeneration_failure =
			"could not retain an exact full-period singleton pending its native seam split";
		    if (wrapper->Verbose()) {
			std::cerr << entity_type << " #" << entity_id
			    << ": relocated full-period singleton T"
			    << trim.m_trim_index << " rejected winding-direction="
			    << winding_direction << " uv=" << temporary_start.x
			    << ':' << temporary_start.y << "->"
			    << temporary_end.x << ':' << temporary_end.y
			    << " maximum-distance=" << maximum_temporary_distance
			    << " sample=" << maximum_temporary_sample
			    << " tolerance=" << exact_reparameterization_tolerance
			    << std::endl;
		    }
		}
	    }
	} else {
	    /* The temporary curve was projected from the exact directed edge onto
	     * the candidate surface and already preserves every loop join.  A
	     * seam move in one parameter direction must not let the generic
	     * pullback choose a different lift-equivalent image in the other
	     * closed direction: doing so tears an otherwise valid torus band by
	     * exactly one period.  Pin both projected endpoint images while the
	     * interior is regenerated from the immutable 3-D edge.  The helper
	     * still densely validates the complete curve and both endpoint lifts,
	     * so these are continuity constraints rather than accepted geometry. */
	    ON_3dPoint required_start = trim.PointAtStart();
	    ON_3dPoint required_end = trim.PointAtEnd();
	    /* A closest-point projection of an exact edge endpoint on the new
	     * surface seam may return Domain().Min() even when the directed
	     * interior of that edge approaches the lift-equivalent Domain().Max()
	     * image (or conversely).  Pinning the isolated endpoint image then
	     * asks exact pullback to jump a whole period in its final segment.
	     *
	     * Resolve only this native-boundary ambiguity.  The adjacent interior
	     * sample chooses the side, while the raw candidate lift, immutable
	     * directed edge endpoint, and STEP topology vertex independently prove
	     * the exchanged min/max image before it becomes a pullback constraint. */
	    const ON_Interval trim_domain = trim.Domain();
	    const ON_Interval edge_domain = edge ?
		edge->Domain() : ON_Interval::EmptyInterval;
	    if (trim_domain.IsIncreasing() && edge_domain.IsIncreasing()) {
		ON_3dPoint *required[2] = {&required_start, &required_end};
		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    const double interior_fraction =
			endpoint == 0 ? 1.0 / 64.0 : 63.0 / 64.0;
		    const ON_3dPoint interior = trim.PointAt(
			trim_domain.ParameterAt(interior_fraction));
		    const int vertex_index = trim.m_vi[endpoint];
		    const ON_3dPoint topology_vertex =
			vertex_index >= 0 && vertex_index < brep->m_V.Count() ?
			brep->m_V[vertex_index].point :
			ON_3dPoint::UnsetPoint;
		    const ON_3dPoint edge_endpoint = edge->PointAt(
			edge_domain[trim.m_bRev3d ? 1 - endpoint : endpoint]);
		    for (int closed_direction = 0; closed_direction < 2;
			    ++closed_direction) {
			if (!surface->IsClosed(closed_direction) ||
				!interior.IsValid())
			    continue;
			const ON_Interval closed_domain =
			    surface->Domain(closed_direction);
			const double closed_period = closed_domain.Length();
			const double endpoint_parameter_guard = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    closed_period * kPeriodicParameterSnapFraction);
			if (!closed_domain.IsIncreasing() ||
				!(closed_period > ON_ZERO_TOLERANCE))
			    continue;
			double alternative = ON_UNSET_VALUE;
			if (fabs((*required[endpoint])[closed_direction] -
				closed_domain.Min()) <= endpoint_parameter_guard &&
				interior[closed_direction] >
				    closed_domain.Mid())
			    alternative = closed_domain.Max();
			else if (fabs((*required[endpoint])[closed_direction] -
				closed_domain.Max()) <= endpoint_parameter_guard &&
				interior[closed_direction] <
				    closed_domain.Mid())
			    alternative = closed_domain.Min();
			if (!std::isfinite(alternative))
			    continue;
			const bool selected_minimum =
			    fabs(alternative - closed_domain.Min()) <=
				endpoint_parameter_guard;
			ON_3dPoint endpoint_candidate = *required[endpoint];
			endpoint_candidate[closed_direction] = alternative;
			const ON_3dPoint lift =
			    surface->PointAt(endpoint_candidate.x,
				endpoint_candidate.y);
			if (!lift.IsValid() || !topology_vertex.IsValid() ||
				!edge_endpoint.IsValid() ||
				lift.DistanceTo(topology_vertex) >
				    trim_tolerance ||
				lift.DistanceTo(edge_endpoint) >
				    trim_tolerance ||
				edge_endpoint.DistanceTo(topology_vertex) >
				    trim_tolerance)
			    continue;
			*required[endpoint] = endpoint_candidate;
			normalized_relocated_endpoint = true;
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": relocated trim T" << trim.m_trim_index
				<< "/STEP edge #" << edge->m_edge_user.i
				<< " selected native "
				<< (selected_minimum ?
				    "minimum" : "maximum")
				<< " image for endpoint " << endpoint
				<< " in closed direction " << closed_direction
				<< std::endl;
		    }
		}
	    }
	    regenerated = edge && edge->GetNurbForm(edge_nurbs) &&
		regenerate_trim_polyline(brep, trim, surface, edge_nurbs,
		    trim_tolerance, &regeneration_failure, NULL,
		    &required_start, &required_end, true, wrapper, true);
	}
	if (!regenerated) {
	    failed_trim_index = trim.m_trim_index;
	    failed_step_edge = edge ? edge->m_edge_user.i : 0;
	}
    }
    if (!regenerated) {
	/* Restore the pre-relocation surface and pcurves.  Newly added candidate
	 * curves remain unreferenced and are compacted with the BREP later. */
	if (nurbs)
	    *nurbs = original_nurbs;
	else if (sum)
	    *sum = original_sum;
	else
	    *revolution = original_revolution;
	if (failure_reason) {
	    std::ostringstream reason;
	    reason << "exact-edge regeneration on the relocated surface failed";
	    if (failed_trim_index >= 0)
		reason << " for trim T" << failed_trim_index;
	    if (failed_step_edge > 0)
		reason << "/STEP edge #" << failed_step_edge;
	    reason << ": " << regeneration_failure;
	    *failure_reason = reason.str();
	}
	for (size_t index = 0; index < replacements.size(); ++index) {
	    const int original_index = replacements[index].original_c2_index;
	    if (original_index >= 0 && original_index < brep->m_C2.Count()) {
		brep->SetTrimCurve(brep->m_T[replacements[index].trim_index],
		    original_index);
		brep->m_T[replacements[index].trim_index].m_iso = replacements[index].iso;
		brep->m_T[replacements[index].trim_index].m_type = replacements[index].type;
	    }
	}
	return false;
    }
    if (normalized_relocated_endpoint)
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "normalized a topology-proven relocated periodic edge endpoint "
	    "onto its native surface seam");
    return true;
}


/* Report whether an ordinary open boundary crosses the native seam of a
 * geometrically closed surface.  Parameter samples are obtained from the
 * immutable 3-D edge and unwrapped in traversal order, so a bad supplied
 * pcurve cannot hide the crossing.  This is only a trigger for the
 * transactional whole-surface seam relocation above; it never changes
 * topology or accepts geometry by itself. */
bool
exact_open_trim_crosses_native_seam(const ON_BrepTrim &trim,
	const ON_Surface *surface, int direction, double tolerance,
	brlcad::PullbackContext &pullback_context)
{
    const ON_BrepEdge *edge = trim.Edge();
    if (!surface || !edge || direction < 0 || direction > 1 ||
	    !surface->IsClosed(direction) ||
	    trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
	    trim.m_vi[0] == trim.m_vi[1] ||
	    edge->m_vi[0] == edge->m_vi[1])
	return false;
    /* A trim which is an OpenNURBS seam in one surface direction can still
     * cross the native seam in the other direction (for example, a short
     * meridian edge crossing a torus profile cut).  Exclude only the
     * coordinate held fixed by the trim's established ISO classification. */
    int fixed_direction = -1;
    if (trim.m_iso == ON_Surface::W_iso ||
	    trim.m_iso == ON_Surface::E_iso)
	fixed_direction = 0;
    else if (trim.m_iso == ON_Surface::S_iso ||
	    trim.m_iso == ON_Surface::N_iso)
	fixed_direction = 1;
    if (trim.m_type == ON_BrepTrim::seam &&
	    fixed_direction == direction)
	return false;
    const ON_Interval surface_domain = surface->Domain(direction);
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge->Domain();
    if (!surface_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing())
	return false;
    const double period = surface_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    const int samples = std::min(256, std::max(64, edge->SpanCount() * 4));
    const double projection_tolerance = std::max(tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
    const double seam_guard = std::max(ON_ZERO_TOLERANCE *
	kNumericalToleranceScale, period * 1.0e-10);
    const bool surface_closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    const ON_Interval surface_domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    bool have_previous = false;
    double previous = 0.0;
    for (int sample = 0; sample <= samples; ++sample) {
	if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) / samples;
	const ON_3dPoint edge_point = edge->PointAt(edge_domain.ParameterAt(
	    trim.m_bRev3d ? 1.0 - fraction : fraction));
	const ON_3dPoint trim_uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	const ON_2dPoint seed(trim_uv.x, trim_uv.y);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() || !seed.IsValid())
	    return false;
	/* The supplied pcurve is only a numerical seed: acceptance is still based
	 * on projecting the immutable 3-D STEP edge and validating its lift.  A
	 * local seeded solve keeps successive samples on the pcurve's coherent
	 * periodic branch and avoids rebuilding the same surface span index for
	 * every trim.  If that local proof fails, retain the historical global
	 * closest-point search so a bad supplied pcurve cannot hide a crossing. */
	const double solver_tolerance = std::max(ON_ZERO_TOLERANCE,
	    projection_tolerance * 0.1);
	bool projected = pullback_context.SurfaceClosestPointFromSeed(surface,
	    edge_point, seed, uv, lift, distance, projection_tolerance,
	    surface_closed, surface_domains, solver_tolerance) &&
	    distance <= projection_tolerance;
	if (!projected) {
	    uv = ON_2dPoint::UnsetPoint;
	    distance = DBL_MAX;
	    projected = pullback_context.SurfaceClosestPoint(surface, edge_point,
		uv, lift, distance, 0, solver_tolerance,
		projection_tolerance) && distance <= projection_tolerance;
	}
	if (!projected)
	    return false;
	double current = uv[direction];
	if (have_previous)
	    current += round((previous - current) / period) * period;
	if (have_previous) {
	    const double minimum = std::min(previous, current);
	    const double maximum = std::max(previous, current);
	    const double first_seam = surface_domain.Min() +
		ceil((minimum - surface_domain.Min()) / period) * period;
	    if (first_seam > minimum + seam_guard &&
		    first_seam < maximum - seam_guard)
		return true;
	    /* An exact interior sample on the seam is also a crossing even when
	     * floating-point unwrapping places both neighboring samples on the
	     * same side.  Endpoints are harmless: adjacent trims may legitimately
	     * meet on the native boundary. */
	    const double nearest_seam = surface_domain.Min() +
		round((current - surface_domain.Min()) / period) * period;
	    if (sample < samples &&
		    fabs(current - previous) > seam_guard &&
		    fabs(current - nearest_seam) <= seam_guard)
		return true;
	}
	previous = current;
	have_previous = true;
    }
    return false;
}


/* Detect the complementary failure mode to an ordinary edge crossing the
 * native seam.  Some writers keep a loop literally joined by giving one open
 * pcurve endpoint each of two lift-equivalent periodic images.  A fitted
 * curve between those endpoints can then acquire a false full-period winding
 * even though the immutable STEP edge stays on one coherent surface branch.
 *
 * This predicate is only an authorization to try the transactional private
 * surface-seam relocation above.  Require all of the independent witnesses:
 * the source pcurve has exactly one period of endpoint travel, both adjacent
 * trims require those literal endpoint images at the same STEP vertices, the
 * complete directed 3-D edge projects to a branch with negligible winding,
 * and an interior source-pcurve lift is not on the edge locus.  The seam
 * mover must still find an empty interval, regenerate every affected trim
 * from its exact edge, and validate all lifts before anything is retained. */
bool
exact_open_trim_has_spurious_periodic_winding(const ON_BrepTrim &trim,
	const ON_Surface *surface, int direction, double tolerance,
	brlcad::PullbackContext &pullback_context)
{
    const ON_BrepEdge *edge = trim.Edge();
    const ON_BrepLoop *loop = trim.Loop();
    if (!surface || !edge || !loop || direction < 0 || direction > 1 ||
	    !surface->IsClosed(direction) ||
	    trim.m_vi[0] < 0 || trim.m_vi[1] < 0 ||
	    trim.m_vi[0] == trim.m_vi[1] ||
	    edge->m_vi[0] == edge->m_vi[1] || loop->TrimCount() < 2)
	return false;

    const ON_Interval surface_domain = surface->Domain(direction);
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge->Domain();
    const double period = surface_domain.Length();
    if (!surface_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing() || !(period > ON_ZERO_TOLERANCE))
	return false;

    const double parameter_guard = std::max(ON_ZERO_TOLERANCE *
	kNumericalToleranceScale, period * 1.0e-8);
    const ON_3dPoint source_start = trim.PointAt(trim_domain.Min());
    const ON_3dPoint source_end = trim.PointAt(trim_domain.Max());
    if (!source_start.IsValid() || !source_end.IsValid())
	return false;
    const double source_delta = source_end[direction] -
	source_start[direction];
    if (fabs(fabs(source_delta) - period) > parameter_guard)
	return false;

    /* The period jump must be a real loop constraint, not merely an isolated
	 * malformed curve endpoint which ordinary exact-edge regeneration can
	 * replace. */
    const int offset = loop->IndexOfTrim(trim);
    const ON_BrepTrim *previous = offset >= 0 ? loop->Trim((offset +
	loop->TrimCount() - 1) % loop->TrimCount()) : NULL;
    const ON_BrepTrim *next = offset >= 0 ?
	loop->Trim((offset + 1) % loop->TrimCount()) : NULL;
    if (!previous || !next || previous->m_vi[1] != trim.m_vi[0] ||
	    next->m_vi[0] != trim.m_vi[1] ||
	    fabs(previous->PointAtEnd()[direction] -
		source_start[direction]) > parameter_guard ||
	    fabs(next->PointAtStart()[direction] -
		source_end[direction]) > parameter_guard)
	return false;

    const double projection_tolerance = std::max(tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
    const bool surface_closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    const ON_Interval surface_domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    const int samples = std::min(256, std::max(64,
	edge->SpanCount() * 4));
    bool have_projected_parameter = false;
    double projected_start = 0.0;
    double projected_previous = 0.0;
    for (int sample = 0; sample <= samples; ++sample) {
	if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) / samples;
	const ON_3dPoint edge_point = edge->PointAt(
	    edge_domain.ParameterAt(trim.m_bRev3d ?
		1.0 - fraction : fraction));
	const ON_3dPoint source_uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	if (!edge_point.IsValid() || !source_uv.IsValid())
	    return false;
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	const ON_2dPoint seed(source_uv.x, source_uv.y);
	const double solver_tolerance = std::max(ON_ZERO_TOLERANCE,
	    projection_tolerance * 0.1);
	bool projected = pullback_context.SurfaceClosestPointFromSeed(surface,
	    edge_point, seed, uv, lift, distance, projection_tolerance,
	    surface_closed, surface_domains, solver_tolerance) &&
	    distance <= projection_tolerance;
	if (!projected) {
	    uv = ON_2dPoint::UnsetPoint;
	    distance = DBL_MAX;
	    projected = pullback_context.SurfaceClosestPoint(surface,
		edge_point, uv, lift, distance, 0, solver_tolerance,
		projection_tolerance) && distance <= projection_tolerance;
	}
	if (!projected)
	    return false;
	double parameter = uv[direction];
	if (have_projected_parameter)
	    parameter += round((projected_previous - parameter) / period) *
		period;
	else
	    projected_start = parameter;
	projected_previous = parameter;
	have_projected_parameter = true;
    }
    const double projected_delta = projected_previous - projected_start;
    /* The exact edge must stay on one phase.  A genuine helical or
	 * near-full-period edge is not evidence for moving the surface seam. */
    if (fabs(projected_delta) > 0.25 * period ||
	    fabs(fabs(source_delta - projected_delta) - period) >
		4.0 * parameter_guard)
	return false;

    ON_NurbsCurve edge_nurbs;
    if (!edge->GetNurbForm(edge_nurbs))
	return false;
    bool source_off_edge = false;
    for (int sample = 1; !source_off_edge && sample < samples; ++sample) {
	if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) / samples;
	const ON_3dPoint source_uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	const ON_3dPoint source_lift = source_uv.IsValid() ?
	    surface->PointAt(source_uv.x, source_uv.y) :
	    ON_3dPoint::UnsetPoint;
	double edge_parameter = 0.0;
	/* A failed distance query is absence of evidence, not proof that the
	 * authored pcurve leaves the immutable edge locus. */
	if (!source_lift.IsValid() ||
	    !ON_NurbsCurve_GetClosestPoint(&edge_parameter, &edge_nurbs,
		source_lift))
	    return false;
	if (source_lift.DistanceTo(edge_nurbs.PointAt(edge_parameter)) >
		projection_tolerance)
	source_off_edge = true;
    }
    return source_off_edge;
}

} /* namespace step_brep_detail */
