/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"

#include "brep.h"

#include "../boolean_endpoint_pair.h"


static const double ENDPOINT_TOLERANCE = 1.0e-6;
static const double PARAMETER_TOLERANCE = 1.0e-9;


static void
free_boundary(ON_SimpleArray<ON_Curve *> &boundary)
{
    for (int curve_index = 0; curve_index < boundary.Count();
	 ++curve_index)
    {
	delete boundary[curve_index];
    }
    boundary.Empty();
}


static bool
append_line(
    ON_SimpleArray<ON_Curve *> &boundary,
    const ON_3dPoint &start,
    const ON_3dPoint &end)
{
    ON_LineCurve *curve = new ON_LineCurve(start, end);
    if (curve == NULL) {
	return false;
    }
    boundary.Append(curve);
    return true;
}


static bool
make_square_boundary(ON_SimpleArray<ON_Curve *> &boundary)
{
    const ON_3dPoint lower_left(0.0, 0.0, 0.0);
    const ON_3dPoint lower_right(4.0, 0.0, 0.0);
    const ON_3dPoint upper_right(4.0, 4.0, 0.0);
    const ON_3dPoint upper_left(0.0, 4.0, 0.0);

    return append_line(boundary, lower_left, lower_right) &&
	append_line(boundary, lower_right, upper_right) &&
	append_line(boundary, upper_right, upper_left) &&
	append_line(boundary, upper_left, lower_left);
}


static int
check_endpoint_pair(
    const char *description,
    const ON_3dPoint &start,
    const ON_3dPoint &end,
    bool expected)
{
    ON_SimpleArray<ON_Curve *> boundary;
    if (!make_square_boundary(boundary)) {
	free_boundary(boundary);
	return 1;
    }

    ON_LineCurve curve(start, end);
    BooleanBoundaryEndpointHit hits[2];
    const bool actual = boolean_find_certified_open_curve_endpoint_pair(
	&curve, boundary, ENDPOINT_TOLERANCE, hits);
    free_boundary(boundary);

    if (actual == expected) {
	return 0;
    }

    bu_log("%s returned %d instead of %d\n", description, actual, expected);
    return 1;
}


static int
check_valid_transverse_pair()
{
    ON_SimpleArray<ON_Curve *> boundary;
    if (!make_square_boundary(boundary)) {
	free_boundary(boundary);
	return 1;
    }

    ON_LineCurve curve(ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 4.0, 0.0));
    BooleanBoundaryEndpointHit hits[2];
    const bool found = boolean_find_certified_open_curve_endpoint_pair(
	&curve, boundary, ENDPOINT_TOLERANCE, hits);
    const ON_Interval domain = curve.Domain();
    free_boundary(boundary);

    if (!found || hits[0].m_boundary_curve_index != 0 ||
	hits[1].m_boundary_curve_index != 2 ||
	std::fabs(hits[0].m_curve_parameter - domain.Min()) >
	PARAMETER_TOLERANCE ||
	std::fabs(hits[1].m_curve_parameter - domain.Max()) >
	PARAMETER_TOLERANCE)
    {
	bu_log("valid transverse endpoint pair was not certified\n");
	return 1;
    }

    return 0;
}


static int
check_interior_hit_rejected()
{
    ON_LineCurve curve(ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 4.0, 0.0));
    ON_SimpleArray<double> parameters;
    const ON_Interval domain = curve.Domain();
    parameters.Append(domain.Min());
    parameters.Append(domain.Mid());
    parameters.Append(domain.Max());

    if (boolean_curve_hits_are_endpoint_only(parameters, domain,
	PARAMETER_TOLERANCE))
    {
	bu_log("interior curve hit was accepted as an endpoint hit\n");
	return 1;
    }

    return 0;
}


static int
check_tangent_same_boundary_opt_in()
{
    ON_SimpleArray<ON_Curve *> boundary;
    if (!make_square_boundary(boundary)) {
	free_boundary(boundary);
	return 1;
    }

    ON_LineCurve curve(ON_3dPoint(1.0, 0.0, 0.0),
	ON_3dPoint(3.0, 0.0, 0.0));
    BooleanBoundaryEndpointHit hits[2];
    const bool tangent_only = boolean_find_certified_open_curve_endpoint_pair(
	&curve, boundary, ENDPOINT_TOLERANCE, hits, true);
    const bool paired_seam = boolean_find_certified_open_curve_endpoint_pair(
	&curve, boundary, ENDPOINT_TOLERANCE, hits, true, true);
    free_boundary(boundary);

    if (tangent_only || !paired_seam ||
	hits[0].m_boundary_curve_index != 0 ||
	hits[1].m_boundary_curve_index != 0)
    {
	bu_log("same-boundary tangent opt-in returned %d/%d\n",
	    tangent_only, paired_seam);
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    return check_valid_transverse_pair() +
	check_endpoint_pair("near endpoint pair",
	    ON_3dPoint(1.0, ENDPOINT_TOLERANCE * 0.5, 0.0),
	    ON_3dPoint(1.0, 4.0 - ENDPOINT_TOLERANCE * 0.5, 0.0), false) +
	check_endpoint_pair("tangent endpoint pair",
	    ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(3.0, 0.0, 0.0), false) +
	check_endpoint_pair("vertex endpoint pair",
	    ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 4.0, 0.0), false) +
	check_endpoint_pair("unpaired endpoint",
	    ON_3dPoint(1.0, 0.0, 0.0), ON_3dPoint(1.0, 2.0, 0.0), false) +
	check_interior_hit_rejected() +
	check_tangent_same_boundary_opt_in();
}
