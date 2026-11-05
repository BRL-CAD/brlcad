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


static const double EQUIVALENCE_TOLERANCE = INTERSECTION_TOL;
static const double CERTIFIED_OFFSET = EQUIVALENCE_TOLERANCE * 0.125;
static const double UNCERTIFIED_OFFSET = EQUIVALENCE_TOLERANCE * 2.0;
static const double CURVE_LENGTH = 10.0;
static const double CURVED_PATH_HEIGHT = 1.0;
static const double RETRACED_FIRST_X = 2.5;
static const double RETRACED_SECOND_X = 7.5;


static int
check_equivalence(const char *description,
		  const ON_Curve *first_curve,
		  const ON_Curve *second_curve,
		  bool reverse_second,
		  bool expected)
{
    const bool actual = ON_CurvesAreEquivalent(first_curve, second_curve,
	EQUIVALENCE_TOLERANCE, reverse_second);
    if (actual != expected) {
	bu_log("%s returned %d instead of %d\n", description, actual, expected);
	return 1;
    }

    return 0;
}


static int
check_curved_paths_are_not_equivalent()
{
    ON_NurbsCurve upper_curve(3, false, 3, 3);
    ON_NurbsCurve lower_curve(3, false, 3, 3);
    if (!upper_curve.SetCV(0, ON_3dPoint::Origin) ||
	!upper_curve.SetCV(1, ON_3dPoint(CURVE_LENGTH * 0.5,
	    CURVED_PATH_HEIGHT, 0.0)) ||
	!upper_curve.SetCV(2, ON_3dPoint(CURVE_LENGTH, 0.0, 0.0)) ||
	!upper_curve.MakeClampedUniformKnotVector() ||
	!lower_curve.SetCV(0, ON_3dPoint::Origin) ||
	!lower_curve.SetCV(1, ON_3dPoint(CURVE_LENGTH * 0.5,
	    -CURVED_PATH_HEIGHT, 0.0)) ||
	!lower_curve.SetCV(2, ON_3dPoint(CURVE_LENGTH, 0.0, 0.0)) ||
	!lower_curve.MakeClampedUniformKnotVector() || !lower_curve.Reverse())
    {
	return 1;
    }

    return check_equivalence("different curved paths", &upper_curve,
	&lower_curve, true, false);
}


static int
check_retraced_path_is_not_equivalent()
{
    ON_LineCurve line(ON_3dPoint::Origin,
	ON_3dPoint(CURVE_LENGTH, 0.0, 0.0));
    ON_3dPointArray retraced_points;
    retraced_points.Append(ON_3dPoint(CURVE_LENGTH, 0.0, 0.0));
    retraced_points.Append(ON_3dPoint(RETRACED_FIRST_X, 0.0, 0.0));
    retraced_points.Append(ON_3dPoint(RETRACED_SECOND_X, 0.0, 0.0));
    retraced_points.Append(ON_3dPoint::Origin);
    ON_PolylineCurve retraced_curve(retraced_points);

    return check_equivalence("retraced path", &line, &retraced_curve,
	true, false);
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);

    ON_LineCurve forward_line(ON_3dPoint::Origin,
	ON_3dPoint(CURVE_LENGTH, 0.0, 0.0));
    ON_LineCurve reverse_line(ON_3dPoint(CURVE_LENGTH, 0.0, 0.0),
	ON_3dPoint::Origin);
    ON_3dPointArray segmented_reverse_points;
    segmented_reverse_points.Append(ON_3dPoint(CURVE_LENGTH, 0.0, 0.0));
    segmented_reverse_points.Append(ON_3dPoint(RETRACED_SECOND_X, 0.0, 0.0));
    segmented_reverse_points.Append(ON_3dPoint(RETRACED_FIRST_X, 0.0, 0.0));
    segmented_reverse_points.Append(ON_3dPoint::Origin);
    ON_PolylineCurve segmented_reverse_curve(segmented_reverse_points);
    ON_LineCurve nearby_reverse_line(
	ON_3dPoint(CURVE_LENGTH, CERTIFIED_OFFSET, 0.0),
	ON_3dPoint(0.0, CERTIFIED_OFFSET, 0.0));
    ON_LineCurve separate_reverse_line(
	ON_3dPoint(CURVE_LENGTH, UNCERTIFIED_OFFSET, 0.0),
	ON_3dPoint(0.0, UNCERTIFIED_OFFSET, 0.0));

    return check_equivalence("exact reverse line", &forward_line,
	&reverse_line, true, true) +
	check_equivalence("opposite direction", &forward_line, &reverse_line,
	false, false) +
	check_equivalence("segmented reverse line", &forward_line,
	&segmented_reverse_curve, true, true) +
	check_equivalence("nearby reverse line", &forward_line,
	&nearby_reverse_line, true, true) +
	check_equivalence("separate reverse line", &forward_line,
	&separate_reverse_line, true, false) +
	check_curved_paths_are_not_equivalent() +
	check_retraced_path_is_not_equivalent();
}
