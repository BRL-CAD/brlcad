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


static const double OUTER_MINIMUM = 0.0;
static const double OUTER_MAXIMUM = 2.0;
static const double NESTED_MINIMUM = 0.2;
static const double NESTED_MAXIMUM = 1.8;
static const double NESTED_TOP = 1.6;
static const double OVERLAP_MINIMUM = 1.0;
static const double OVERLAP_MAXIMUM = 3.0;


static ON_Brep *
make_box(
    double minimum_x,
    double minimum_y,
    double minimum_z,
    double maximum_x,
    double maximum_y,
    double maximum_z)
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(minimum_x, minimum_y, minimum_z),
	ON_3dPoint(maximum_x, minimum_y, minimum_z),
	ON_3dPoint(maximum_x, maximum_y, minimum_z),
	ON_3dPoint(minimum_x, maximum_y, minimum_z),
	ON_3dPoint(minimum_x, minimum_y, maximum_z),
	ON_3dPoint(maximum_x, minimum_y, maximum_z),
	ON_3dPoint(maximum_x, maximum_y, maximum_z),
	ON_3dPoint(minimum_x, maximum_y, maximum_z)
    };
    return ON_BrepBox(corners);
}


static int
check_union(
    const char *case_name,
    ON_Brep *first,
    ON_Brep *second,
    const ON_3dPoint &first_material_point,
    const ON_3dPoint &second_material_point,
    const ON_3dPoint &shared_material_point,
    const ON_3dPoint &exterior_point)
{
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	bu_log("%s Boolean inputs are invalid\n", case_name);
	delete first;
	delete second;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, first, second, BOOLEAN_UNION);
    delete first;
    delete second;

    const bool result_valid = result.IsValid();
    const bool result_solid = result.IsSolid();
    const bool result_tolerances_valid = result.IsValidTolerancesAndFlags();
    if (status != BREP_BOOLEAN_SUCCESS || !result_valid || !result_solid ||
	!result_tolerances_valid)
    {
	bu_log("%s union result is invalid: status %d, valid %d, solid %d, tolerances %d\n",
	    case_name, status, result_valid, result_solid, result_tolerances_valid);
	return 1;
    }
    if (!ON_BrepPointInside(first_material_point, &result) ||
	!ON_BrepPointInside(second_material_point, &result) ||
	!ON_BrepPointInside(shared_material_point, &result) ||
	ON_BrepPointInside(exterior_point, &result))
    {
	bu_log("%s union result has incorrect containment\n", case_name);
	return 1;
    }

    return 0;
}


static int
check_nested_shared_face_union()
{
    ON_Brep *outer = make_box(OUTER_MINIMUM, OUTER_MINIMUM, OUTER_MINIMUM,
	OUTER_MAXIMUM, OUTER_MAXIMUM, OUTER_MAXIMUM);
    ON_Brep *nested = make_box(NESTED_MINIMUM, OUTER_MINIMUM, NESTED_MINIMUM,
	NESTED_MAXIMUM, NESTED_TOP, NESTED_MAXIMUM);
    const ON_3dPoint outer_material_point(0.1, 1.9, 0.1);
    const ON_3dPoint nested_material_point(1.0, 0.5, 1.0);
    const ON_3dPoint shared_material_point(1.0, 0.01, 1.0);
    const ON_3dPoint exterior_point(2.1, 1.0, 1.0);
    return check_union("nested shared-face union", outer, nested, outer_material_point,
	nested_material_point, shared_material_point, exterior_point);
}


static int
check_overlapping_shared_face_union()
{
    ON_Brep *first = make_box(OUTER_MINIMUM, OUTER_MINIMUM, OUTER_MINIMUM,
	OUTER_MAXIMUM, OUTER_MAXIMUM, OUTER_MAXIMUM);
    ON_Brep *second = make_box(OVERLAP_MINIMUM, OUTER_MINIMUM,
	OVERLAP_MINIMUM, OVERLAP_MAXIMUM, OUTER_MAXIMUM, OVERLAP_MAXIMUM);
    const ON_3dPoint first_material_point(0.5, 1.0, 0.5);
    const ON_3dPoint second_material_point(2.5, 1.0, 2.5);
    const ON_3dPoint shared_material_point(1.5, 1.0, 1.5);
    const ON_3dPoint exterior_point(0.5, 1.0, 2.5);
    return check_union("overlapping shared-face union", first, second,
	first_material_point, second_material_point, shared_material_point,
	exterior_point);
}


static int
check_multiple_shared_face_union()
{
    ON_Brep *first = new ON_Brep();
    ON_Brep *left = make_box(0.0, OUTER_MINIMUM, OUTER_MINIMUM,
	2.0, OUTER_MAXIMUM, 2.0);
    ON_Brep *right = make_box(4.0, OUTER_MINIMUM, OUTER_MINIMUM,
	6.0, OUTER_MAXIMUM, 2.0);
    ON_Brep *bridge = make_box(1.0, OUTER_MINIMUM, 1.0,
	5.0, OUTER_MAXIMUM, 3.0);
    if (first == NULL || left == NULL || right == NULL || bridge == NULL) {
	delete first;
	delete left;
	delete right;
	delete bridge;
	return 1;
    }
    first->Append(*left);
    first->Append(*right);
    delete left;
    delete right;

    const ON_3dPoint left_material_point(0.5, 1.0, 0.5);
    const ON_3dPoint right_material_point(5.5, 1.0, 0.5);
    const ON_3dPoint bridge_material_point(3.0, 1.0, 2.5);
    const ON_3dPoint shared_material_point(1.5, 1.0, 1.5);
    const ON_3dPoint exterior_point(3.0, 1.0, 0.5);

    if (!first->IsValid() || !first->IsSolid() || !bridge->IsValid() ||
	!bridge->IsSolid())
    {
	bu_log("multiple shared-face union inputs are invalid\n");
	delete first;
	delete bridge;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, first, bridge, BOOLEAN_UNION);
    delete first;
    delete bridge;

    if (status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid() || !result.IsValidTolerancesAndFlags())
    {
	bu_log("multiple shared-face union result is invalid\n");
	return 1;
    }
    if (!ON_BrepPointInside(left_material_point, &result) ||
	!ON_BrepPointInside(right_material_point, &result) ||
	!ON_BrepPointInside(bridge_material_point, &result) ||
	!ON_BrepPointInside(shared_material_point, &result) ||
	ON_BrepPointInside(exterior_point, &result))
    {
	bu_log("multiple shared-face union result has incorrect containment\n");
	return 1;
    }

    return 0;
}


static int
check_cavity_fill_union()
{
    /* Model the common CSG construction (outer - inner) u inner.  The two
     * cavity boundaries have opposite face orientations even when their
     * underlying plane surfaces happen to use the same parameterization. */
    ON_Brep *outer = make_box(-1000.0, -1000.0, 2734.95,
	0.0, 1000.0, 3734.95);
    ON_Brep *cutter = make_box(-995.0, -995.0, 2739.95,
	-5.0, 995.0, 3729.95);
    ON_Brep *fill = make_box(-995.0, -995.0, 2739.95,
	-5.0, 995.0, 3729.95);
    if (outer == NULL || cutter == NULL || fill == NULL ||
	!outer->IsValid() || !outer->IsSolid() ||
	!cutter->IsValid() || !cutter->IsSolid() ||
	!fill->IsValid() || !fill->IsSolid())
    {
	bu_log("cavity-fill union inputs are invalid\n");
	delete outer;
	delete cutter;
	delete fill;
	return 1;
    }

    ON_Brep shell;
    const int difference_status = ON_Boolean(&shell, outer, cutter,
	BOOLEAN_DIFF);
    delete outer;
    delete cutter;
    if (difference_status != BREP_BOOLEAN_SUCCESS || !shell.IsValid() ||
	!shell.IsSolid())
    {
	bu_log("cavity-fill subtraction result is invalid\n");
	delete fill;
	return 1;
    }

    ON_Brep result;
    const int union_status = ON_Boolean(&result, &shell, fill,
	BOOLEAN_UNION);
    delete fill;
    if (union_status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid() || !result.IsValidTolerancesAndFlags())
    {
	bu_log("cavity-fill union result is invalid\n");
	return 1;
    }

    const ON_3dPoint shell_point(-997.0, 0.0, 3234.95);
    const ON_3dPoint filled_point(-500.0, 0.0, 3234.95);
    const ON_3dPoint exterior_point(1.0, 0.0, 3234.95);
    if (!ON_BrepPointInside(shell_point, &result) ||
	!ON_BrepPointInside(filled_point, &result) ||
	ON_BrepPointInside(exterior_point, &result))
    {
	bu_log("cavity-fill union result has incorrect containment\n");
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = check_nested_shared_face_union();
    failures += check_overlapping_shared_face_union();
    failures += check_multiple_shared_face_union();
    failures += check_cavity_fill_union();
    return failures;
}
