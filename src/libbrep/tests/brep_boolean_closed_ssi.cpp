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
#include "brep/boolean.h"


static const double CLOSED_SSI_RADIUS = 1000.0;
static const double CLOSED_SSI_CENTER_DISTANCE = 1700.0;


static ON_Brep *
make_sphere(const ON_3dPoint &center)
{
    return ON_BrepSphere(ON_Sphere(center, CLOSED_SSI_RADIUS));
}


static int
check_closed_ssi_union()
{
    ON_Brep *first = make_sphere(ON_3dPoint::Origin);
    ON_Brep *second = make_sphere(ON_3dPoint(0.0,
	-CLOSED_SSI_CENTER_DISTANCE, 0.0));
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	delete first;
	delete second;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, first, second, BOOLEAN_UNION);
    delete first;
    delete second;

    const bool valid = result.IsValid();
    const bool solid = result.IsSolid();
    const bool tolerances_valid = result.IsValidTolerancesAndFlags();
    if (status != BREP_BOOLEAN_SUCCESS || !valid || !solid ||
	!tolerances_valid)
    {
	bu_log("closed SSI union is invalid: status %d, valid %d, solid %d, "
	    "tolerances %d\n", status, valid, solid, tolerances_valid);
	return 1;
    }

    if (!ON_BrepPointInside(ON_3dPoint::Origin, &result) ||
	!ON_BrepPointInside(ON_3dPoint(0.0, -CLOSED_SSI_CENTER_DISTANCE,
	0.0), &result))
    {
	bu_log("closed SSI union does not contain both sphere centers\n");
	return 1;
    }

    return 0;
}


static int
check_shallow_sphere_union()
{
    static const double radius = 2.75184;
    static const double center_distance = 5.5;
    ON_Brep *first = ON_BrepSphere(ON_Sphere(ON_3dPoint::Origin, radius));
    ON_Brep *second = ON_BrepSphere(ON_Sphere(
	ON_3dPoint(center_distance, 0.0, 0.0), radius));
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	delete first;
	delete second;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, first, second, BOOLEAN_UNION);
    delete first;
    delete second;

    const bool valid = result.IsValid();
    const bool solid = result.IsSolid();
    const bool tolerances_valid = result.IsValidTolerancesAndFlags();
    if (status != BREP_BOOLEAN_SUCCESS || result.m_F.Count() != 2 ||
	!valid || !solid || !tolerances_valid)
    {
	bu_log("shallow sphere union is invalid: status %d, faces %d, valid "
	    "%d, solid %d, tolerances %d\n", status, result.m_F.Count(),
	    valid, solid, tolerances_valid);
	return 1;
    }

    if (!ON_BrepPointInside(ON_3dPoint::Origin, &result) ||
	!ON_BrepPointInside(ON_3dPoint(center_distance, 0.0, 0.0), &result))
    {
	bu_log("shallow sphere union does not contain both sphere centers\n");
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_closed_ssi_union() + check_shallow_sphere_union();
}
