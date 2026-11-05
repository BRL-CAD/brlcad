/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#ifndef LIBBREP_PULLBACK_SEAM_ENDPOINT_H
#define LIBBREP_PULLBACK_SEAM_ENDPOINT_H

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "brep.h"


static double
pullback_seam_parameter_tolerance(const ON_Interval &domain)
{
    const double scale = std::max(1.0, std::max(std::fabs(domain.Min()),
	std::max(std::fabs(domain.Max()), std::fabs(domain.Length()))));
    return std::max(ON_ZERO_TOLERANCE, 1024.0 * DBL_EPSILON * scale);
}


static int
pullback_seam_boundary_side(const ON_Interval &domain, double parameter)
{
    if (!std::isfinite(parameter))
	return 0;
    const double tolerance = pullback_seam_parameter_tolerance(domain);
    const bool at_minimum = std::fabs(parameter - domain.Min()) <= tolerance;
    const bool at_maximum = std::fabs(parameter - domain.Max()) <= tolerance;
    if (at_minimum == at_maximum)
	return 0;
    return at_minimum ? -1 : 1;
}


static double
pullback_native_surface_parameter(const ON_Surface *surface, int direction,
    double parameter)
{
    const ON_Interval domain = surface->Domain(direction);
    if (!surface->IsClosed(direction) || domain.Includes(parameter))
	return parameter;
    const double period = domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return parameter;
    double offset = std::fmod(parameter - domain.Min(), period);
    if (offset < 0.0)
	offset += period;
    return domain.Min() + offset;
}


/* A closed model curve may be represented on a singly periodic surface by an
 * open UV curve whose endpoints are on opposite copies of the same seam.  A
 * tolerance-bounded closest-point walk can stop each endpoint at a slightly
 * different value of the non-seam parameter, leaving the pushed-up edge open.
 *
 * Canonicalize only the unambiguous case: exactly one closed parameter
 * direction spans opposite native boundaries, and using one of the two
 * already validated non-seam endpoint parameters makes both seam lifts agree
 * with the authoritative model-curve endpoint.  No averaging or unconstrained
 * endpoint motion is allowed, and the complete returned curve is validated by
 * the caller after this local correction. */
static bool
pullback_normalize_closed_curve_seam_endpoints(const ON_Surface *surface,
    const ON_Curve *model_curve, ON_3dPointArray &points, double tolerance)
{
    if (!surface || !model_curve || points.Count() < 2 ||
	    !model_curve->IsClosed() || !(tolerance > 0.0) ||
	    !std::isfinite(tolerance))
	return false;

    const ON_3dPoint model_start = model_curve->PointAtStart();
    const ON_3dPoint model_end = model_curve->PointAtEnd();
    if (!model_start.IsValid() || !model_end.IsValid() ||
	    model_start.DistanceTo(model_end) > tolerance)
	return false;

    const ON_2dPoint start(points[0].x, points[0].y);
    const ON_2dPoint end(points[points.Count() - 1].x,
	points[points.Count() - 1].y);
    if (!start.IsValid() || !end.IsValid())
	return false;

    int seam_direction = -1;
    int start_side = 0;
    int end_side = 0;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	ON_Interval domain = surface->Domain(direction);
	domain.MakeIncreasing();
	if (!domain.IsIncreasing())
	    continue;
	const int candidate_start_side = pullback_seam_boundary_side(domain,
	    start[direction]);
	const int candidate_end_side = pullback_seam_boundary_side(domain,
	    end[direction]);
	if (candidate_start_side == 0 ||
		candidate_end_side != -candidate_start_side)
	    continue;
	if (seam_direction >= 0)
	    return false;
	seam_direction = direction;
	start_side = candidate_start_side;
	end_side = candidate_end_side;
    }
    if (seam_direction < 0)
	return false;

    ON_Interval seam_domain = surface->Domain(seam_direction);
    seam_domain.MakeIncreasing();
    const int other_direction = 1 - seam_direction;
    const double candidates[2] = {start[other_direction],
	end[other_direction]};
    ON_2dPoint best_start = ON_2dPoint::UnsetPoint;
    ON_2dPoint best_end = ON_2dPoint::UnsetPoint;
    double best_distance = DBL_MAX;
    double best_displacement = DBL_MAX;

    for (int candidate_index = 0; candidate_index < 2; ++candidate_index) {
	ON_2dPoint candidate_start(start);
	ON_2dPoint candidate_end(end);
	candidate_start[seam_direction] = start_side < 0 ?
	    seam_domain.Min() : seam_domain.Max();
	candidate_end[seam_direction] = end_side < 0 ?
	    seam_domain.Min() : seam_domain.Max();
	candidate_start[other_direction] = candidates[candidate_index];
	candidate_end[other_direction] = candidates[candidate_index];

	const ON_3dPoint start_lift = surface->PointAt(
	    pullback_native_surface_parameter(surface, 0, candidate_start.x),
	    pullback_native_surface_parameter(surface, 1, candidate_start.y));
	const ON_3dPoint end_lift = surface->PointAt(
	    pullback_native_surface_parameter(surface, 0, candidate_end.x),
	    pullback_native_surface_parameter(surface, 1, candidate_end.y));
	if (!start_lift.IsValid() || !end_lift.IsValid())
	    continue;
	const double start_distance = start_lift.DistanceTo(model_start);
	const double end_distance = end_lift.DistanceTo(model_start);
	const double maximum_distance = std::max(start_distance, end_distance);
	if (!std::isfinite(maximum_distance) || maximum_distance > tolerance)
	    continue;

	const double displacement =
	    std::fabs(candidate_start[other_direction] -
		start[other_direction]) +
	    std::fabs(candidate_end[other_direction] - end[other_direction]);
	if (maximum_distance < best_distance ||
		(maximum_distance == best_distance &&
		 displacement < best_displacement)) {
	    best_start = candidate_start;
	    best_end = candidate_end;
	    best_distance = maximum_distance;
	    best_displacement = displacement;
	}
    }

    if (!best_start.IsValid() || !best_end.IsValid())
	return false;
    points[0].x = best_start.x;
    points[0].y = best_start.y;
    points[points.Count() - 1].x = best_end.x;
    points[points.Count() - 1].y = best_end.y;
    return true;
}


/* On a nonperiodic surface the pullback of a closed model curve must also be
 * closed.  Independent bounded closest-point solves can nevertheless leave
 * its two UV endpoints slightly apart.  Collapse that gap only when the
 * complete straight UV bridge lifts inside the model-space tolerance of the
 * authoritative closed endpoint.  Keeping one of the two observed endpoint
 * parameters avoids inventing an averaged parameter on a curved surface. */
static bool
pullback_normalize_closed_curve_direct_endpoints(const ON_Surface *surface,
    const ON_Curve *model_curve, ON_3dPointArray &points, double tolerance)
{
    if (!surface || !model_curve || points.Count() < 2 ||
	!model_curve->IsClosed() || !(tolerance > 0.0) ||
	!std::isfinite(tolerance))
	return false;

    for (int direction = 0; direction < 2; ++direction) {
	if (surface->IsClosed(direction) || surface->IsPeriodic(direction))
	    return false;
    }

    const ON_3dPoint model_start = model_curve->PointAtStart();
    const ON_3dPoint model_end = model_curve->PointAtEnd();
    const ON_2dPoint start(points[0].x, points[0].y);
    const ON_2dPoint end(points[points.Count() - 1].x,
	points[points.Count() - 1].y);
    if (!model_start.IsValid() || !model_end.IsValid() ||
	!start.IsValid() || !end.IsValid() ||
	model_start.DistanceTo(model_end) > tolerance)
	return false;

    static const int bridge_sample_count = 16;
    for (int sample = 0; sample <= bridge_sample_count; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    bridge_sample_count;
	const ON_2dPoint parameter = (1.0 - fraction) * start + fraction * end;
	const ON_3dPoint lifted = surface->PointAt(parameter.x, parameter.y);
	if (!lifted.IsValid() || lifted.DistanceTo(model_start) > tolerance ||
	    lifted.DistanceTo(model_end) > tolerance)
	    return false;
    }

    const ON_3dPoint start_lift = surface->PointAt(start.x, start.y);
    const ON_3dPoint end_lift = surface->PointAt(end.x, end.y);
    if (!start_lift.IsValid() || !end_lift.IsValid())
	return false;
    const double start_error = std::max(start_lift.DistanceTo(model_start),
	start_lift.DistanceTo(model_end));
    const double end_error = std::max(end_lift.DistanceTo(model_start),
	end_lift.DistanceTo(model_end));
    const ON_2dPoint canonical = start_error <= end_error ? start : end;
    points[0].x = canonical.x;
    points[0].y = canonical.y;
    points[points.Count() - 1].x = canonical.x;
    points[points.Count() - 1].y = canonical.y;
    return true;
}


static bool
pullback_normalize_closed_curve_endpoints(const ON_Surface *surface,
    const ON_Curve *model_curve, ON_3dPointArray &points, double tolerance)
{
    return pullback_normalize_closed_curve_seam_endpoints(surface,
	model_curve, points, tolerance) ||
	pullback_normalize_closed_curve_direct_endpoints(surface, model_curve,
	    points, tolerance);
}

#endif
