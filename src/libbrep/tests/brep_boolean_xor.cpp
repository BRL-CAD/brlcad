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


static const double BOX_MINIMUM = 0.0;
static const double OVERLAP_MINIMUM = 1.0;
static const double FIRST_MAXIMUM = 2.0;
static const double SECOND_MAXIMUM = 3.0;
static const double FIRST_ONLY_SAMPLE = 0.5;
static const double CENTER_SAMPLE = 1.0;
static const double INTERSECTION_SAMPLE = 1.25;
static const double SHARED_SAMPLE = 1.5;
static const double SECOND_ONLY_SAMPLE = 2.5;
static const double EXTERIOR_SAMPLE = 3.5;
static const double LOWER_EXTERIOR_SAMPLE = -0.5;


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


static ON_Brep *
make_corner_first_box()
{
    return make_box(BOX_MINIMUM, BOX_MINIMUM, BOX_MINIMUM,
	FIRST_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM);
}


static ON_Brep *
make_corner_second_box()
{
    return make_box(OVERLAP_MINIMUM, OVERLAP_MINIMUM, OVERLAP_MINIMUM,
	SECOND_MAXIMUM, SECOND_MAXIMUM, SECOND_MAXIMUM);
}


static int
check_boolean(
    const char *case_name,
    ON_Brep *first,
    ON_Brep *second,
    op_type operation,
    const char *operation_name,
    const ON_3dPoint &included_point1,
    const ON_3dPoint &included_point2,
    const ON_3dPoint &excluded_point1,
    const ON_3dPoint &excluded_point2)
{
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	bu_log("%s %s inputs are invalid\n", case_name, operation_name);
	delete first;
	delete second;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, first, second, operation);
    delete first;
    delete second;

    const bool result_valid = result.IsValid();
    const bool result_solid = result.IsSolid();
    const bool result_tolerances_valid = result.IsValidTolerancesAndFlags();
    if (status != BREP_BOOLEAN_SUCCESS || !result_valid || !result_solid ||
	!result_tolerances_valid)
    {
	bu_log("%s %s result is invalid: status %d, valid %d, solid %d, tolerances %d\n",
	    case_name, operation_name, status, result_valid, result_solid,
	    result_tolerances_valid);
	return 1;
    }
    if (!ON_BrepPointInside(included_point1, &result) ||
	!ON_BrepPointInside(included_point2, &result) ||
	ON_BrepPointInside(excluded_point1, &result) ||
	ON_BrepPointInside(excluded_point2, &result))
    {
	bu_log("%s %s result has incorrect containment\n", case_name,
	    operation_name);
	return 1;
    }

    return 0;
}


static int
check_overlapping_xor()
{
    ON_Brep *first = make_box(BOX_MINIMUM, BOX_MINIMUM, BOX_MINIMUM,
	FIRST_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM);
    ON_Brep *second = make_box(OVERLAP_MINIMUM, BOX_MINIMUM,
	BOX_MINIMUM, SECOND_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM);
    return check_boolean("overlapping", first, second, BOOLEAN_XOR,
	"exclusive-or", ON_3dPoint(FIRST_ONLY_SAMPLE, CENTER_SAMPLE,
	CENTER_SAMPLE), ON_3dPoint(SECOND_ONLY_SAMPLE, CENTER_SAMPLE,
	CENTER_SAMPLE), ON_3dPoint(SHARED_SAMPLE, CENTER_SAMPLE,
	CENTER_SAMPLE), ON_3dPoint(EXTERIOR_SAMPLE, CENTER_SAMPLE,
	CENTER_SAMPLE));
}


static int
check_contained_xor()
{
    ON_Brep *first = make_box(BOX_MINIMUM, BOX_MINIMUM, BOX_MINIMUM,
	SECOND_MAXIMUM, SECOND_MAXIMUM, SECOND_MAXIMUM);
    ON_Brep *second = make_box(OVERLAP_MINIMUM, OVERLAP_MINIMUM,
	OVERLAP_MINIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM);
    return check_boolean("contained", first, second, BOOLEAN_XOR,
	"exclusive-or", ON_3dPoint(FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE,
	FIRST_ONLY_SAMPLE), ON_3dPoint(SECOND_ONLY_SAMPLE, SECOND_ONLY_SAMPLE,
	SECOND_ONLY_SAMPLE), ON_3dPoint(SHARED_SAMPLE, SHARED_SAMPLE,
	SHARED_SAMPLE), ON_3dPoint(EXTERIOR_SAMPLE, EXTERIOR_SAMPLE,
	EXTERIOR_SAMPLE));
}


static int
check_overlapping_differences()
{
    int failures = 0;
    failures += check_boolean("overlapping first difference",
	make_box(BOX_MINIMUM, BOX_MINIMUM, BOX_MINIMUM, FIRST_MAXIMUM,
	    FIRST_MAXIMUM, FIRST_MAXIMUM), make_box(OVERLAP_MINIMUM,
	    BOX_MINIMUM, BOX_MINIMUM, SECOND_MAXIMUM, FIRST_MAXIMUM,
	    FIRST_MAXIMUM), BOOLEAN_DIFF, "difference",
	ON_3dPoint(FIRST_ONLY_SAMPLE, CENTER_SAMPLE, CENTER_SAMPLE),
	ON_3dPoint(FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE),
	ON_3dPoint(SHARED_SAMPLE, CENTER_SAMPLE, CENTER_SAMPLE),
	ON_3dPoint(SECOND_ONLY_SAMPLE, CENTER_SAMPLE, CENTER_SAMPLE));
    failures += check_boolean("overlapping second difference",
	make_box(OVERLAP_MINIMUM, BOX_MINIMUM, BOX_MINIMUM, SECOND_MAXIMUM,
	    FIRST_MAXIMUM, FIRST_MAXIMUM), make_box(BOX_MINIMUM, BOX_MINIMUM,
	    BOX_MINIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM),
	BOOLEAN_DIFF, "difference", ON_3dPoint(SECOND_ONLY_SAMPLE,
	CENTER_SAMPLE, CENTER_SAMPLE), ON_3dPoint(SECOND_ONLY_SAMPLE,
	FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE), ON_3dPoint(SHARED_SAMPLE,
	CENTER_SAMPLE, CENTER_SAMPLE), ON_3dPoint(FIRST_ONLY_SAMPLE,
	CENTER_SAMPLE, CENTER_SAMPLE));
    return failures;
}


static int
check_corner_overlap_difference()
{
    return check_boolean("corner overlap", make_corner_first_box(),
	make_corner_second_box(), BOOLEAN_DIFF, "difference", ON_3dPoint(FIRST_ONLY_SAMPLE,
	FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE), ON_3dPoint(FIRST_ONLY_SAMPLE,
	SHARED_SAMPLE, SHARED_SAMPLE), ON_3dPoint(SHARED_SAMPLE,
	SHARED_SAMPLE, SHARED_SAMPLE), ON_3dPoint(SECOND_ONLY_SAMPLE,
	SECOND_ONLY_SAMPLE, SECOND_ONLY_SAMPLE));
}


static int
check_corner_overlap_intersection()
{
    return check_boolean("corner overlap", make_corner_first_box(),
	make_corner_second_box(), BOOLEAN_INTERSECT, "intersection",
	ON_3dPoint(SHARED_SAMPLE, SHARED_SAMPLE, SHARED_SAMPLE),
	ON_3dPoint(INTERSECTION_SAMPLE, INTERSECTION_SAMPLE,
	INTERSECTION_SAMPLE), ON_3dPoint(FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE,
	FIRST_ONLY_SAMPLE), ON_3dPoint(SECOND_ONLY_SAMPLE, SECOND_ONLY_SAMPLE,
	SECOND_ONLY_SAMPLE));
}


static int
check_corner_overlap_union()
{
    return check_boolean("corner overlap", make_corner_first_box(),
	make_corner_second_box(), BOOLEAN_UNION, "union",
	ON_3dPoint(FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE, FIRST_ONLY_SAMPLE),
	ON_3dPoint(SECOND_ONLY_SAMPLE, SECOND_ONLY_SAMPLE, SECOND_ONLY_SAMPLE),
	ON_3dPoint(EXTERIOR_SAMPLE, EXTERIOR_SAMPLE, EXTERIOR_SAMPLE),
	ON_3dPoint(LOWER_EXTERIOR_SAMPLE, LOWER_EXTERIOR_SAMPLE,
	LOWER_EXTERIOR_SAMPLE));
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = check_overlapping_xor();
    failures += check_contained_xor();
    failures += check_overlapping_differences();
    failures += check_corner_overlap_difference();
    failures += check_corner_overlap_intersection();
    failures += check_corner_overlap_union();
    return failures;
}
