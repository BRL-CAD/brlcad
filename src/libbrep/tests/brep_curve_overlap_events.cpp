/* BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#include "common.h"

#include <cmath>

#include "bu/app.h"
#include "bu/log.h"

#include "brep.h"
#include "brep/intersect.h"

#include "../brep_defines.h"


static const int EXPECTED_OVERLAP_COUNT = 2;
static const double FIRST_OVERLAP_Y = 0.0;
static const double SECOND_OVERLAP_Y = 10.0;
static const double OVERLAP_START_X = 1.0;
static const double OVERLAP_END_X = 3.0;
static const double ROUTE_DEPTH = -4.0;
static const double ROUTE_LEFT_X = -2.0;
static const double ROUTE_HEIGHT = 12.0;
static const double CSI_ROUTE_ELEVATION = 2.0;
static const double CSI_SURFACE_MIN_PARAMETER = -4.0;
static const double CSI_SURFACE_MAX_U_PARAMETER = 4.0;
static const int CCI_SUBDIVISION_COUNT = 1 << (MAX_CCI_DEPTH + 1);
static const int CCI_FALSE_OVERLAP_SAMPLES = 16;
static const int CCI_FALSE_OVERLAP_SEGMENTS_PER_SAMPLE = 2;
static const double CCI_FALSE_OVERLAP_HEIGHT = 1.0;
static const double CURVED_OVERLAP_RADIUS = 1.0;
static const double NEAR_LINEAR_CURVE_HEIGHT = INTERSECTION_TOL * 0.25;


static bool
is_expected_overlap(const ON_X_EVENT &event, double expected_y)
{
    const ON_3dPoint first_point = event.m_A[0];
    const ON_3dPoint second_point = event.m_A[1];
    if (std::fabs(first_point.y - expected_y) > INTERSECTION_TOL ||
	std::fabs(second_point.y - expected_y) > INTERSECTION_TOL) {
	return false;
    }

    ON_Interval x_range(first_point.x, second_point.x);
    x_range.MakeIncreasing();
    return std::fabs(x_range.Min() - OVERLAP_START_X) <= INTERSECTION_TOL &&
	std::fabs(x_range.Max() - OVERLAP_END_X) <= INTERSECTION_TOL;
}


static int
check_disconnected_curve_overlaps()
{
    ON_3dPointArray first_points;
    first_points.Append(ON_3dPoint(0.0, FIRST_OVERLAP_Y, 0.0));
    first_points.Append(ON_3dPoint(4.0, FIRST_OVERLAP_Y, 0.0));
    first_points.Append(ON_3dPoint(4.0, SECOND_OVERLAP_Y, 0.0));
    first_points.Append(ON_3dPoint(0.0, SECOND_OVERLAP_Y, 0.0));

    ON_3dPointArray second_points;
    second_points.Append(ON_3dPoint(OVERLAP_START_X, FIRST_OVERLAP_Y, 0.0));
    second_points.Append(ON_3dPoint(OVERLAP_END_X, FIRST_OVERLAP_Y, 0.0));
    second_points.Append(ON_3dPoint(OVERLAP_END_X, ROUTE_DEPTH, 0.0));
    second_points.Append(ON_3dPoint(ROUTE_LEFT_X, ROUTE_DEPTH, 0.0));
    second_points.Append(ON_3dPoint(ROUTE_LEFT_X, ROUTE_HEIGHT, 0.0));
    second_points.Append(ON_3dPoint(OVERLAP_START_X, ROUTE_HEIGHT, 0.0));
    second_points.Append(ON_3dPoint(OVERLAP_START_X, SECOND_OVERLAP_Y, 0.0));
    second_points.Append(ON_3dPoint(OVERLAP_END_X, SECOND_OVERLAP_Y, 0.0));

    ON_PolylineCurve first_curve(first_points);
    ON_PolylineCurve second_curve(second_points);
    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(&first_curve, &second_curve, events,
	INTERSECTION_TOL);
    if (result != EXPECTED_OVERLAP_COUNT ||
	events.Count() != EXPECTED_OVERLAP_COUNT) {
	bu_log("disconnected curve overlap returned %d events instead of %d\n",
	       result, EXPECTED_OVERLAP_COUNT);
	return 1;
    }

    int first_overlap_count = 0;
    int second_overlap_count = 0;
    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	const ON_X_EVENT &event = events[event_index];
	if (event.m_type != ON_X_EVENT::ccx_overlap) {
	    bu_log("disconnected curve overlap returned a non-overlap event\n");
	    return 1;
	}
	if (is_expected_overlap(event, FIRST_OVERLAP_Y)) {
	    first_overlap_count++;
	} else if (is_expected_overlap(event, SECOND_OVERLAP_Y)) {
	    second_overlap_count++;
	} else {
	    bu_log("disconnected curve overlap returned an unexpected interval\n");
	    return 1;
	}
    }

    if (first_overlap_count != 1 || second_overlap_count != 1) {
	bu_log("disconnected curve overlap did not preserve both intervals\n");
	return 1;
    }

    return 0;
}


static int
check_disconnected_curve_surface_overlaps()
{
    const ON_Plane plane(ON_3dPoint::Origin, ON_3dVector::XAxis,
	ON_3dVector::YAxis);
    ON_PlaneSurface surface(plane);
    if (!surface.SetDomain(0, CSI_SURFACE_MIN_PARAMETER,
	CSI_SURFACE_MAX_U_PARAMETER) ||
	!surface.SetDomain(1, CSI_SURFACE_MIN_PARAMETER, ROUTE_HEIGHT) ||
	!surface.SetExtents(0, surface.Domain(0)) ||
	!surface.SetExtents(1, surface.Domain(1))) {
	return 1;
    }

    ON_3dPointArray points;
    points.Append(ON_3dPoint(OVERLAP_START_X, FIRST_OVERLAP_Y, 0.0));
    points.Append(ON_3dPoint(OVERLAP_END_X, FIRST_OVERLAP_Y, 0.0));
    points.Append(ON_3dPoint(OVERLAP_END_X, FIRST_OVERLAP_Y,
	CSI_ROUTE_ELEVATION));
    points.Append(ON_3dPoint(OVERLAP_END_X, ROUTE_HEIGHT,
	CSI_ROUTE_ELEVATION));
    points.Append(ON_3dPoint(OVERLAP_START_X, ROUTE_HEIGHT,
	CSI_ROUTE_ELEVATION));
    points.Append(ON_3dPoint(OVERLAP_START_X, SECOND_OVERLAP_Y,
	CSI_ROUTE_ELEVATION));
    points.Append(ON_3dPoint(OVERLAP_START_X, SECOND_OVERLAP_Y, 0.0));
    points.Append(ON_3dPoint(OVERLAP_END_X, SECOND_OVERLAP_Y, 0.0));

    ON_PolylineCurve curve(points);
    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(&curve, &surface, events,
	INTERSECTION_TOL);
    if (result != EXPECTED_OVERLAP_COUNT ||
	events.Count() != EXPECTED_OVERLAP_COUNT) {
	bu_log("disconnected curve-surface overlap returned %d events instead of %d\n",
	       result, EXPECTED_OVERLAP_COUNT);
	return 1;
    }

    int first_overlap_count = 0;
    int second_overlap_count = 0;
    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	const ON_X_EVENT &event = events[event_index];
	if (event.m_type != ON_X_EVENT::csx_overlap) {
	    bu_log("disconnected curve-surface overlap returned a non-overlap event\n");
	    return 1;
	}
	if (is_expected_overlap(event, FIRST_OVERLAP_Y)) {
	    first_overlap_count++;
	} else if (is_expected_overlap(event, SECOND_OVERLAP_Y)) {
	    second_overlap_count++;
	} else {
	    bu_log("disconnected curve-surface overlap returned an unexpected interval\n");
	    return 1;
	}
    }

    if (first_overlap_count != 1 || second_overlap_count != 1) {
	bu_log("disconnected curve-surface overlap did not preserve both intervals\n");
	return 1;
    }

    return 0;
}


static int
check_degenerate_curve_surface_overlap_mapping()
{
    const ON_Plane plane(ON_3dPoint::Origin, ON_3dVector::XAxis,
	ON_3dVector::YAxis);
    ON_PlaneSurface surface(plane);
    if (!surface.SetDomain(0, -1.0, 1.0) ||
	!surface.SetDomain(1, -1.0, 1.0) ||
	!surface.SetExtents(0, surface.Domain(0)) ||
	!surface.SetExtents(1, surface.Domain(1)))
    {
	return 1;
    }

    ON_LineCurve point_curve(ON_3dPoint::Origin, ON_3dPoint::Origin);
    ON_SimpleArray<ON_X_EVENT> events;
    ON_CurveArray overlap_curves;
    const int result = ON_Intersect(&point_curve, &surface, events,
	INTERSECTION_TOL, 2.0 * INTERSECTION_TOL, NULL, NULL, NULL,
	&overlap_curves);
    if (result != 1 || events.Count() != 1 || overlap_curves.Count() != 1 ||
	events[0].m_type != ON_X_EVENT::csx_overlap ||
	overlap_curves[0] == NULL || overlap_curves[0]->Dimension() != 2)
    {
	bu_log("degenerate curve-surface overlap did not preserve its mapped "
	    "parameter curve\n");
	return 1;
    }
    return 0;
}


static int
check_curve_samples_do_not_prove_overlap()
{
    // Every former probe lands at a zero-height vertex.  Interleaved vertices
    // depart from the line far enough to disprove a continuous overlap.
    const int segment_count = CCI_SUBDIVISION_COUNT *
	CCI_FALSE_OVERLAP_SAMPLES * CCI_FALSE_OVERLAP_SEGMENTS_PER_SAMPLE;
    ON_LineCurve line(ON_3dPoint::Origin, ON_3dPoint(1.0, 0.0, 0.0));
    ON_3dPointArray points;
    for (int segment_index = 0; segment_index <= segment_count;
	++segment_index) {
	const double x = static_cast<double>(segment_index) / segment_count;
	const double y = segment_index % CCI_FALSE_OVERLAP_SEGMENTS_PER_SAMPLE ?
	    CCI_FALSE_OVERLAP_HEIGHT : 0.0;
	points.Append(ON_3dPoint(x, y, 0.0));
    }

    ON_PolylineCurve zigzag(points);
    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(&line, &zigzag, events,
	INTERSECTION_TOL);
    if (result < 0) {
	bu_log("curve sample overlap test exhausted its intersection budget\n");
	return 1;
    }

    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	if (events[event_index].m_type == ON_X_EVENT::ccx_overlap) {
	    bu_log("curve sample overlap test reported a false overlap\n");
	    return 1;
	}
    }

    return 0;
}


static int
check_near_linear_curve_overlap()
{
    ON_NurbsCurve curve(3, false, 3, 3);
    if (!curve.SetCV(0, ON_3dPoint::Origin) ||
	!curve.SetCV(1, ON_3dPoint(0.5, NEAR_LINEAR_CURVE_HEIGHT * 2.0,
	    0.0)) ||
	!curve.SetCV(2, ON_3dPoint(1.0, 0.0, 0.0)) ||
	!curve.MakeClampedUniformKnotVector()) {
	return 1;
    }

    ON_LineCurve line(ON_3dPoint::Origin, ON_3dPoint(1.0, 0.0, 0.0));
    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(&line, &curve, events,
	INTERSECTION_TOL);
    if (result != 1 || events.Count() != 1 ||
	events[0].m_type != ON_X_EVENT::ccx_overlap) {
	bu_log("near-linear curve overlap returned %d with %d events\n", result,
	    events.Count());
	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    bu_log("  event %d type %d, A %.17g -> %.17g, B %.17g -> "
		"%.17g\n", event_index, events[event_index].m_type,
		events[event_index].m_a[0], events[event_index].m_a[1],
		events[event_index].m_b[0], events[event_index].m_b[1]);
	}
	return 1;
    }

    return 0;
}


static int
check_curved_curve_overlap()
{
    const ON_Arc arc(ON_3dPoint(CURVED_OVERLAP_RADIUS, 0.0, 0.0),
	ON_3dPoint(0.0, CURVED_OVERLAP_RADIUS, 0.0),
	ON_3dPoint(-CURVED_OVERLAP_RADIUS, 0.0, 0.0));
    ON_ArcCurve arc_curve(arc);
    ON_NurbsCurve nurbs_curve;
    if (!arc.IsValid() || arc_curve.GetNurbForm(nurbs_curve) <= 0) {
	return 1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(&arc_curve, &nurbs_curve, events,
	INTERSECTION_TOL);
    if (result != 1 || events.Count() != 1 ||
	events[0].m_type != ON_X_EVENT::ccx_overlap) {
	bu_log("curved curve overlap returned %d events instead of one overlap\n",
	       result);
	return 1;
    }

    return 0;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_disconnected_curve_overlaps() +
	check_disconnected_curve_surface_overlaps() +
	check_degenerate_curve_surface_overlap_mapping() +
	check_curve_samples_do_not_prove_overlap() +
	check_near_linear_curve_overlap() +
	check_curved_curve_overlap();
}
