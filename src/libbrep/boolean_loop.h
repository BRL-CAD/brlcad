/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#ifndef LIBBREP_BOOLEAN_LOOP_H
#define LIBBREP_BOOLEAN_LOOP_H

#include <algorithm>
#include <cmath>

#include "brep.h"
#include "brep/intersect.h"


static bool
append_boolean_loop_curve_to_polycurve(ON_Curve *curve,
	ON_PolyCurve &polycurve)
{
    if (curve == NULL) {
	return false;
    }

    ON_PolyCurve *nested = ON_PolyCurve::Cast(curve);
    if (nested != NULL) {
	const ON_CurveArray &segments = nested->SegmentCurves();
	for (int segment_index = 0; segment_index < segments.Count();
	     ++segment_index)
	{
	    if (segments[segment_index] == NULL ||
		!append_boolean_loop_curve_to_polycurve(
		    segments[segment_index]->Duplicate(),
		    polycurve))
	    {
		delete nested;
		return false;
	    }
	}
	delete nested;
	return true;
    }

    polycurve.Append(curve);
    return true;
}


static bool
boolean_loop_is_continuous_and_closed(
    const ON_SimpleArray<ON_Curve *> &loop)
{
    if (loop.Count() < 1) {
	return false;
    }

    for (int curve_index = 0; curve_index < loop.Count(); ++curve_index) {
	const ON_Curve *curve = loop[curve_index];
	const ON_Curve *next_curve =
	    loop[(curve_index + 1) % loop.Count()];
	if (curve == NULL || next_curve == NULL ||
	    !curve->Domain().IsIncreasing())
	{
	    return false;
	}

	const ON_3dPoint curve_end = curve->PointAtEnd();
	const ON_3dPoint next_start = next_curve->PointAtStart();
	if (!curve_end.IsValid() || !next_start.IsValid() ||
	    curve_end.DistanceTo(next_start) > ON_ZERO_TOLERANCE)
	{
	    return false;
	}
    }

    return true;
}


static bool
append_boolean_loop_arc(
    const ON_SimpleArray<ON_Curve *> &loop,
    int start_index,
    int curve_count,
    ON_PolyCurve &arc)
{
    const int loop_curve_count = loop.Count();
    for (int curve_offset = 0; curve_offset < curve_count; ++curve_offset) {
	const int curve_index =
	    (start_index + curve_offset) % loop_curve_count;
	if (loop[curve_index] == NULL ||
	    !append_boolean_loop_curve_to_polycurve(
		loop[curve_index]->Duplicate(), arc))
	{
	    return false;
	}
    }

    return true;
}


/* A continuous closed loop whose complete NURBS control polygon is collinear
 * cannot enclose parameter-space area.  This catches one-curve polylines
 * which run out and retrace themselves: their bounding-box diagonal and
 * curve length are nonzero, so point-collapse tests alone miss them.  The
 * proof uses only coordinate-roundoff tolerance, preserving narrow loops
 * with any resolvable transverse extent. */
static bool
boolean_loop_is_collinear(const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_SimpleArray<ON_3dPoint> control_points;
    double coordinate_scale = 1.0;
    for (int curve_index = 0; curve_index < loop.Count(); ++curve_index) {
	if (loop[curve_index] == NULL) {
	    return false;
	}

	ON_NurbsCurve nurbs;
	if (loop[curve_index]->GetNurbForm(nurbs) <= 0) {
	    return false;
	}
	for (int control_index = 0; control_index < nurbs.CVCount();
	     ++control_index)
	{
	    ON_3dPoint point;
	    if (!nurbs.GetCV(control_index, point) || !point.IsValid()) {
		return false;
	    }
	    coordinate_scale = std::max(coordinate_scale, fabs(point.x));
	    coordinate_scale = std::max(coordinate_scale, fabs(point.y));
	    coordinate_scale = std::max(coordinate_scale, fabs(point.z));
	    control_points.Append(point);
	}
    }
    if (control_points.Count() < 1) {
	return true;
    }

    const double tolerance = 2.0 * std::max(ON_ZERO_TOLERANCE,
	ON_EPSILON * coordinate_scale);
    const ON_3dPoint origin = control_points[0];
    int direction_index = 0;
    double maximum_distance = 0.0;
    for (int point_index = 1; point_index < control_points.Count();
	 ++point_index)
    {
	const double distance = origin.DistanceTo(control_points[point_index]);
	if (distance > maximum_distance) {
	    maximum_distance = distance;
	    direction_index = point_index;
	}
    }
    if (maximum_distance <= tolerance) {
	return true;
    }

    ON_3dVector direction = control_points[direction_index] - origin;
    if (!direction.Unitize()) {
	return true;
    }
    for (int point_index = 1; point_index < control_points.Count();
	 ++point_index)
    {
	const ON_3dVector offset = control_points[point_index] - origin;
	if (ON_CrossProduct(offset, direction).Length() > tolerance) {
	    return false;
	}
    }
    return true;
}


static bool
loop_is_degenerate(const ON_SimpleArray<ON_Curve *> &loop)
{
    if (loop.Count() < 1) {
	return true;
    }
    if (!boolean_loop_is_continuous_and_closed(loop)) {
	return false;
    }
    if (boolean_loop_is_collinear(loop)) {
	return true;
    }
    if (loop.Count() == 1) {
	ON_BoundingBox bbox;
	return loop[0]->GetTightBoundingBox(bbox) &&
	    bbox.Diagonal().Length() <= ON_ZERO_TOLERANCE;
    }

    for (int first_start = 0; first_start < loop.Count(); ++first_start) {
	for (int first_curve_count = 1;
	     first_curve_count < loop.Count(); ++first_curve_count)
	{
	    ON_PolyCurve first_arc;
	    ON_PolyCurve second_arc;
	    const int second_start =
		(first_start + first_curve_count) % loop.Count();
	    if (!append_boolean_loop_arc(loop, first_start, first_curve_count,
		    first_arc) ||
		!append_boolean_loop_arc(loop, second_start,
		    loop.Count() - first_curve_count, second_arc))
	    {
		return false;
	    }
	    if (ON_CurvesAreEquivalent(&first_arc, &second_arc,
		ON_ZERO_TOLERANCE, true))
	    {
		return true;
	    }
	}
    }

    return false;
}


#endif
