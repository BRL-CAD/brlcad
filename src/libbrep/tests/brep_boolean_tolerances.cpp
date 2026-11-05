/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include <cmath>

#include "bu/app.h"
#include "bu/log.h"

#include "vmath.h"
#include "brep.h"
#include "brep/boolean.h"


static ON_Brep *
make_sphere(const ON_3dPoint &center, double radius)
{
    return ON_BrepSphere(ON_Sphere(center, radius));
}


static bool
has_measured_tolerances(const ON_Brep &brep)
{
    for (int vertex_index = 0; vertex_index < brep.m_V.Count(); ++vertex_index) {
	const double tolerance = brep.m_V[vertex_index].m_tolerance;
	if (!(tolerance >= 0.0) || !std::isfinite(tolerance) ||
	    tolerance == MAX_FASTF) {
	    return false;
	}
    }

    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
	const double tolerance = brep.m_E[edge_index].m_tolerance;
	if (!(tolerance >= 0.0) || !std::isfinite(tolerance) ||
	    tolerance == MAX_FASTF) {
	    return false;
	}
    }

    for (int trim_index = 0; trim_index < brep.m_T.Count(); ++trim_index) {
	const ON_BrepTrim &trim = brep.m_T[trim_index];
	if (!(trim.m_tolerance[0] >= 0.0) ||
	    !(trim.m_tolerance[1] >= 0.0) ||
	    !std::isfinite(trim.m_tolerance[0]) ||
	    !std::isfinite(trim.m_tolerance[1]) ||
	    trim.m_tolerance[0] == MAX_FASTF ||
	    trim.m_tolerance[1] == MAX_FASTF)
	{
	    return false;
	}
    }

    return true;
}


static int
check_boolean(
    const ON_Brep *first,
    const ON_Brep *second,
    op_type operation,
    const char *description)
{
    ON_Brep result;
    if (ON_Boolean(&result, first, second, operation) != BREP_BOOLEAN_SUCCESS) {
	bu_log("%s: Boolean evaluation failed\n", description);
	return 1;
    }

    if (!result.IsValid() || !result.IsSolid() ||
	!result.IsValidTolerancesAndFlags() || !has_measured_tolerances(result))
    {
	bu_log("%s: result does not have measured valid tolerances\n", description);
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    ON_Brep *first = make_sphere(ON_3dPoint::Origin, 10.0);
    ON_Brep *disjoint = make_sphere(ON_3dPoint(30.0, 30.0, 30.0), 10.0);
    if (first == NULL || disjoint == NULL) {
	delete first;
	delete disjoint;
	return 1;
    }

    int failures = 0;
    failures += check_boolean(first, disjoint, BOOLEAN_UNION,
	"disjoint union");
    failures += check_boolean(first, disjoint, BOOLEAN_DIFF,
	"disjoint difference");
    failures += check_boolean(first, disjoint, BOOLEAN_XOR,
	"disjoint exclusive-or");

    delete first;
    delete disjoint;
    return failures;
}
