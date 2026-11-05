/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#ifndef LIBBREP_BOOLEAN_ENDPOINT_PAIR_H
#define LIBBREP_BOOLEAN_ENDPOINT_PAIR_H

#include <cmath>

#include "brep.h"
#include "brep/intersect.h"

#include "brep_defines.h"


struct BooleanBoundaryEndpointHit {
    ON_3dPoint m_point;
    int m_boundary_curve_index;
    double m_boundary_parameter;
    double m_curve_parameter;
};


static bool
boolean_curve_parameter_is_endpoint(
    double curve_parameter,
    const ON_Interval &curve_domain,
    double parameter_tolerance)
{
    ON_Interval domain = curve_domain;
    domain.MakeIncreasing();
    if (!domain.IsIncreasing() || !std::isfinite(curve_parameter) ||
	!std::isfinite(parameter_tolerance) || parameter_tolerance < 0.0)
    {
	return false;
    }

    return std::fabs(curve_parameter - domain.Min()) <= parameter_tolerance ||
	std::fabs(curve_parameter - domain.Max()) <= parameter_tolerance;
}


static bool
boolean_curve_hits_are_endpoint_only(
    const ON_SimpleArray<double> &curve_parameters,
    const ON_Interval &curve_domain,
    double parameter_tolerance)
{
    for (int parameter_index = 0;
	 parameter_index < curve_parameters.Count(); ++parameter_index)
    {
	if (!boolean_curve_parameter_is_endpoint(curve_parameters[parameter_index],
		curve_domain, parameter_tolerance))
	{
	    return false;
	}
    }

    return true;
}


static bool
boolean_find_exact_boundary_endpoint_hit(
    const ON_Curve *curve,
    double curve_parameter,
    const ON_SimpleArray<ON_Curve *> &boundary,
    double intersection_tolerance,
    BooleanBoundaryEndpointHit &hit,
    bool allow_tangent = false)
{
    /* Parameter-space tolerances are derived from the surface metric and can
     * legitimately be smaller than ON_ZERO_TOLERANCE on high-scale surfaces.
     * Reject only a nonpositive tolerance; imposing the global OpenNURBS
     * constant here discards otherwise exact endpoint incidences. */
    if (curve == NULL || boundary.Count() < 1 ||
	!std::isfinite(intersection_tolerance) ||
	intersection_tolerance <= 0.0)
    {
	return false;
    }

    ON_Interval curve_domain = curve->Domain();
    curve_domain.MakeIncreasing();
    if (!curve_domain.IsIncreasing() ||
	!boolean_curve_parameter_is_endpoint(curve_parameter, curve_domain,
	    ON_ZERO_TOLERANCE))
    {
	return false;
    }

    const ON_3dPoint endpoint = curve->PointAt(curve_parameter);
    if (!endpoint.IsValid()) {
	return false;
    }

    bool found = false;
    for (int boundary_index = 0; boundary_index < boundary.Count();
	 ++boundary_index)
    {
	const ON_Curve *boundary_curve = boundary[boundary_index];
	if (boundary_curve == NULL) {
	    return false;
	}

	ON_ClassArray<ON_PX_EVENT> events;
	if (!ON_Intersect(endpoint, *boundary_curve, events,
		intersection_tolerance))
	{
	    continue;
	}
	if (events.Count() != 1) {
	    return false;
	}

	const ON_PX_EVENT &event = events[0];
	const ON_Interval boundary_domain = boundary_curve->Domain();
	if (event.m_type != ON_PX_EVENT::pcx_point ||
	    !boundary_domain.Includes(event.m_b[0]) ||
	    !event.m_A.IsValid() || !event.m_B.IsValid() ||
	    event.m_A.DistanceTo(endpoint) > ON_ZERO_TOLERANCE ||
	    event.m_B.DistanceTo(endpoint) > ON_ZERO_TOLERANCE)
	{
	    return false;
	}

	const ON_3dPoint boundary_start = boundary_curve->PointAtStart();
	const ON_3dPoint boundary_end = boundary_curve->PointAtEnd();
	if (!boundary_start.IsValid() || !boundary_end.IsValid() ||
	    event.m_B.DistanceTo(boundary_start) <= intersection_tolerance ||
	    event.m_B.DistanceTo(boundary_end) <= intersection_tolerance)
	{
	    return false;
	}

	const ON_3dVector curve_tangent = curve->TangentAt(curve_parameter);
	const ON_3dVector boundary_tangent =
	    boundary_curve->TangentAt(event.m_b[0]);
    if (curve_tangent.Length() <= ON_ZERO_TOLERANCE ||
	    boundary_tangent.Length() <= ON_ZERO_TOLERANCE ||
	    (!allow_tangent &&
	     curve_tangent.IsParallelTo(boundary_tangent, ANGLE_TOL) != 0))
	{
	    return false;
	}

	if (found) {
	    return false;
	}

	hit.m_point = event.m_B;
	hit.m_boundary_curve_index = boundary_index;
	hit.m_boundary_parameter = event.m_b[0];
	hit.m_curve_parameter = curve_parameter;
	found = true;
    }

    return found;
}


static bool
boolean_find_certified_open_curve_endpoint_pair(
    const ON_Curve *curve,
    const ON_SimpleArray<ON_Curve *> &boundary,
    double intersection_tolerance,
    BooleanBoundaryEndpointHit hits[2],
    bool allow_tangent = false,
    bool allow_same_boundary = false)
{
    if (curve == NULL || hits == NULL) {
	return false;
    }

    ON_Interval curve_domain = curve->Domain();
    curve_domain.MakeIncreasing();
    if (!curve_domain.IsIncreasing()) {
	return false;
    }

    const ON_3dPoint curve_start = curve->PointAtStart();
    const ON_3dPoint curve_end = curve->PointAtEnd();
    if (!curve_start.IsValid() || !curve_end.IsValid() ||
	curve_start.DistanceTo(curve_end) <= ON_ZERO_TOLERANCE ||
	!boolean_find_exact_boundary_endpoint_hit(curve, curve_domain.Min(),
	    boundary, intersection_tolerance, hits[0], allow_tangent) ||
	!boolean_find_exact_boundary_endpoint_hit(curve, curve_domain.Max(),
	    boundary, intersection_tolerance, hits[1], allow_tangent))
    {
	return false;
    }

    if (allow_tangent && !allow_same_boundary &&
	hits[0].m_boundary_curve_index == hits[1].m_boundary_curve_index)
    {
	return false;
    }

    return hits[0].m_point.DistanceTo(hits[1].m_point) >
	intersection_tolerance;
}

#endif
