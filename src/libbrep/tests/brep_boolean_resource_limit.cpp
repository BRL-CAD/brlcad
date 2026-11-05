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
#include "brep/intersect.h"


static const double CYLINDER_RADIUS = 6.0;
static const double CYLINDER_HEIGHT = 12.0;
static const double CYLINDER_OVERLAP_OFFSET = 3.0;


static int
check_periodic_overlap_failure()
{
    const ON_Cylinder first_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0, -CYLINDER_HEIGHT * 0.5),
		  CYLINDER_RADIUS),
	CYLINDER_HEIGHT);
    const ON_Cylinder second_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0,
		CYLINDER_OVERLAP_OFFSET - CYLINDER_HEIGHT * 0.5),
		  CYLINDER_RADIUS),
	CYLINDER_HEIGHT);
    ON_Brep *first = ON_BrepCylinder(first_cylinder, true, true);
    ON_Brep *second = ON_BrepCylinder(second_cylinder, true, true);
    if (first == NULL || second == NULL || !first->IsValid() ||
	!second->IsValid() || !first->IsSolid() || !second->IsSolid()) {
	delete first;
	delete second;
	return 1;
    }

    /* Exercise Boolean-level propagation with a deliberately tiny scoped
     * budget.  The default SSI budget is an implementation safeguard, not a
     * promise that this geometry must remain expensive forever: analytic
     * overlap handling can legitimately make it complete. */
    const brlcad::IntersectionWorkBudgetHandle budget =
	brlcad::CreateIntersectionWorkBudget(1);
    if (!budget) {
	delete first;
	delete second;
	return 1;
    }
    brlcad::SetIntersectionWorkBudget(budget);

    ON_Brep result;
    struct brep_boolean_failure_info failure =
	BREP_BOOLEAN_FAILURE_INFO_INIT;
    const int status = ON_Boolean(&result, first, second, BOOLEAN_INTERSECT,
	&failure);
    const bool exhausted = brlcad::IntersectionWorkBudgetExceeded();
    brlcad::ClearIntersectionWorkBudget();
    delete first;
    delete second;

    if (status == BREP_BOOLEAN_BUDGET_EXCEEDED &&
	result.m_F.Count() == 0 && exhausted &&
	!brlcad::CurrentIntersectionWorkBudget() &&
	failure.stage == BREP_BOOLEAN_FAILURE_STAGE_SURFACE_INTERSECTION &&
	failure.reason == BREP_BOOLEAN_FAILURE_REASON_WORK_BUDGET &&
	failure.face_index >= 0 && failure.peer_face_index >= 0 &&
	failure.intersection_candidates > 0)
    {
	return 0;
    }

    bu_log("scoped-budget Boolean did not fail safely: status=%d faces=%d "
	"exhausted=%d cleared=%d stage=%s reason=%s faces=%d/%d "
	"candidates=%zu\n", status, result.m_F.Count(), exhausted,
	brlcad::CurrentIntersectionWorkBudget() ? 0 : 1,
	ON_BooleanFailureStageDescription(failure.stage),
	ON_BooleanFailureReasonDescription(failure.reason), failure.face_index,
	failure.peer_face_index, failure.intersection_candidates);
    return 1;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_periodic_overlap_failure();
}
