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

#include "../brep_defines.h"
#include "../boolean_loop.h"


static const double LOOP_LENGTH = 10.0;
static const double FIRST_SUBDIVISION = 2.5;
static const double SECOND_SUBDIVISION = 7.5;


static void
free_loop(ON_SimpleArray<ON_Curve *> &loop)
{
    for (int curve_index = 0; curve_index < loop.Count(); ++curve_index) {
	delete loop[curve_index];
    }
    loop.Empty();
}


static int
check_loop(const char *description, ON_SimpleArray<ON_Curve *> &loop,
	bool expected)
{
    const bool actual = loop_is_degenerate(loop);
    free_loop(loop);
    if (actual == expected) {
	return 0;
    }

    bu_log("%s returned %d instead of %d\n", description, actual, expected);
    return 1;
}


static bool
sampled_loop_is_degenerate(const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_PolyCurve loop_curve;
    for (int curve_index = 0; curve_index < loop.Count(); ++curve_index) {
	if (loop[curve_index] == NULL ||
	    !append_boolean_loop_curve_to_polycurve(
		loop[curve_index]->Duplicate(), loop_curve))
	{
	    return false;
	}
    }

    const ON_Interval domain = loop_curve.Domain();
    const ON_3dPoint first = loop_curve.PointAt(domain.ParameterAt(0.25));
    const ON_3dPoint second = loop_curve.PointAt(domain.ParameterAt(0.75));
    return first.DistanceTo(second) < INTERSECTION_TOL;
}


static bool
append_line(
    ON_SimpleArray<ON_Curve *> &loop,
    const ON_3dPoint &start,
    const ON_3dPoint &end,
    double domain_start,
    double domain_end)
{
    ON_LineCurve *curve = new ON_LineCurve(start, end);
    if (curve == NULL || !curve->SetDomain(domain_start, domain_end)) {
	delete curve;
	return false;
    }

    loop.Append(curve);
    return true;
}


static int
check_asymmetric_segmented_retrace()
{
    ON_SimpleArray<ON_Curve *> loop;
    if (!append_line(loop, ON_3dPoint::Origin,
	    ON_3dPoint(LOOP_LENGTH, 0.0, 0.0), 0.0, 100.0))
    {
	return 1;
    }

    ON_3dPointArray points;
    points.Append(ON_3dPoint(LOOP_LENGTH, 0.0, 0.0));
    points.Append(ON_3dPoint(SECOND_SUBDIVISION, 0.0, 0.0));
    points.Append(ON_3dPoint(FIRST_SUBDIVISION, 0.0, 0.0));
    points.Append(ON_3dPoint::Origin);
    ON_PolylineCurve *reverse = new ON_PolylineCurve(points);
    if (reverse == NULL || !reverse->SetDomain(100.0, 101.0)) {
	delete reverse;
	free_loop(loop);
	return 1;
    }
    loop.Append(reverse);

    if (sampled_loop_is_degenerate(loop)) {
	bu_log("asymmetric segmented retrace did not distinguish sampled failure\n");
	free_loop(loop);
	return 1;
	}
    return check_loop("asymmetric segmented retrace", loop, true);
}


static int
check_curved_retrace()
{
    ON_NurbsCurve *forward = new ON_NurbsCurve(3, false, 3, 3);
    if (forward == NULL ||
	!forward->SetCV(0, ON_3dPoint::Origin) ||
	!forward->SetCV(1, ON_3dPoint(LOOP_LENGTH * 0.5, 2.0, 0.0)) ||
	!forward->SetCV(2, ON_3dPoint(LOOP_LENGTH, 0.0, 0.0)) ||
	!forward->MakeClampedUniformKnotVector())
    {
	delete forward;
	return 1;
    }

    ON_Curve *reverse = forward->Duplicate();
    if (reverse == NULL || !reverse->Reverse()) {
	delete forward;
	delete reverse;
	return 1;
    }

    ON_SimpleArray<ON_Curve *> loop;
    loop.Append(forward);
    loop.Append(reverse);
    return check_loop("curved retrace", loop, true);
}


static int
check_rectangle_is_retained()
{
    ON_SimpleArray<ON_Curve *> loop;
    if (!append_line(loop, ON_3dPoint::Origin,
	    ON_3dPoint(LOOP_LENGTH, 0.0, 0.0), 0.0, 1.0) ||
	!append_line(loop, ON_3dPoint(LOOP_LENGTH, 0.0, 0.0),
	    ON_3dPoint(LOOP_LENGTH, LOOP_LENGTH, 0.0), 1.0, 2.0) ||
	!append_line(loop, ON_3dPoint(LOOP_LENGTH, LOOP_LENGTH, 0.0),
	    ON_3dPoint(0.0, LOOP_LENGTH, 0.0), 2.0, 3.0) ||
	!append_line(loop, ON_3dPoint(0.0, LOOP_LENGTH, 0.0),
	    ON_3dPoint::Origin, 3.0, 4.0))
    {
	free_loop(loop);
	return 1;
    }

    return check_loop("rectangle", loop, false);
}


static int
check_single_circle_is_retained()
{
    ON_Circle circle(ON_3dPoint::Origin, LOOP_LENGTH);
    ON_NurbsCurve *curve = new ON_NurbsCurve();
    if (curve == NULL || circle.GetNurbForm(*curve) == 0) {
	delete curve;
	return 1;
    }

    ON_SimpleArray<ON_Curve *> loop;
    loop.Append(curve);
    return check_loop("single circle", loop, false);
}


static int
check_single_collinear_retrace()
{
    ON_3dPointArray points;
    points.Append(ON_3dPoint::Origin);
    points.Append(ON_3dPoint(LOOP_LENGTH, 0.0, 0.0));
    points.Append(ON_3dPoint(SECOND_SUBDIVISION, 0.0, 0.0));
    points.Append(ON_3dPoint(FIRST_SUBDIVISION, 0.0, 0.0));
    points.Append(ON_3dPoint::Origin);

    ON_SimpleArray<ON_Curve *> loop;
    loop.Append(new ON_PolylineCurve(points));
    return check_loop("single collinear retrace", loop, true);
}


static int
check_thin_rectangle_is_retained()
{
    const double width = 1.0e-8;
    ON_SimpleArray<ON_Curve *> loop;
    if (!append_line(loop, ON_3dPoint::Origin,
	    ON_3dPoint(LOOP_LENGTH, 0.0, 0.0), 0.0, 1.0) ||
	!append_line(loop, ON_3dPoint(LOOP_LENGTH, 0.0, 0.0),
	    ON_3dPoint(LOOP_LENGTH, width, 0.0), 1.0, 2.0) ||
	!append_line(loop, ON_3dPoint(LOOP_LENGTH, width, 0.0),
	    ON_3dPoint(0.0, width, 0.0), 2.0, 3.0) ||
	!append_line(loop, ON_3dPoint(0.0, width, 0.0),
	    ON_3dPoint::Origin, 3.0, 4.0))
    {
	free_loop(loop);
	return 1;
    }

    return check_loop("thin rectangle", loop, false);
}


static int
check_repeated_crossing_is_retained()
{
    const ON_3dPoint origin = ON_3dPoint::Origin;
    ON_SimpleArray<ON_Curve *> loop;
    if (!append_line(loop, origin, ON_3dPoint(3.0, 0.0, 0.0),
	    0.0, 1.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(3.0, 0.0, 0.0),
	    ON_3dPoint(1.0, 2.0, 0.0), 1.0 / 3.0, 2.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(1.0, 2.0, 0.0), origin,
	    2.0 / 3.0, 1.0) ||
	!append_line(loop, origin, ON_3dPoint(-3.0, 0.0, 0.0),
	    1.0, 5.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(-3.0, 0.0, 0.0),
	    ON_3dPoint(-1.0, -2.0, 0.0), 5.0 / 3.0, 7.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(-1.0, -2.0, 0.0), origin,
	    7.0 / 3.0, 3.0) ||
	!append_line(loop, origin, ON_3dPoint(0.0, 3.0, 0.0),
	    3.0, 10.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(0.0, 3.0, 0.0),
	    ON_3dPoint(2.0, 1.0, 0.0), 10.0 / 3.0, 11.0 / 3.0) ||
	!append_line(loop, ON_3dPoint(2.0, 1.0, 0.0), origin,
	    11.0 / 3.0, 4.0))
    {
	free_loop(loop);
	return 1;
    }

    if (!sampled_loop_is_degenerate(loop)) {
	bu_log("repeated crossing did not distinguish sampled failure\n");
	free_loop(loop);
	return 1;
    }
    return check_loop("repeated crossing", loop, false);
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    ON_SimpleArray<ON_Curve *> empty_loop;
    return check_loop("empty loop", empty_loop, true) +
	check_asymmetric_segmented_retrace() +
	check_curved_retrace() +
	check_rectangle_is_retained() +
	check_single_circle_is_retained() +
	check_single_collinear_retrace() +
	check_thin_rectangle_is_retained() +
	check_repeated_crossing_is_retained();
}
