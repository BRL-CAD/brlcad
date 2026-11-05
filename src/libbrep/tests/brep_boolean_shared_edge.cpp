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


static const double FIRST_MINIMUM = 1.4;
static const double FIRST_MAXIMUM = 2.6;
static const double PARTIAL_SECOND_MINIMUM = 2.2;
static const double SECOND_MAXIMUM = 3.8;


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
make_upper_skewed_prism()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(0.0, 0.0, 4.0),
	ON_3dPoint(5.0, 0.0, 0.0),
	ON_3dPoint(5.0, 6.0, 0.0),
	ON_3dPoint(0.0, 6.0, 4.0),
	ON_3dPoint(0.1, 0.0, 4.2),
	ON_3dPoint(5.1, 0.0, 0.2),
	ON_3dPoint(5.1, 6.0, 0.2),
	ON_3dPoint(0.1, 6.0, 4.2)
    };
    return ON_BrepBox(corners);
}


static ON_Brep *
make_lower_skewed_prism()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(3.0, 0.0, -3.0),
	ON_3dPoint(5.0, 0.0, 0.0),
	ON_3dPoint(5.0, 6.0, 0.0),
	ON_3dPoint(3.0, 6.0, -3.0),
	ON_3dPoint(2.8, 0.0, -2.9),
	ON_3dPoint(4.8, 0.0, 0.1),
	ON_3dPoint(4.8, 6.0, 0.1),
	ON_3dPoint(2.8, 6.0, -2.9)
    };
    return ON_BrepBox(corners);
}


static int
check_shared_edge_contact(const char *case_name, double second_minimum_x)
{
    const ON_3dPoint retained_point(2.0, 2.0, 2.0);
    const ON_3dPoint outside_point(2.4, 2.7, 2.7);
    ON_Brep *first = make_box(FIRST_MINIMUM, FIRST_MINIMUM, FIRST_MINIMUM,
	FIRST_MAXIMUM, FIRST_MAXIMUM, FIRST_MAXIMUM);
    ON_Brep *second = make_box(second_minimum_x, FIRST_MAXIMUM,
	FIRST_MAXIMUM, SECOND_MAXIMUM, SECOND_MAXIMUM, SECOND_MAXIMUM);
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	bu_log("%s Boolean inputs are invalid\n", case_name);
	delete first;
	delete second;
	return 1;
    }

    ON_Brep difference;
    const int difference_status = ON_Boolean(&difference, first, second,
	BOOLEAN_DIFF);
    ON_Brep intersection;
    const int intersection_status = ON_Boolean(&intersection, first, second,
	BOOLEAN_INTERSECT);
    delete first;
    delete second;

    if (difference_status != BREP_BOOLEAN_SUCCESS || !difference.IsValid() ||
	!difference.IsSolid() || !difference.IsValidTolerancesAndFlags())
    {
	bu_log("%s subtraction result is invalid\n", case_name);
	return 1;
    }
    if (!ON_BrepPointInside(retained_point, &difference) ||
	ON_BrepPointInside(outside_point, &difference))
    {
	bu_log("%s subtraction result has incorrect containment\n", case_name);
	return 1;
    }
    if (intersection_status != BREP_BOOLEAN_EMPTY ||
	intersection.m_F.Count() != 0)
    {
	bu_log("%s intersection result is not empty\n", case_name);
	return 1;
    }

    return 0;
}


static int
check_skewed_shared_edge_contact()
{
    const ON_3dPoint first_inside(1.0, 3.0, 3.3);
    const ON_3dPoint second_inside(3.7, 3.0, -1.75);
    const ON_3dPoint outside(2.0, 3.0, 0.0);
    ON_Brep *first = make_upper_skewed_prism();
    ON_Brep *second = make_lower_skewed_prism();
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	bu_log("skewed shared-edge Boolean inputs are invalid\n");
	delete first;
	delete second;
	return 1;
    }

    ON_Brep union_result;
    const int union_status = ON_Boolean(&union_result, first, second,
	BOOLEAN_UNION);
    ON_Brep difference;
    const int difference_status = ON_Boolean(&difference, first, second,
	BOOLEAN_DIFF);
    ON_Brep intersection;
    const int intersection_status = ON_Boolean(&intersection, first, second,
	BOOLEAN_INTERSECT);
    delete first;
    delete second;

    if (union_status != BREP_BOOLEAN_SUCCESS || !union_result.IsValid() ||
	!union_result.IsSolid() || !union_result.IsValidTolerancesAndFlags())
    {
	bu_log("skewed shared-edge union result is invalid\n");
	return 1;
    }
    if (!ON_BrepPointInside(first_inside, &union_result) ||
	!ON_BrepPointInside(second_inside, &union_result) ||
	ON_BrepPointInside(outside, &union_result))
    {
	bu_log("skewed shared-edge union has incorrect containment\n");
	return 1;
    }
    if (difference_status != BREP_BOOLEAN_SUCCESS || !difference.IsValid() ||
	!difference.IsSolid() || !difference.IsValidTolerancesAndFlags())
    {
	bu_log("skewed shared-edge subtraction result is invalid\n");
	return 1;
    }
    if (!ON_BrepPointInside(first_inside, &difference) ||
	ON_BrepPointInside(second_inside, &difference) ||
	ON_BrepPointInside(outside, &difference))
    {
	bu_log("skewed shared-edge subtraction has incorrect containment\n");
	return 1;
    }
    if (intersection_status != BREP_BOOLEAN_EMPTY ||
	intersection.m_F.Count() != 0)
    {
	bu_log("skewed shared-edge intersection is not empty\n");
	return 1;
    }

    return 0;
}


static int
check_partitioned_coplanar_contact()
{
    const ON_3dPoint first_corners[8] = {
	ON_3dPoint(438.0, 530.0, 0.0),
	ON_3dPoint(450.0, 530.0, 1.907348632813e-6),
	ON_3dPoint(450.0, 836.0, 1.907348632813e-6),
	ON_3dPoint(438.0, 836.0, 0.0),
	ON_3dPoint(438.0, 530.0, 84.0),
	ON_3dPoint(450.0, 530.0, 84.0),
	ON_3dPoint(450.0, 836.0, 84.0),
	ON_3dPoint(438.0, 836.0, 84.0)
    };
    const ON_3dPoint second_corners[8] = {
	ON_3dPoint(450.0, 812.0, 0.0),
	ON_3dPoint(588.0, 812.0, 0.0),
	ON_3dPoint(588.0, 836.0, -1.109730237658e-5),
	ON_3dPoint(450.0, 836.0, -1.109730237658e-5),
	ON_3dPoint(450.0, 812.0, 84.0),
	ON_3dPoint(588.0, 812.0, 84.0),
	ON_3dPoint(588.0, 836.0, 84.0),
	ON_3dPoint(450.0, 836.0, 84.0)
    };
    ON_Brep *first = ON_BrepBox(first_corners);
    ON_Brep *second = ON_BrepBox(second_corners);
    if (first == NULL || second == NULL || !first->IsValid() ||
	!first->IsSolid() || !second->IsValid() || !second->IsSolid())
    {
	bu_log("partitioned coplanar contact inputs are invalid\n");
	delete first;
	delete second;
	return 1;
    }

    ON_Brep union_result;
    const int union_status = ON_Boolean(&union_result, first, second,
	BOOLEAN_UNION);
    delete first;
    delete second;
    if (union_status != BREP_BOOLEAN_SUCCESS || !union_result.IsValid() ||
	!union_result.IsSolid() || !union_result.IsValidTolerancesAndFlags())
    {
	bu_log("partitioned coplanar contact union result is invalid\n");
	return 1;
    }

    const ON_3dPoint first_inside(444.0, 700.0, 42.0);
    const ON_3dPoint second_inside(500.0, 820.0, 42.0);
    const ON_3dPoint outside(500.0, 800.0, 42.0);
    if (!ON_BrepPointInside(first_inside, &union_result) ||
	!ON_BrepPointInside(second_inside, &union_result) ||
	ON_BrepPointInside(outside, &union_result))
    {
	bu_log("partitioned coplanar contact union has incorrect containment\n");
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = check_shared_edge_contact("partially shared edge",
	PARTIAL_SECOND_MINIMUM);
    failures += check_shared_edge_contact("fully shared edge", FIRST_MINIMUM);
    failures += check_skewed_shared_edge_contact();
    failures += check_partitioned_coplanar_contact();
    return failures;
}
