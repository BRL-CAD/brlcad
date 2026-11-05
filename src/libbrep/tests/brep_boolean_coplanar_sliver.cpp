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


static const ON_3dPoint PLATE_A_MATERIAL_POINT(40.0, 7.625, 13.0);
static const ON_3dPoint FRONT_BOTTOM_PLATE_MATERIAL_POINT(47.4, 7.625, 3.9);
static const ON_3dPoint SHARED_MATERIAL_POINT(49.540145807, 7.625,
    6.352837827);
static const ON_3dPoint EXTERIOR_POINT(49.7, 7.625, 7.0);


static ON_Brep *
make_plate_a()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(38.9582, 0.0, 13.5),
	ON_3dPoint(49.67187529607328, 0.0, 6.250004683428479),
	ON_3dPoint(49.67187529607328, 15.25, 6.250004683428479),
	ON_3dPoint(38.9582, 15.25, 13.5),
	ON_3dPoint(39.09831060660339, 0.0, 13.707048346811145),
	ON_3dPoint(49.81198590267668, 0.0, 6.457053030239624),
	ON_3dPoint(49.81198590267668, 15.25, 6.457053030239624),
	ON_3dPoint(39.09831060660339, 15.25, 13.707048346811145)
    };
    return ON_BrepBox(corners);
}


static ON_Brep *
make_front_bottom_plate()
{
    const ON_3dPoint corners[8] = {
	ON_3dPoint(46.0815, 0.0, 2.25),
	ON_3dPoint(49.67185403127074, 0.0, 6.249997866266668),
	ON_3dPoint(49.67185403127074, 15.25, 6.249997866266668),
	ON_3dPoint(46.0815, 15.25, 2.25),
	ON_3dPoint(45.8954535876155, 0.0, 2.416993210756779),
	ON_3dPoint(49.48580761888624, 0.0, 6.416991077023454),
	ON_3dPoint(49.48580761888624, 15.25, 6.416991077023454),
	ON_3dPoint(45.8954535876155, 15.25, 2.416993210756779)
    };
    return ON_BrepBox(corners);
}


static int
check_coplanar_sliver_boolean(op_type operation, const char *case_name)
{
    ON_Brep *plate_a = make_plate_a();
    ON_Brep *front_bottom_plate = make_front_bottom_plate();
    const bool plate_valid = plate_a != NULL && plate_a->IsValid();
    const bool plate_solid = plate_valid && plate_a->IsSolid();
    const bool front_valid = front_bottom_plate != NULL &&
	front_bottom_plate->IsValid();
    const bool front_solid = front_valid && front_bottom_plate->IsSolid();
    const bool plate_sample_in_plate = plate_solid &&
	ON_BrepPointInside(PLATE_A_MATERIAL_POINT, plate_a);
    const bool plate_sample_in_front = front_solid &&
	ON_BrepPointInside(PLATE_A_MATERIAL_POINT, front_bottom_plate);
    const bool front_sample_in_plate = plate_solid &&
	ON_BrepPointInside(FRONT_BOTTOM_PLATE_MATERIAL_POINT, plate_a);
    const bool front_sample_in_front = front_solid &&
	ON_BrepPointInside(FRONT_BOTTOM_PLATE_MATERIAL_POINT,
	    front_bottom_plate);
    const bool shared_sample_in_plate = plate_solid &&
	ON_BrepPointInside(SHARED_MATERIAL_POINT, plate_a);
    const bool shared_sample_in_front = front_solid &&
	ON_BrepPointInside(SHARED_MATERIAL_POINT, front_bottom_plate);
    const bool exterior_sample_in_plate = plate_solid &&
	ON_BrepPointInside(EXTERIOR_POINT, plate_a);
    const bool exterior_sample_in_front = front_solid &&
	ON_BrepPointInside(EXTERIOR_POINT, front_bottom_plate);
    if (!plate_valid || !plate_solid || !front_valid || !front_solid ||
	!plate_sample_in_plate || plate_sample_in_front ||
	front_sample_in_plate || !front_sample_in_front ||
	!shared_sample_in_plate || !shared_sample_in_front ||
	exterior_sample_in_plate || exterior_sample_in_front)
    {
	bu_log("%s inputs or samples are invalid: plate valid/solid %d/%d, "
	    "front valid/solid %d/%d, plate sample %d/%d, front sample "
	    "%d/%d, shared sample %d/%d, exterior sample %d/%d\n",
	    case_name, plate_valid, plate_solid, front_valid, front_solid,
	    plate_sample_in_plate, plate_sample_in_front,
	    front_sample_in_plate, front_sample_in_front,
	    shared_sample_in_plate, shared_sample_in_front,
	    exterior_sample_in_plate, exterior_sample_in_front);
	delete plate_a;
	delete front_bottom_plate;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, plate_a, front_bottom_plate,
	operation);
    delete plate_a;
    delete front_bottom_plate;

    const bool result_valid = result.IsValid();
    const bool result_solid = result.IsSolid();
    const bool result_tolerances_valid = result.IsValidTolerancesAndFlags();
    if (status != BREP_BOOLEAN_SUCCESS || !result_valid || !result_solid ||
	!result_tolerances_valid)
    {
	bu_log("%s result is invalid: status %d, valid %d, solid %d, "
	    "tolerances %d\n", case_name, status, result_valid, result_solid,
	    result_tolerances_valid);
	return 1;
    }

    if (operation == BOOLEAN_UNION) {
	const bool plate_inside = ON_BrepPointInside(
	    PLATE_A_MATERIAL_POINT, &result);
	const bool front_inside = ON_BrepPointInside(
	    FRONT_BOTTOM_PLATE_MATERIAL_POINT, &result);
	const bool shared_inside = ON_BrepPointInside(
	    SHARED_MATERIAL_POINT, &result);
	const bool exterior_inside = ON_BrepPointInside(EXTERIOR_POINT, &result);
	if (!plate_inside || !front_inside || !shared_inside || exterior_inside)
	{
	    bu_log("%s result has incorrect containment: plate/front/shared/"
		"exterior %d/%d/%d/%d\n", case_name, plate_inside,
		front_inside, shared_inside, exterior_inside);
	    return 1;
	}
	return 0;
    }

    const bool plate_inside = ON_BrepPointInside(
	PLATE_A_MATERIAL_POINT, &result);
    const bool front_inside = ON_BrepPointInside(
	FRONT_BOTTOM_PLATE_MATERIAL_POINT, &result);
    const bool shared_inside = ON_BrepPointInside(
	SHARED_MATERIAL_POINT, &result);
    const bool exterior_inside = ON_BrepPointInside(EXTERIOR_POINT, &result);
    if (plate_inside || front_inside || !shared_inside || exterior_inside)
    {
	bu_log("%s result has incorrect containment: plate/front/shared/"
	    "exterior %d/%d/%d/%d\n", case_name, plate_inside,
	    front_inside, shared_inside, exterior_inside);
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = check_coplanar_sliver_boolean(BOOLEAN_UNION,
	"coplanar sliver union");
    failures += check_coplanar_sliver_boolean(BOOLEAN_INTERSECT,
	"coplanar sliver intersection");
    return failures;
}
