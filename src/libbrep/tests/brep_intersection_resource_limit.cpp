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
#include "brep/intersect.h"

#include "../brep_defines.h"


static const int DENSE_CYCLE_COUNT = 512;
static const int DENSE_CYCLE_CORNER_COUNT = 4;
static const int PRESERVED_EVENT_COUNT = 1;
static const double DENSE_CYCLE_EXTENT = 1.0;
static const double DENSE_SURFACE_EXTENT = 2.0;
static const size_t OPERATION_CANDIDATE_PAIR_LIMIT =
    static_cast<size_t>(MAX_CCI_DEPTH) + 2;
static const int LINE_INTERSECTION_EVENT_COUNT = 1;


static void
append_dense_cycle(ON_3dPointArray &points, int starting_corner)
{
    const ON_3dPoint corners[DENSE_CYCLE_CORNER_COUNT] = {
	ON_3dPoint(-DENSE_CYCLE_EXTENT, -DENSE_CYCLE_EXTENT, 0.0),
	ON_3dPoint(DENSE_CYCLE_EXTENT, -DENSE_CYCLE_EXTENT, 0.0),
	ON_3dPoint(DENSE_CYCLE_EXTENT, DENSE_CYCLE_EXTENT, 0.0),
	ON_3dPoint(-DENSE_CYCLE_EXTENT, DENSE_CYCLE_EXTENT, 0.0)
    };
    points.Append(corners[starting_corner]);
    for (int cycle_index = 0; cycle_index < DENSE_CYCLE_COUNT;
	 ++cycle_index)
    {
	for (int corner_index = 1;
	     corner_index <= DENSE_CYCLE_CORNER_COUNT; ++corner_index)
	{
	    points.Append(corners[(starting_corner + corner_index) %
		DENSE_CYCLE_CORNER_COUNT]);
	}
    }
}


static int
check_curve_curve_budget()
{
    ON_3dPointArray points;
    ON_3dPointArray shifted_points;
    append_dense_cycle(points, 0);
    // The same repeatedly retraced boundary with a different start corner is
    // not globally parameter-equivalent, so this remains a subdivision budget
    // test even when exact whole-interval overlaps have a fast path.
    append_dense_cycle(shifted_points, 1);
    ON_PolylineCurve first_curve(points);
    ON_PolylineCurve second_curve(shifted_points);
    ON_SimpleArray<ON_X_EVENT> events;
    ON_X_EVENT preserved_event;
    events.Append(preserved_event);
    const int result = ON_Intersect(&first_curve, &second_curve, events,
	INTERSECTION_TOL);
    if (result != BREP_CCI_BUDGET_EXCEEDED) {
	bu_log("dense curve-curve intersection returned %d instead of a budget result\n",
	       result);
	return 1;
    }
    if (events.Count() != PRESERVED_EVENT_COUNT) {
	bu_log("dense curve-curve intersection changed input events after exhaustion\n");
	return 1;
    }

    return 0;
}


static int
check_curve_surface_budget()
{
    ON_3dPointArray points;
    append_dense_cycle(points, 0);
    ON_PolylineCurve curve(points);
    const ON_Plane plane(ON_3dPoint::Origin, ON_3dVector::XAxis,
	ON_3dVector::YAxis);
    ON_PlaneSurface surface(plane);
    surface.SetDomain(0, -DENSE_SURFACE_EXTENT, DENSE_SURFACE_EXTENT);
    surface.SetDomain(1, -DENSE_SURFACE_EXTENT, DENSE_SURFACE_EXTENT);
    surface.SetExtents(0, surface.Domain(0));
    surface.SetExtents(1, surface.Domain(1));

    ON_SimpleArray<ON_X_EVENT> events;
    ON_X_EVENT preserved_event;
    events.Append(preserved_event);
    const int result = ON_Intersect(&curve, &surface, events,
	INTERSECTION_TOL);
    if (result != BREP_CSI_BUDGET_EXCEEDED) {
	bu_log("dense curve-surface intersection returned %d instead of a budget result\n",
	       result);
	return 1;
    }
    if (events.Count() != PRESERVED_EVENT_COUNT) {
	bu_log("dense curve-surface intersection changed input events after exhaustion\n");
	return 1;
    }

    return 0;
}


static int
check_operation_budget()
{
    ON_LineCurve horizontal(ON_3dPoint(-1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0));
    ON_LineCurve vertical(ON_3dPoint(0.0, -1.0, 0.0),
	ON_3dPoint(0.0, 1.0, 0.0));
    const brlcad::IntersectionWorkBudgetHandle budget =
	brlcad::CreateIntersectionWorkBudget(OPERATION_CANDIDATE_PAIR_LIMIT);
    brlcad::SetIntersectionWorkBudget(budget);

    int failures = 0;
    ON_SimpleArray<ON_X_EVENT> first_events;
    const int first_result = ON_Intersect(&horizontal, &vertical,
	first_events, INTERSECTION_TOL);
    if (first_result != LINE_INTERSECTION_EVENT_COUNT ||
	first_events.Count() != LINE_INTERSECTION_EVENT_COUNT) {
	bu_log("first scoped curve intersection did not complete normally\n");
	failures++;
    }

    ON_SimpleArray<ON_X_EVENT> second_events;
    ON_X_EVENT preserved_event;
    second_events.Append(preserved_event);
    const int second_result = ON_Intersect(&horizontal, &vertical,
	second_events, INTERSECTION_TOL);
    if (second_result != BREP_CCI_BUDGET_EXCEEDED) {
	bu_log("aggregate curve intersection budget was not reported\n");
	failures++;
    }
    if (second_events.Count() != PRESERVED_EVENT_COUNT) {
	bu_log("aggregate budget exhaustion changed input events\n");
	failures++;
    }
    if (!brlcad::IntersectionWorkBudgetExceeded()) {
	bu_log("aggregate curve intersection budget was not marked exhausted\n");
	failures++;
    }

    brlcad::ClearIntersectionWorkBudget();
    if (brlcad::CurrentIntersectionWorkBudget()) {
	bu_log("aggregate curve intersection budget was not cleared\n");
	failures++;
    }

    const brlcad::IntersectionWorkBudgetHandle unlimited_budget =
	brlcad::CreateIntersectionWorkBudget(0);
    if (unlimited_budget) {
	bu_log("zero aggregate budget did not produce an unlimited handle\n");
	failures++;
    }
    brlcad::SetIntersectionWorkBudget(unlimited_budget);
    ON_SimpleArray<ON_X_EVENT> unlimited_events;
    const int unlimited_result = ON_Intersect(&horizontal, &vertical,
	unlimited_events, INTERSECTION_TOL);
    if (unlimited_result != LINE_INTERSECTION_EVENT_COUNT ||
	unlimited_events.Count() != LINE_INTERSECTION_EVENT_COUNT ||
	brlcad::IntersectionWorkBudgetExceeded()) {
	bu_log("unlimited aggregate budget changed curve intersection behavior\n");
	failures++;
    }
    brlcad::ClearIntersectionWorkBudget();

    return failures;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_curve_curve_budget() + check_curve_surface_budget() +
	check_operation_budget();
}
