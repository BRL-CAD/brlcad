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


static const ON_3dPoint THROUGH_CUT_MATERIAL_POINT(7.0, 0.0, 0.0);
static const ON_3dPoint THROUGH_CUT_REMOVED_POINT(0.0, 0.0, 0.0);
static const ON_3dPoint THROUGH_CUT_EXTERIOR_POINT(11.0, 0.0, 0.0);


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
single_curve_inner_loop_count(const ON_Brep &brep)
{
    int count = 0;
    for (int loop_index = 0; loop_index < brep.m_L.Count(); ++loop_index) {
	const ON_BrepLoop &loop = brep.m_L[loop_index];
	if (loop.m_type != ON_BrepLoop::inner || loop.m_ti.Count() != 1) {
	    continue;
	}

	const int trim_index = loop.m_ti[0];
	if (trim_index < 0 || trim_index >= brep.m_T.Count()) {
	    continue;
	}
	const ON_BrepEdge *edge = brep.m_T[trim_index].Edge();
	if (edge != NULL && edge->BoundingBox().Diagonal().Length() >
		ON_ZERO_TOLERANCE)
	{
	    ++count;
	}
    }
    return count;
}


static int
check_through_cut_difference(
    const char *case_name,
    ON_Brep *outer,
    ON_Brep *cutter,
    int minimum_single_curve_inner_loops = 0)
{
    if (outer == NULL || cutter == NULL || !outer->IsValid() ||
	!outer->IsSolid() || !cutter->IsValid() || !cutter->IsSolid())
    {
	bu_log("%s Boolean inputs are invalid\n", case_name);
	delete outer;
	delete cutter;
	return 1;
    }

    ON_Brep result;
    const int status = ON_Boolean(&result, outer, cutter, BOOLEAN_DIFF);
    delete outer;
    delete cutter;

    if (status != BREP_BOOLEAN_SUCCESS) {
	bu_log("%s Boolean evaluation failed: status=%d\n", case_name, status);
	return 1;
    }
    if (!result.IsValid() || !result.IsSolid() ||
	!result.IsValidTolerancesAndFlags())
    {
	bu_log("%s Boolean result is invalid\n", case_name);
	return 1;
    }
    const int single_curve_inner_loops =
	single_curve_inner_loop_count(result);
    if (single_curve_inner_loops < minimum_single_curve_inner_loops) {
	bu_log("%s retained %d one-curve inner loops instead of at least %d\n",
	    case_name, single_curve_inner_loops,
	    minimum_single_curve_inner_loops);
	return 1;
    }
    if (!ON_BrepPointInside(THROUGH_CUT_MATERIAL_POINT, &result) ||
	ON_BrepPointInside(THROUGH_CUT_REMOVED_POINT, &result) ||
	ON_BrepPointInside(THROUGH_CUT_EXTERIOR_POINT, &result))
    {
	bu_log("%s Boolean result has incorrect containment\n", case_name);
	return 1;
	}

    return 0;
}


static int
check_rectangular_through_cut()
{
    static const double OUTER_HALF_EXTENT = 10.0;
    static const double CUTTER_HALF_WIDTH = 5.0;
    static const double CUTTER_HALF_LENGTH = 20.0;
    ON_Brep *outer = make_box(-OUTER_HALF_EXTENT, -OUTER_HALF_EXTENT,
	-OUTER_HALF_EXTENT, OUTER_HALF_EXTENT, OUTER_HALF_EXTENT,
	OUTER_HALF_EXTENT);
    ON_Brep *cutter = make_box(-CUTTER_HALF_WIDTH, -CUTTER_HALF_LENGTH,
	-CUTTER_HALF_WIDTH, CUTTER_HALF_WIDTH, CUTTER_HALF_LENGTH,
	CUTTER_HALF_WIDTH);
    return check_through_cut_difference("rectangular through-cut", outer,
	cutter);
}


static int
check_coplanar_rectangular_through_cut()
{
    static const double OUTER_HALF_EXTENT = 10.0;
    static const double CUTTER_HALF_WIDTH = 5.0;
    static const double CUTTER_HALF_LENGTH = 20.0;
    ON_Brep *outer = make_box(-OUTER_HALF_EXTENT, -OUTER_HALF_EXTENT,
	-OUTER_HALF_EXTENT, OUTER_HALF_EXTENT, OUTER_HALF_EXTENT,
	OUTER_HALF_EXTENT);
    ON_Brep *cutter = make_box(-CUTTER_HALF_WIDTH, -CUTTER_HALF_LENGTH,
	-OUTER_HALF_EXTENT, CUTTER_HALF_WIDTH, CUTTER_HALF_LENGTH,
	OUTER_HALF_EXTENT);
    return check_through_cut_difference("coplanar rectangular through-cut",
	outer, cutter);
}


static int
check_cylindrical_through_cut()
{
    static const double OUTER_HALF_EXTENT = 10.0;
    static const double CUTTER_RADIUS = 5.0;
    static const double CUTTER_HALF_LENGTH = 20.0;
    const ON_Cylinder cylinder(ON_Circle(
	ON_3dPoint(0.0, 0.0, -CUTTER_HALF_LENGTH), CUTTER_RADIUS),
	2.0 * CUTTER_HALF_LENGTH);
    ON_Brep *outer = make_box(-OUTER_HALF_EXTENT, -OUTER_HALF_EXTENT,
	-OUTER_HALF_EXTENT, OUTER_HALF_EXTENT, OUTER_HALF_EXTENT,
	OUTER_HALF_EXTENT);
    ON_Brep *cutter = ON_BrepCylinder(cylinder, true, true);
    return check_through_cut_difference("cylindrical through-cut", outer,
	cutter, 2);
}

static int
check_rotated_cylindrical_through_cut()
{
    static const double OUTER_HALF_EXTENT = 10.0;
    static const double CUTTER_RADIUS = 5.0;
    static const double CUTTER_HALF_LENGTH = 20.0;
    const ON_3dPoint cutter_base(0.0, -CUTTER_HALF_LENGTH, 0.0);
    const ON_Plane cutter_plane(cutter_base, ON_3dVector::YAxis);
    const ON_Cylinder cylinder(ON_Circle(cutter_plane, CUTTER_RADIUS),
	2.0 * CUTTER_HALF_LENGTH);
    ON_Brep *outer = make_box(-OUTER_HALF_EXTENT, -OUTER_HALF_EXTENT,
	-OUTER_HALF_EXTENT, OUTER_HALF_EXTENT, OUTER_HALF_EXTENT,
	OUTER_HALF_EXTENT);
    ON_Brep *cutter = ON_BrepCylinder(cylinder, true, true);
    return check_through_cut_difference("rotated cylindrical through-cut",
	outer, cutter);
}

static int
check_transformed_cylindrical_through_cut()
{
    static const double OUTER_HALF_EXTENT = 10.0;
    static const double CUTTER_RADIUS = 5.0;
    static const double CUTTER_HALF_LENGTH = 20.0;
    const ON_Cylinder cylinder(ON_Circle(
	ON_3dPoint(0.0, 0.0, -CUTTER_HALF_LENGTH), CUTTER_RADIUS),
	2.0 * CUTTER_HALF_LENGTH);
    ON_Brep *outer = make_box(-OUTER_HALF_EXTENT, -OUTER_HALF_EXTENT,
	-OUTER_HALF_EXTENT, OUTER_HALF_EXTENT, OUTER_HALF_EXTENT,
	OUTER_HALF_EXTENT);
    ON_Brep *cutter = ON_BrepCylinder(cylinder, true, true);
    ON_Xform rotation;
    rotation.Rotation(ON_3dVector::ZAxis, ON_3dVector::YAxis,
	ON_3dPoint::Origin);
    if (!outer->Transform(rotation) || !cutter->Transform(rotation)) {
	delete outer;
	delete cutter;
	return 1;
    }
    return check_through_cut_difference("transformed cylindrical through-cut",
	outer, cutter);
}


static int
check_disconnected_cylindrical_union()
{
    static const double BOX_MINIMUM_X = -10.0;
    static const double LEFT_BOX_MAXIMUM_X = -6.0;
    static const double RIGHT_BOX_MINIMUM_X = 6.0;
    static const double BOX_MAXIMUM_X = 10.0;
    static const double BOX_HALF_EXTENT = 5.0;
    static const double CYLINDER_RADIUS = 1.0;
    static const double CYLINDER_HALF_LENGTH = 2.0;
    static const ON_3dPoint LEFT_SAMPLE(-8.0, 0.0, 0.0);
    static const ON_3dPoint CYLINDER_SAMPLE(0.0, 0.0, 0.0);
    static const ON_3dPoint RIGHT_SAMPLE(8.0, 0.0, 0.0);
    static const ON_3dPoint EXTERIOR_SAMPLE(0.0, 3.0, 0.0);
    const ON_Cylinder cylinder(ON_Circle(
	ON_3dPoint(0.0, 0.0, -CYLINDER_HALF_LENGTH), CYLINDER_RADIUS),
	2.0 * CYLINDER_HALF_LENGTH);

    ON_Brep *left = make_box(BOX_MINIMUM_X, -BOX_HALF_EXTENT,
	-BOX_HALF_EXTENT, LEFT_BOX_MAXIMUM_X, BOX_HALF_EXTENT,
	BOX_HALF_EXTENT);
    ON_Brep *right = make_box(RIGHT_BOX_MINIMUM_X, -BOX_HALF_EXTENT,
	-BOX_HALF_EXTENT, BOX_MAXIMUM_X, BOX_HALF_EXTENT,
	BOX_HALF_EXTENT);
    ON_Brep *center = ON_BrepCylinder(cylinder, true, true);
    if (left == NULL || right == NULL || center == NULL || !left->IsValid() ||
	!left->IsSolid() || !right->IsValid() || !right->IsSolid() ||
	!center->IsValid() || !center->IsSolid())
    {
	delete left;
	delete right;
	delete center;
	return 1;
    }

    ON_Brep outer;
    const int outer_status = ON_Boolean(&outer, left, right, BOOLEAN_UNION);
    delete left;
    delete right;
    if (outer_status != BREP_BOOLEAN_SUCCESS || !outer.IsValid() ||
	!outer.IsSolid() || !outer.IsValidTolerancesAndFlags())
    {
	delete center;
	return 1;
    }

    ON_Brep result;
    const int result_status = ON_Boolean(&result, &outer, center,
	BOOLEAN_UNION);
    delete center;
    if (result_status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid() || !result.IsValidTolerancesAndFlags())
    {
	bu_log("disconnected cylindrical union result is invalid\n");
	return 1;
    }
    if (!ON_BrepPointInside(LEFT_SAMPLE, &result) ||
	!ON_BrepPointInside(CYLINDER_SAMPLE, &result) ||
	!ON_BrepPointInside(RIGHT_SAMPLE, &result) ||
	ON_BrepPointInside(EXTERIOR_SAMPLE, &result))
    {
	bu_log("disconnected cylindrical union has incorrect containment\n");
	return 1;
    }

    return 0;
}


static int
check_overlapping_bounds_cylinder_sphere_union()
{
    static const double CYLINDER_RADIUS = 1.0;
    static const double CYLINDER_LENGTH = 4.0;
    static const double CYLINDER_MINIMUM_Z = -2.0;
    static const double SPHERE_RADIUS = 1.0;
    static const ON_3dPoint SPHERE_CENTER(1.5, 1.5, 0.0);
    static const ON_3dPoint CYLINDER_SAMPLE(0.0, 0.0, 0.0);
    static const ON_3dPoint SPHERE_SAMPLE(1.5, 1.5, 0.0);
    static const ON_3dPoint EXTERIOR_SAMPLE(0.0, 3.0, 0.0);
    const ON_Cylinder cylinder(ON_Circle(
	ON_3dPoint(0.0, 0.0, CYLINDER_MINIMUM_Z), CYLINDER_RADIUS),
	CYLINDER_LENGTH);

    ON_Brep *cylinder_brep = ON_BrepCylinder(cylinder, true, true);
    ON_Brep *sphere_brep = ON_BrepSphere(ON_Sphere(SPHERE_CENTER,
	SPHERE_RADIUS));
    if (cylinder_brep == NULL || sphere_brep == NULL ||
	!cylinder_brep->IsValid() || !cylinder_brep->IsSolid() ||
	!sphere_brep->IsValid() || !sphere_brep->IsSolid())
    {
	delete cylinder_brep;
	delete sphere_brep;
	return 1;
    }

    ON_Brep result;
    const int result_status = ON_Boolean(&result, cylinder_brep, sphere_brep,
	BOOLEAN_UNION);
    delete cylinder_brep;
    delete sphere_brep;
    if (result_status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid() || !result.IsValidTolerancesAndFlags())
    {
	return 1;
    }
    if (!ON_BrepPointInside(CYLINDER_SAMPLE, &result) ||
	!ON_BrepPointInside(SPHERE_SAMPLE, &result) ||
	ON_BrepPointInside(EXTERIOR_SAMPLE, &result))
    {
	return 1;
    }

    return 0;
}


static int
check_component_preservation_union()
{
    static const double CYLINDER_RADIUS = 1.0;
    static const double CYLINDER_LENGTH = 4.0;
    static const double CYLINDER_MINIMUM_Z = -2.0;
    static const double SPHERE_RADIUS = 1.0;
    static const ON_3dPoint CYLINDER_SAMPLE(-6.0, 0.0, 0.0);
    static const ON_3dPoint BOX_SAMPLE(1.0, 1.0, 0.0);
    static const ON_3dPoint SPHERE_SAMPLE(0.0, 0.0, 0.0);
    static const ON_3dPoint EXTERIOR_SAMPLE(3.0, 0.0, 0.0);
    const ON_Cylinder cylinder(ON_Circle(
	ON_3dPoint(-6.0, 0.0, CYLINDER_MINIMUM_Z), CYLINDER_RADIUS),
	CYLINDER_LENGTH);

    ON_Brep *cylinder_brep = ON_BrepCylinder(cylinder, true, true);
    ON_Brep *box_brep = make_box(0.9, 0.9, -0.1, 1.1, 1.1, 0.1);
    ON_Brep *sphere_brep = ON_BrepSphere(ON_Sphere(ON_3dPoint::Origin,
	SPHERE_RADIUS));
    if (cylinder_brep == NULL || box_brep == NULL || sphere_brep == NULL ||
	!cylinder_brep->IsValid() || !cylinder_brep->IsSolid() ||
	!box_brep->IsValid() || !box_brep->IsSolid() ||
	!sphere_brep->IsValid() || !sphere_brep->IsSolid())
    {
	delete cylinder_brep;
	delete box_brep;
	delete sphere_brep;
	return 1;
    }

    ON_Brep outer;
    const int outer_status = ON_Boolean(&outer, cylinder_brep, box_brep,
	BOOLEAN_UNION);
    delete cylinder_brep;
    delete box_brep;
    if (outer_status != BREP_BOOLEAN_SUCCESS || !outer.IsValid() ||
	!outer.IsSolid() || !outer.IsValidTolerancesAndFlags())
    {
	delete sphere_brep;
	return 1;
    }

    ON_Brep result;
    const int result_status = ON_Boolean(&result, &outer, sphere_brep,
	BOOLEAN_UNION);
    delete sphere_brep;
    if (result_status != BREP_BOOLEAN_SUCCESS || !result.IsValid() ||
	!result.IsSolid() || !result.IsValidTolerancesAndFlags())
    {
	return 1;
    }
    if (!ON_BrepPointInside(CYLINDER_SAMPLE, &result) ||
	!ON_BrepPointInside(BOX_SAMPLE, &result) ||
	!ON_BrepPointInside(SPHERE_SAMPLE, &result) ||
	ON_BrepPointInside(EXTERIOR_SAMPLE, &result))
    {
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    int failures = check_rectangular_through_cut();
    failures += check_coplanar_rectangular_through_cut();
    failures += check_cylindrical_through_cut();
    failures += check_rotated_cylindrical_through_cut();
    failures += check_transformed_cylindrical_through_cut();
    failures += check_disconnected_cylindrical_union();
    failures += check_overlapping_bounds_cylinder_sphere_union();
    failures += check_component_preservation_union();
    return failures;
}
