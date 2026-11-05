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


static const double SSI_INTERSECTION_TOLERANCE = INTERSECTION_TOL;
static const double SSI_OVERLAP_TOLERANCE = 2.0 * SSI_INTERSECTION_TOLERANCE;
static const double SSI_FITTING_TOLERANCE = SSI_INTERSECTION_TOLERANCE;
static const int SSI_COHERENCE_SAMPLE_COUNT = 25;
static const double SSI_CYLINDER_RADIUS = 6.0;
static const double SSI_CYLINDER_HEIGHT = 12.0;
static const double SSI_CYLINDER_OVERLAP_OFFSET = 3.0;
static const double SSI_CYLINDER_PATCH_START = 0.125;
static const double SSI_CYLINDER_PATCH_END = 0.375;
static const int SSI_PATCH_COUNT = 4;
static const int SSI_CENTER_CONTROL_INDEX = SSI_PATCH_COUNT / 2;
static const double SSI_PATCH_DOMAIN_LENGTH = (double)SSI_PATCH_COUNT;
static const double SSI_GAP_HEIGHT = 1.0;
static const int SSI_GAP_SUPPORT_SPAN_COUNT = 2;
static const int SSI_OFFSET_GAP_PATCH_COUNT = 8;
static const int SSI_OFFSET_GAP_CONTROL_U_INDEX = 2;
static const int SSI_OFFSET_GAP_CONTROL_V_INDEX = 2;
static const double SSI_OFFSET_GAP_PATCH_DOMAIN_LENGTH =
    (double)SSI_OFFSET_GAP_PATCH_COUNT;
static const double SSI_OFFSET_GAP_NORMALIZED_WIDTH =
    (double)SSI_GAP_SUPPORT_SPAN_COUNT / SSI_OFFSET_GAP_PATCH_COUNT;
static const double SSI_OFFSET_GAP_NORMALIZED_AREA = 1.0 -
    SSI_OFFSET_GAP_NORMALIZED_WIDTH * SSI_OFFSET_GAP_NORMALIZED_WIDTH;
static const double SSI_OFFSET_GAP_AREA_TOLERANCE = 1.0e-2;
static const int SSI_LOOP_SAMPLE_COUNT = 32;
static const double SSI_AFFINE_PLANAR_HALF_EXTENT = 128.0;
static const int SSI_AFFINE_PLANAR_EVENT_COUNT = 1;
static const int SSI_AFFINE_PLANAR_EMPTY_EVENT_COUNT = 0;
static const double SSI_AFFINE_PLANAR_DISJOINT_OFFSET =
    3.0 * SSI_AFFINE_PLANAR_HALF_EXTENT;


static bool
valid_ssi_curve(const char *curve_name, const ON_Curve *curve)
{
    if (curve == NULL) {
	bu_log("sphere-cylinder SSI returned a missing %s curve\n", curve_name);
	return false;
    }

    ON_wString messages;
    ON_TextLog log(messages);
    if (curve->IsValid(&log)) {
	return true;
    }

    ON_String message(messages);
    bu_log("sphere-cylinder SSI returned an invalid %s curve: %s\n",
	   curve_name, message.Array());
    return false;
}


static bool
coherent_ssi_event(const ON_SSX_EVENT &event,
		   const ON_Surface *surfaceA,
		   const ON_Surface *surfaceB)
{
    const ON_Interval domain = event.m_curve3d->Domain();
    if (event.m_curveA->Domain() != domain ||
	event.m_curveB->Domain() != domain) {
	bu_log("sphere-cylinder SSI returned inconsistent curve domains\n");
	return false;
    }

    for (int sample_index = 0;
	 sample_index <= SSI_COHERENCE_SAMPLE_COUNT;
	 ++sample_index) {
	const double parameter = domain.ParameterAt(
	    (double)sample_index / SSI_COHERENCE_SAMPLE_COUNT);
	const ON_3dPoint point3d = event.m_curve3d->PointAt(parameter);
	const ON_3dPoint pointA = event.m_curveA->PointAt(parameter);
	const ON_3dPoint pointB = event.m_curveB->PointAt(parameter);
	const double residualA =
	    surfaceA->PointAt(pointA.x, pointA.y).DistanceTo(point3d);
	const double residualB =
	    surfaceB->PointAt(pointB.x, pointB.y).DistanceTo(point3d);
	if (residualA > SSI_FITTING_TOLERANCE ||
	    residualB > SSI_FITTING_TOLERANCE) {
	    bu_log("sphere-cylinder SSI sample residuals exceed tolerance: "
		   "A=%g B=%g\n", residualA, residualB);
	    return false;
	}
    }
    return true;
}


static bool
make_affine_planar_surface(
    ON_NurbsSurface &surface,
    const ON_3dPoint &southwest,
    const ON_3dVector &u_direction,
    const ON_3dVector &v_direction)
{
    ON_NurbsSurface candidate(3, 0, 2, 2, 2, 2);
    if (!candidate.SetCV(0, 0, southwest) ||
	!candidate.SetCV(1, 0, southwest + u_direction) ||
	!candidate.SetCV(1, 1, southwest + u_direction + v_direction) ||
	!candidate.SetCV(0, 1, southwest + v_direction) ||
	!candidate.SetKnot(0, 0, 0.0) || !candidate.SetKnot(0, 1, 1.0) ||
	!candidate.SetKnot(1, 0, 0.0) || !candidate.SetKnot(1, 1, 1.0))
    {
	return false;
    }
    surface = candidate;
    return surface.IsValid();
}


static int
check_affine_planar_events()
{
    const double patch_width = 2.0 * SSI_AFFINE_PLANAR_HALF_EXTENT;
    ON_NurbsSurface first_surface;
    ON_NurbsSurface second_surface;
    ON_NurbsSurface disjoint_surface;
    if (!make_affine_planar_surface(first_surface,
	ON_3dPoint(-SSI_AFFINE_PLANAR_HALF_EXTENT,
	    -SSI_AFFINE_PLANAR_HALF_EXTENT, 0.0),
	ON_3dVector(patch_width, 0.0, 0.0),
	ON_3dVector(0.0, patch_width, 0.0)) ||
	!make_affine_planar_surface(second_surface,
	ON_3dPoint(-SSI_AFFINE_PLANAR_HALF_EXTENT, 0.0,
	    -SSI_AFFINE_PLANAR_HALF_EXTENT),
	ON_3dVector(patch_width, 0.0, 0.0),
	ON_3dVector(0.0, 0.0, patch_width)) ||
	!make_affine_planar_surface(disjoint_surface,
	ON_3dPoint(-SSI_AFFINE_PLANAR_HALF_EXTENT,
	    SSI_AFFINE_PLANAR_DISJOINT_OFFSET,
	    -SSI_AFFINE_PLANAR_HALF_EXTENT),
	ON_3dVector(patch_width, 0.0, 0.0),
	ON_3dVector(0.0, 0.0, patch_width)))
    {
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> events;
    const int result = ON_Intersect(&first_surface, &second_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	SSI_FITTING_TOLERANCE);
    if (result != SSI_AFFINE_PLANAR_EVENT_COUNT ||
	events.Count() != SSI_AFFINE_PLANAR_EVENT_COUNT)
    {
	bu_log("affine planar SSI returned %d events instead of %d\n", result,
	    SSI_AFFINE_PLANAR_EVENT_COUNT);
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> disjoint_events;
    const int disjoint_result = ON_Intersect(&first_surface,
	&disjoint_surface, disjoint_events, SSI_INTERSECTION_TOLERANCE,
	SSI_OVERLAP_TOLERANCE, SSI_FITTING_TOLERANCE);
    if (disjoint_result != SSI_AFFINE_PLANAR_EMPTY_EVENT_COUNT ||
	disjoint_events.Count() != SSI_AFFINE_PLANAR_EMPTY_EVENT_COUNT)
    {
	bu_log("disjoint affine planar SSI returned %d events instead of %d\n",
	    disjoint_result, SSI_AFFINE_PLANAR_EMPTY_EVENT_COUNT);
	return 1;
    }

    const ON_SSX_EVENT &event = events[0];
    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
	!valid_ssi_curve("affine planar 3D", event.m_curve3d) ||
	!valid_ssi_curve("affine planar surface A", event.m_curveA) ||
	!valid_ssi_curve("affine planar surface B", event.m_curveB) ||
	!coherent_ssi_event(event, &first_surface, &second_surface))
    {
	return 1;
    }

    const ON_BoundingBox curve_box = event.m_curve3d->BoundingBox();
    if (curve_box.m_min.x > -SSI_AFFINE_PLANAR_HALF_EXTENT +
	SSI_INTERSECTION_TOLERANCE ||
	curve_box.m_max.x < SSI_AFFINE_PLANAR_HALF_EXTENT -
	SSI_INTERSECTION_TOLERANCE ||
	fabs(curve_box.m_min.y) > SSI_OVERLAP_TOLERANCE ||
	fabs(curve_box.m_max.y) > SSI_OVERLAP_TOLERANCE ||
	fabs(curve_box.m_min.z) > SSI_OVERLAP_TOLERANCE ||
	fabs(curve_box.m_max.z) > SSI_OVERLAP_TOLERANCE)
    {
	bu_log("affine planar SSI returned incorrect bounds: "
	    "(%g, %g, %g)-(%g, %g, %g)\n", curve_box.m_min.x,
	    curve_box.m_min.y, curve_box.m_min.z, curve_box.m_max.x,
	    curve_box.m_max.y, curve_box.m_max.z);
	return 1;
    }

    return 0;
}


static bool
make_closed_ruled_surface(ON_NurbsSurface &surface,
	ON_Plane &base_plane, ON_3dVector &height)
{
    const ON_3dPoint base_origin(3341.115722656, 317.75390625,
	1165.860107422);
    const ON_3dVector first_axis(-1.73813533783, 0.0,
	4.773392677307);
    const ON_3dVector second_axis(4.773393630981, 0.0,
	1.73813533783);
    height = ON_3dVector(0.0, 22.86000061035, 0.0);
    base_plane = ON_Plane(base_origin, first_axis, second_axis);
    const ON_Plane top_plane(base_origin + height, first_axis, second_axis);
    if (!base_plane.IsValid() || !top_plane.IsValid())
	return false;

    ON_Ellipse base_ellipse(base_plane, first_axis.Length(),
	second_axis.Length());
    ON_Ellipse top_ellipse(top_plane, first_axis.Length(),
	second_axis.Length());
    ON_NurbsCurve base_curve, top_curve;
    if (base_ellipse.GetNurbForm(base_curve) == 0 ||
	top_ellipse.GetNurbForm(top_curve) == 0 ||
	!base_curve.SetDomain(0.0, 1.0) ||
	!top_curve.SetDomain(0.0, 1.0) ||
	!surface.CreateRuledSurface(base_curve, top_curve))
    {
	return false;
    }
    return surface.IsValid() && surface.IsClosed(0) &&
	surface.Degree(1) == 1 && surface.CVCount(1) == 2;
}


static int
check_planar_ruled_events()
{
    ON_NurbsSurface ruled_surface;
    ON_Plane base_plane;
    ON_3dVector height;
    if (!make_closed_ruled_surface(ruled_surface, base_plane, height))
	return 1;

    int failures = 0;
    const double fractions[2] = {0.0, 0.5};
    for (int fraction_index = 0; fraction_index < 2; ++fraction_index) {
	const double fraction = fractions[fraction_index];
	ON_Plane section_plane(base_plane);
	section_plane.origin += fraction * height;
	section_plane.UpdateEquation();
	ON_PlaneSurface plane_surface(section_plane);
	if (!plane_surface.SetExtents(0, ON_Interval(-10.0, 10.0)) ||
	    !plane_surface.SetExtents(1, ON_Interval(-10.0, 10.0)))
	{
	    failures++;
	    continue;
	}

	for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	    const ON_Surface *surfaceA = reverse_order ?
		static_cast<const ON_Surface *>(&ruled_surface) :
		static_cast<const ON_Surface *>(&plane_surface);
	    const ON_Surface *surfaceB = reverse_order ?
		static_cast<const ON_Surface *>(&plane_surface) :
		static_cast<const ON_Surface *>(&ruled_surface);
	    ON_ClassArray<ON_SSX_EVENT> events;
	    const int result = ON_Intersect(surfaceA, surfaceB, events,
		SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
		SSI_FITTING_TOLERANCE);
	    if (result != 1 || events.Count() != 1) {
		bu_log("planar-ruled SSI returned %d events instead of one "
		    "at fraction %g in order %d\n", result, fraction,
		    reverse_order);
		failures++;
		continue;
	    }

	    const ON_SSX_EVENT &event = events[0];
	    const ON_Curve *plane_curve = reverse_order ? event.m_curveB :
		event.m_curveA;
	    const ON_Curve *ruled_curve = reverse_order ? event.m_curveA :
		event.m_curveB;
	    if (!valid_ssi_curve("planar-ruled 3D", event.m_curve3d) ||
		!valid_ssi_curve("planar-ruled plane", plane_curve) ||
		!valid_ssi_curve("planar-ruled ruled surface", ruled_curve) ||
		!event.m_curve3d->IsClosed() || !plane_curve->IsClosed() ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const ON_3dPoint ruled_start = ruled_curve->PointAtStart();
	    const ON_3dPoint ruled_end = ruled_curve->PointAtEnd();
	    const ON_3dPoint lifted_start = ruled_surface.PointAt(
		ruled_start.x, ruled_start.y);
	    const ON_3dPoint lifted_end = ruled_surface.PointAt(
		ruled_end.x, ruled_end.y);
	    if (std::fabs(ruled_start.y - fraction) >
		    SSI_INTERSECTION_TOLERANCE ||
		std::fabs(ruled_end.y - fraction) >
		    SSI_INTERSECTION_TOLERANCE ||
		lifted_start.DistanceTo(lifted_end) >
		    SSI_INTERSECTION_TOLERANCE)
	    {
		bu_log("planar-ruled SSI did not preserve the ruled isocurve "
		    "at fraction %g in order %d\n", fraction, reverse_order);
		failures++;
	    }
	}
    }
    return failures;
}


static int
check_planar_parallel_ruled_events()
{
    ON_NurbsSurface ruled_surface;
    ON_Plane base_plane;
    ON_3dVector height;
    if (!make_closed_ruled_surface(ruled_surface, base_plane, height))
	return 1;

    /* This plane contains the ruling direction and bisects the closed
     * section.  Its intersection with the ruled surface is therefore two
     * complete ruling lines, not a short closed retrace near either root. */
    const ON_Plane ruling_plane(base_plane.origin, base_plane.xaxis, height);
    if (!ruling_plane.IsValid())
	return 1;
    ON_PlaneSurface plane_surface(ruling_plane);
    if (!plane_surface.SetExtents(0, ON_Interval(-64.0, 64.0)) ||
	!plane_surface.SetExtents(1, ON_Interval(-64.0, 64.0)))
    {
	return 1;
    }

    int failures = 0;
    ON_Interval ruling_domain = ruled_surface.Domain(1);
    ruling_domain.MakeIncreasing();
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(&ruled_surface) :
	    static_cast<const ON_Surface *>(&plane_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&plane_surface) :
	    static_cast<const ON_Surface *>(&ruled_surface);
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != 2 || events.Count() != 2) {
	    bu_log("parallel planar-ruled SSI returned %d events instead of "
		"two in order %d\n", result, reverse_order);
	    failures++;
	    continue;
	}

	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    const ON_SSX_EVENT &event = events[event_index];
	    const ON_Curve *plane_curve = reverse_order ? event.m_curveB :
		event.m_curveA;
	    const ON_Curve *ruled_curve = reverse_order ? event.m_curveA :
		event.m_curveB;
	    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
		!valid_ssi_curve("parallel planar-ruled 3D", event.m_curve3d) ||
		!valid_ssi_curve("parallel planar-ruled plane", plane_curve) ||
		!valid_ssi_curve("parallel planar-ruled ruled surface",
		    ruled_curve) ||
		event.m_curve3d->IsClosed() || plane_curve->IsClosed() ||
		ruled_curve->IsClosed() ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const ON_3dPoint ruled_start = ruled_curve->PointAtStart();
	    const ON_3dPoint ruled_end = ruled_curve->PointAtEnd();
	    const double endpoint_min = std::min(ruled_start.y, ruled_end.y);
	    const double endpoint_max = std::max(ruled_start.y, ruled_end.y);
	    if (std::fabs(endpoint_min - ruling_domain.Min()) >
		    SSI_INTERSECTION_TOLERANCE ||
		std::fabs(endpoint_max - ruling_domain.Max()) >
		    SSI_INTERSECTION_TOLERANCE)
	    {
		bu_log("parallel planar-ruled SSI event %d did not span the "
		    "complete ruling domain in order %d\n", event_index,
		    reverse_order);
		failures++;
	    }
	}
    }
    return failures;
}


static int
check_planar_oblique_tapered_ruled_events()
{
    const ON_Plane base_plane(ON_3dPoint(3.0, -2.0, 0.0),
	ON_3dVector::XAxis, ON_3dVector::YAxis);
    const ON_Plane top_plane(ON_3dPoint(4.5, -1.5, 12.0),
	ON_3dVector::XAxis, ON_3dVector::YAxis);
    ON_Ellipse base_ellipse(base_plane, 6.0, 4.0);
    ON_Ellipse top_ellipse(top_plane, 3.5, 2.5);
    ON_NurbsCurve base_curve, top_curve;
    ON_NurbsSurface ruled_surface;
    if (base_ellipse.GetNurbForm(base_curve) == 0 ||
	top_ellipse.GetNurbForm(top_curve) == 0 ||
	!base_curve.SetDomain(0.0, 1.0) ||
	!top_curve.SetDomain(0.0, 1.0) ||
	!ruled_surface.CreateRuledSurface(base_curve, top_curve) ||
	!ruled_surface.IsValid() || !ruled_surface.IsClosed(0) ||
	ruled_surface.Degree(1) != 1 || ruled_surface.CVCount(1) != 2)
    {
	return 1;
    }

    /* y = -0.5 + z/12 cuts every elliptical transverse section twice, but
     * neither branch is a constant transverse parameter or a complete
     * ruling. */
    const ON_Plane section_plane(ON_3dPoint(0.0, -0.5, 0.0),
	ON_3dVector::XAxis, ON_3dVector(0.0, 1.0, 12.0));
    if (!section_plane.IsValid())
	return 1;
    ON_PlaneSurface plane_surface(section_plane);
    if (!plane_surface.SetExtents(0, ON_Interval(-32.0, 32.0)) ||
	!plane_surface.SetExtents(1, ON_Interval(-32.0, 32.0)))
    {
	return 1;
    }

    ON_Interval ruling_domain = ruled_surface.Domain(1);
    ruling_domain.MakeIncreasing();
    int failures = 0;
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(&ruled_surface) :
	    static_cast<const ON_Surface *>(&plane_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&plane_surface) :
	    static_cast<const ON_Surface *>(&ruled_surface);
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != 2 || events.Count() != 2) {
	    bu_log("oblique tapered planar-ruled SSI returned %d events "
		"instead of two in order %d\n", result, reverse_order);
	    failures++;
	    continue;
	}

	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    const ON_SSX_EVENT &event = events[event_index];
	    const ON_Curve *plane_curve = reverse_order ? event.m_curveB :
		event.m_curveA;
	    const ON_Curve *ruled_curve = reverse_order ? event.m_curveA :
		event.m_curveB;
	    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
		!valid_ssi_curve("oblique tapered planar-ruled 3D",
		    event.m_curve3d) ||
		!valid_ssi_curve("oblique tapered planar-ruled plane",
		    plane_curve) ||
		!valid_ssi_curve("oblique tapered planar-ruled surface",
		    ruled_curve) ||
		event.m_curve3d->IsClosed() || plane_curve->IsClosed() ||
		ruled_curve->IsClosed() ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const ON_3dPoint ruled_start = ruled_curve->PointAtStart();
	    const ON_3dPoint ruled_end = ruled_curve->PointAtEnd();
	    const double endpoint_min = std::min(ruled_start.y,
		ruled_end.y);
	    const double endpoint_max = std::max(ruled_start.y,
		ruled_end.y);
	    if (std::fabs(endpoint_min - ruling_domain.Min()) >
		    SSI_INTERSECTION_TOLERANCE ||
		std::fabs(endpoint_max - ruling_domain.Max()) >
		    SSI_INTERSECTION_TOLERANCE)
	    {
		bu_log("oblique tapered planar-ruled SSI event %d did not "
		    "span the ruling domain in order %d\n", event_index,
		    reverse_order);
		failures++;
	    }
	}
    }
    return failures;
}


static int
check_planar_revolution_events()
{
    static const double radius = 5.0;
    static const double half_length = 20.0;
    const ON_3dPoint base(0.0, -half_length, 0.0);
    const ON_Plane cylinder_plane(base, ON_3dVector::YAxis);
    const ON_Cylinder cylinder(ON_Circle(cylinder_plane, radius),
	2.0 * half_length);
    std::unique_ptr<ON_RevSurface> revolution(cylinder.RevSurfaceForm());
    if (!revolution || !revolution->SetDomain(0, 0.0,
	    2.0 * ON_PI * radius) || !revolution->IsValid())
    {
	return 1;
    }

    const ON_Plane section_plane(ON_3dPoint(0.0, -10.0, 0.0),
	ON_3dVector::YAxis);
    ON_PlaneSurface plane_surface(section_plane);
    if (!plane_surface.SetExtents(0, ON_Interval(-10.0, 10.0)) ||
	!plane_surface.SetExtents(1, ON_Interval(-10.0, 10.0)))
    {
	return 1;
    }

    int failures = 0;
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(revolution.get()) :
	    static_cast<const ON_Surface *>(&plane_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&plane_surface) :
	    static_cast<const ON_Surface *>(revolution.get());
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != 1 || events.Count() != 1) {
	    bu_log("planar-revolution SSI returned %d events instead of one "
		"in order %d\n", result, reverse_order);
	    failures++;
	    continue;
	}

	const ON_SSX_EVENT &event = events[0];
	const ON_Curve *plane_curve = reverse_order ? event.m_curveB :
	    event.m_curveA;
	const ON_Curve *revolution_curve = reverse_order ? event.m_curveA :
	    event.m_curveB;
	if (!valid_ssi_curve("planar-revolution 3D", event.m_curve3d) ||
	    !valid_ssi_curve("planar-revolution plane", plane_curve) ||
	    !valid_ssi_curve("planar-revolution surface", revolution_curve) ||
	    !event.m_curve3d->IsClosed() || !plane_curve->IsClosed() ||
	    !coherent_ssi_event(event, surfaceA, surfaceB))
	{
	    failures++;
	    continue;
	}

	const ON_3dPoint start = revolution_curve->PointAtStart();
	const ON_3dPoint end = revolution_curve->PointAtEnd();
	const ON_3dPoint lifted_start = revolution->PointAt(start.x, start.y);
	const ON_3dPoint lifted_end = revolution->PointAt(end.x, end.y);
	if (lifted_start.DistanceTo(lifted_end) >
	    SSI_INTERSECTION_TOLERANCE)
	{
	    bu_log("planar-revolution SSI did not preserve the closed "
		"rotation isocurve in order %d\n", reverse_order);
	    failures++;
	}
    }
    return failures;
}


static bool
make_elliptical_revolution(ON_RevSurface &surface)
{
    const ON_Plane profile_plane(ON_3dPoint(8.0, 0.0, 0.0),
	ON_3dVector::XAxis, ON_3dVector::ZAxis);
    const ON_Ellipse profile_ellipse(profile_plane, 3.0, 2.0);
    ON_NurbsCurve *profile = ON_NurbsCurve::New();
    if (!profile_plane.IsValid() ||
	profile_ellipse.GetNurbForm(*profile) == 0 || !profile->IsValid())
    {
	delete profile;
	return false;
    }

    surface.m_curve = profile;
    surface.m_axis = ON_Line(ON_3dPoint::Origin,
	ON_3dPoint(0.0, 0.0, 1.0));
    surface.m_angle = ON_Interval(0.0, 2.0 * ON_PI);
    surface.m_t = surface.m_angle;
    surface.m_bTransposed = false;
    surface.BoundingBox();
    return surface.IsValid() && surface.IsClosed(0) && surface.IsClosed(1);
}


static int
check_planar_elliptical_revolution_events()
{
    ON_RevSurface revolution;
    if (!make_elliptical_revolution(revolution))
	return 1;

    ON_PlaneSurface plane_surface(ON_Plane(ON_3dPoint::Origin,
	ON_3dVector::ZAxis));
    if (!plane_surface.SetExtents(0, ON_Interval(-16.0, 16.0)) ||
	!plane_surface.SetExtents(1, ON_Interval(-16.0, 16.0)))
    {
	return 1;
    }

    int failures = 0;
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(&revolution) :
	    static_cast<const ON_Surface *>(&plane_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&plane_surface) :
	    static_cast<const ON_Surface *>(&revolution);
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != 2 || events.Count() != 2) {
	    bu_log("planar-elliptical-revolution SSI returned %d events "
		"instead of two in order %d\n", result, reverse_order);
	    failures++;
	    continue;
	}

	bool inner_circle = false;
	bool outer_circle = false;
	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    const ON_SSX_EVENT &event = events[event_index];
	    const ON_Curve *plane_curve = reverse_order ? event.m_curveB :
		event.m_curveA;
	    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
		!valid_ssi_curve("planar-elliptical-revolution 3D",
		    event.m_curve3d) ||
		!valid_ssi_curve("planar-elliptical-revolution plane",
		    plane_curve) ||
		!event.m_curve3d->IsClosed() || !plane_curve->IsClosed() ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const ON_3dPoint point = event.m_curve3d->PointAt(
		event.m_curve3d->Domain().Mid());
	    const double radius = std::sqrt(point.x * point.x +
		point.y * point.y);
	    if (std::fabs(point.z) > SSI_FITTING_TOLERANCE) {
		failures++;
	    } else if (std::fabs(radius - 5.0) <=
		SSI_FITTING_TOLERANCE) {
		inner_circle = true;
	    } else if (std::fabs(radius - 11.0) <=
		SSI_FITTING_TOLERANCE) {
		outer_circle = true;
	    } else {
		bu_log("planar-elliptical-revolution SSI returned radius %g\n",
		    radius);
		failures++;
	    }
	}
	if (!inner_circle || !outer_circle) {
	    bu_log("planar-elliptical-revolution SSI omitted a circle in "
		"order %d\n", reverse_order);
	    failures++;
	}
    }
    return failures;
}


static int
check_parallel_planar_torus_events()
{
    static const double major_radius = 0.5;
    static const double minor_radius = 0.01;
    static const double plane_offset = 0.0072;
    ON_Torus torus(ON_Plane(ON_3dPoint::Origin, ON_3dVector::ZAxis),
	major_radius, minor_radius);
    std::unique_ptr<ON_RevSurface> revolution(torus.RevSurfaceForm());
    ON_3dVector plane_normal(1.0, -1.0, 0.0);
    if (!revolution || !revolution->IsValid() ||
	!plane_normal.Unitize())
    {
	return 1;
    }

    const ON_3dPoint plane_origin = ON_3dPoint::Origin +
	plane_offset * plane_normal;
    const ON_Plane section_plane(plane_origin, plane_normal);
    ON_PlaneSurface plane_surface(section_plane);
    if (!section_plane.IsValid() ||
	!plane_surface.SetExtents(0, ON_Interval(-1.0, 1.0)) ||
	!plane_surface.SetExtents(1, ON_Interval(-1.0, 1.0)))
    {
	return 1;
    }

    const int expected_event_count = 2 * revolution->m_curve->SpanCount();
    int failures = 0;
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(revolution.get()) :
	    static_cast<const ON_Surface *>(&plane_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&plane_surface) :
	    static_cast<const ON_Surface *>(revolution.get());
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != expected_event_count ||
	    events.Count() != expected_event_count)
	{
	    bu_log("parallel planar-torus SSI returned %d events instead of "
		"%d in order %d\n", result, expected_event_count,
		reverse_order);
	    failures++;
	    continue;
	}

	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    const ON_SSX_EVENT &event = events[event_index];
	    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
		!valid_ssi_curve("parallel planar-torus 3D", event.m_curve3d) ||
		!valid_ssi_curve("parallel planar-torus surface A",
		    event.m_curveA) ||
		!valid_ssi_curve("parallel planar-torus surface B",
		    event.m_curveB) ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const ON_3dPoint endpoints[2] = {
		event.m_curve3d->PointAtStart(),
		event.m_curve3d->PointAtEnd()
	    };
	    for (int endpoint_index = 0; endpoint_index < 2;
		 ++endpoint_index)
	    {
		int match_count = 0;
		for (int peer_index = 0; peer_index < events.Count();
		     ++peer_index)
		{
		    const ON_SSX_EVENT &peer = events[peer_index];
		    const ON_3dPoint peer_endpoints[2] = {
			peer.m_curve3d->PointAtStart(),
			peer.m_curve3d->PointAtEnd()
		    };
		    for (int peer_endpoint = 0; peer_endpoint < 2;
			 ++peer_endpoint)
		    {
			if (peer_index == event_index &&
			    peer_endpoint == endpoint_index)
			{
			    continue;
			}
			if (endpoints[endpoint_index].DistanceTo(
				peer_endpoints[peer_endpoint]) <=
			    SSI_INTERSECTION_TOLERANCE)
			{
			    match_count++;
			}
		    }
		}
		if (match_count != 1) {
		    bu_log("parallel planar-torus SSI endpoint has %d peers "
			"instead of one in order %d\n", match_count,
			reverse_order);
		    failures++;
		}
	    }
	}
    }
    return failures;
}


static bool
curves_form_closed_pair(const ON_Curve &first, const ON_Curve &second)
{
    const ON_3dPoint first_start = first.PointAtStart();
    const ON_3dPoint first_end = first.PointAtEnd();
    const ON_3dPoint second_start = second.PointAtStart();
    const ON_3dPoint second_end = second.PointAtEnd();
    return (first_start.DistanceTo(second_start) <=
		SSI_FITTING_TOLERANCE &&
	    first_end.DistanceTo(second_end) <= SSI_FITTING_TOLERANCE) ||
	(first_start.DistanceTo(second_end) <= SSI_FITTING_TOLERANCE &&
	    first_end.DistanceTo(second_start) <= SSI_FITTING_TOLERANCE);
}


static int
check_coaxial_elliptical_revolution_events(bool offset_seam)
{
    static const double cylinder_radius = 9.5;
    static const double cylinder_height = 8.0;
    const ON_3dPoint cylinder_base(0.0, 0.0, -0.5 * cylinder_height);
    const ON_3dVector cylinder_x = offset_seam ?
	ON_3dVector::YAxis : ON_3dVector::XAxis;
    const ON_3dVector cylinder_y = offset_seam ?
	ON_3dVector(-1.0, 0.0, 0.0) : ON_3dVector::YAxis;
    const ON_Plane cylinder_plane(cylinder_base, cylinder_x, cylinder_y);
    const ON_Cylinder cylinder(ON_Circle(cylinder_plane, cylinder_radius),
	cylinder_height);
    ON_NurbsSurface cylinder_surface;
    ON_RevSurface revolution;
    if (!cylinder_plane.IsValid() ||
	cylinder.GetNurbForm(cylinder_surface) == 0 ||
	!cylinder_surface.IsValid() ||
	!make_elliptical_revolution(revolution))
    {
	return 1;
    }

    const int expected_event_count = offset_seam ? 4 : 2;
    const double expected_height = std::sqrt(3.0);
    int failures = 0;
    for (int reverse_order = 0; reverse_order < 2; ++reverse_order) {
	const ON_Surface *surfaceA = reverse_order ?
	    static_cast<const ON_Surface *>(&revolution) :
	    static_cast<const ON_Surface *>(&cylinder_surface);
	const ON_Surface *surfaceB = reverse_order ?
	    static_cast<const ON_Surface *>(&cylinder_surface) :
	    static_cast<const ON_Surface *>(&revolution);
	ON_ClassArray<ON_SSX_EVENT> events;
	const int result = ON_Intersect(surfaceA, surfaceB, events,
	    SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	    SSI_FITTING_TOLERANCE);
	if (result != expected_event_count ||
	    events.Count() != expected_event_count)
	{
	    bu_log("coaxial elliptical-revolution SSI returned %d events "
		"instead of %d with offset-seam=%d in order %d\n", result,
		expected_event_count, offset_seam, reverse_order);
	    failures++;
	    continue;
	}

	std::vector<const ON_Curve *> lower_curves;
	std::vector<const ON_Curve *> upper_curves;
	for (int event_index = 0; event_index < events.Count(); ++event_index) {
	    const ON_SSX_EVENT &event = events[event_index];
	    if (event.m_type != ON_SSX_EVENT::ssx_transverse ||
		!valid_ssi_curve("coaxial elliptical-revolution 3D",
		    event.m_curve3d) ||
		!valid_ssi_curve("coaxial elliptical-revolution surface A",
		    event.m_curveA) ||
		!valid_ssi_curve("coaxial elliptical-revolution surface B",
		    event.m_curveB) ||
		!coherent_ssi_event(event, surfaceA, surfaceB))
	    {
		failures++;
		continue;
	    }

	    const bool closed = event.m_curve3d->IsClosed();
	    if (closed == offset_seam) {
		bu_log("coaxial elliptical-revolution SSI returned an "
		    "unexpectedly %s curve with offset-seam=%d\n",
		    closed ? "closed" : "open", offset_seam);
		failures++;
	    }
	    const ON_3dPoint point = event.m_curve3d->PointAt(
		event.m_curve3d->Domain().Mid());
	    const double radius = std::sqrt(point.x * point.x +
		point.y * point.y);
	    if (std::fabs(radius - cylinder_radius) >
		    SSI_FITTING_TOLERANCE ||
		std::fabs(std::fabs(point.z) - expected_height) >
		    SSI_FITTING_TOLERANCE)
	    {
		bu_log("coaxial elliptical-revolution SSI returned point "
		    "(%g, %g, %g) off the expected circles\n", point.x,
		    point.y, point.z);
		failures++;
	    }
	    if (point.z < 0.0)
		lower_curves.push_back(event.m_curve3d);
	    else
		upper_curves.push_back(event.m_curve3d);
	}

	const size_t expected_curves_per_circle = offset_seam ? 2 : 1;
	if (lower_curves.size() != expected_curves_per_circle ||
	    upper_curves.size() != expected_curves_per_circle)
	{
	    bu_log("coaxial elliptical-revolution SSI returned incorrect "
		"circle multiplicities with offset-seam=%d\n", offset_seam);
	    failures++;
	} else if (offset_seam &&
	    (!curves_form_closed_pair(*lower_curves[0], *lower_curves[1]) ||
	     !curves_form_closed_pair(*upper_curves[0], *upper_curves[1])))
	{
	    bu_log("coaxial elliptical-revolution SSI offset-seam arcs do "
		"not form closed circle pairs\n");
	    failures++;
	}
    }
    return failures;
}


static int
check_sphere_cylinder_events()
{
    static const double SPHERE_RADIUS = 10.0;
    static const double CYLINDER_RADIUS = 6.0;
    static const double CYLINDER_HEIGHT = 30.0;
    const ON_3dPoint cylinder_base(0.0, 0.0, -CYLINDER_HEIGHT * 0.5);
    const ON_Sphere sphere(ON_3dPoint::Origin, SPHERE_RADIUS);
    const ON_Cylinder cylinder(ON_Circle(cylinder_base, CYLINDER_RADIUS),
	CYLINDER_HEIGHT);

    ON_Brep *sphere_brep = ON_BrepSphere(sphere);
    ON_Brep *cylinder_brep = ON_BrepCylinder(cylinder, true, true);
    if (sphere_brep == NULL || cylinder_brep == NULL ||
	!sphere_brep->IsValid() || !cylinder_brep->IsValid()) {
	delete sphere_brep;
	delete cylinder_brep;
	return 1;
    }

    int failures = 0;
    int closed_curve_count = 0;
    for (int sphere_face_index = 0;
	 sphere_face_index < sphere_brep->m_F.Count(); ++sphere_face_index) {
	const ON_Surface *sphere_surface =
	    sphere_brep->m_F[sphere_face_index].SurfaceOf();
	if (sphere_surface == NULL) {
	    failures++;
	    continue;
	}

	for (int cylinder_face_index = 0;
	     cylinder_face_index < cylinder_brep->m_F.Count(); ++cylinder_face_index) {
	    const ON_Surface *cylinder_surface =
		cylinder_brep->m_F[cylinder_face_index].SurfaceOf();
	    if (cylinder_surface == NULL) {
		failures++;
		continue;
	    }

	    ON_ClassArray<ON_SSX_EVENT> events;
	    if (ON_Intersect(sphere_surface, cylinder_surface, events,
		SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
		SSI_FITTING_TOLERANCE) <= 0) {
		continue;
	    }

	    for (int event_index = 0; event_index < events.Count(); ++event_index) {
		const ON_SSX_EVENT &event = events[event_index];
		if (event.m_type != ON_SSX_EVENT::ssx_transverse &&
		    event.m_type != ON_SSX_EVENT::ssx_tangent &&
		    event.m_type != ON_SSX_EVENT::ssx_overlap) {
		    continue;
		}
		const bool valid_curve3d = valid_ssi_curve("3D", event.m_curve3d);
		const bool valid_curveA = valid_ssi_curve("surface A", event.m_curveA);
		const bool valid_curveB = valid_ssi_curve("surface B", event.m_curveB);
		if (!valid_curve3d || !valid_curveA || !valid_curveB) {
		    failures++;
		    continue;
		}
		if (!coherent_ssi_event(event, sphere_surface, cylinder_surface)) {
		    failures++;
		    continue;
		}
		if (event.m_curve3d->IsClosed()) {
		    closed_curve_count++;
		}
	    }
	}
    }

    if (closed_curve_count == 0) {
	bu_log("sphere-cylinder SSI did not return a closed curve\n");
	failures++;
    }

    delete sphere_brep;
    delete cylinder_brep;
    return failures;
}


static int
check_coincident_cylinder_events()
{
    const ON_Cylinder first_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0, -SSI_CYLINDER_HEIGHT * 0.5),
		  SSI_CYLINDER_RADIUS),
	SSI_CYLINDER_HEIGHT);
    const ON_Cylinder second_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0,
		SSI_CYLINDER_OVERLAP_OFFSET - SSI_CYLINDER_HEIGHT * 0.5),
		  SSI_CYLINDER_RADIUS),
	SSI_CYLINDER_HEIGHT);
    ON_NurbsSurface first_surface;
    ON_NurbsSurface second_surface;
    if (first_cylinder.GetNurbForm(first_surface) == 0 ||
	second_cylinder.GetNurbForm(second_surface) == 0) {
	return 1;
    }

    const ON_Interval first_u = first_surface.Domain(0);
    const ON_Interval second_u = second_surface.Domain(0);
    const ON_Interval first_patch_u(first_u.ParameterAt(SSI_CYLINDER_PATCH_START),
	first_u.ParameterAt(SSI_CYLINDER_PATCH_END));
    const ON_Interval second_patch_u(second_u.ParameterAt(SSI_CYLINDER_PATCH_START),
	second_u.ParameterAt(SSI_CYLINDER_PATCH_END));
    if (!first_surface.Trim(0, first_patch_u) ||
	!second_surface.Trim(0, second_patch_u)) {
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> events;
    ON_Intersect(&first_surface, &second_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	SSI_FITTING_TOLERANCE);

    int failures = 0;
    int overlap_count = 0;
    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	const ON_SSX_EVENT &event = events[event_index];
	if (event.m_type != ON_SSX_EVENT::ssx_overlap) {
	    continue;
	}
	overlap_count++;
	if (!valid_ssi_curve("coincident-cylinder 3D", event.m_curve3d) ||
	    !valid_ssi_curve("coincident-cylinder surface A", event.m_curveA) ||
	    !valid_ssi_curve("coincident-cylinder surface B", event.m_curveB) ||
	    !coherent_ssi_event(event, &first_surface, &second_surface)) {
	    failures++;
	}
    }

    if (overlap_count == 0) {
	bu_log("coincident-cylinder SSI did not return an overlap event\n");
	failures++;
    }

    return failures;
}


static int
check_closed_curve_surface_overlap()
{
    const ON_Cylinder cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0, -SSI_CYLINDER_HEIGHT * 0.5),
		  SSI_CYLINDER_RADIUS),
	SSI_CYLINDER_HEIGHT);
    const ON_Circle circle(ON_3dPoint::Origin, SSI_CYLINDER_RADIUS);
    ON_NurbsSurface cylinder_surface;
    ON_NurbsCurve circle_curve;
    if (cylinder.GetNurbForm(cylinder_surface) == 0 ||
	circle.GetNurbForm(circle_curve) == 0) {
	return 1;
    }

    ON_SimpleArray<ON_X_EVENT> events;
    ON_Intersect(&circle_curve, &cylinder_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE);

    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	if (events[event_index].m_type == ON_X_EVENT::csx_overlap) {
	    return 0;
	}
    }

    bu_log("closed curve-on-surface SSI did not return an overlap event\n");
    return 1;
}


static int
check_periodic_cylinder_budget()
{
    const ON_Cylinder first_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0, -SSI_CYLINDER_HEIGHT * 0.5),
		  SSI_CYLINDER_RADIUS),
	SSI_CYLINDER_HEIGHT);
    const ON_Cylinder second_cylinder(
	ON_Circle(ON_3dPoint(0.0, 0.0,
		SSI_CYLINDER_OVERLAP_OFFSET - SSI_CYLINDER_HEIGHT * 0.5),
		  SSI_CYLINDER_RADIUS),
	SSI_CYLINDER_HEIGHT);
    ON_NurbsSurface first_surface;
    ON_NurbsSurface second_surface;
    if (first_cylinder.GetNurbForm(first_surface) == 0 ||
	second_cylinder.GetNurbForm(second_surface) == 0) {
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> events;
    const int result = ON_Intersect(&first_surface, &second_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	SSI_FITTING_TOLERANCE);
    if (result != BREP_SSI_BUDGET_EXCEEDED) {
	bu_log("periodic-cylinder SSI returned %d instead of a budget result\n",
	       result);
	return 1;
    }
    if (events.Count() != 0) {
	bu_log("periodic-cylinder SSI retained partial events after exhaustion\n");
	return 1;
    }

    return 0;
}


static bool
make_single_gap_surface(ON_NurbsSurface &surface,
			int patch_count,
			int elevated_u_index,
			int elevated_v_index)
{
    ON_NurbsSurface candidate(3, 0, 2, 2,
	patch_count + 1, patch_count + 1);
    for (int knot_index = 0; knot_index <= patch_count; ++knot_index) {
	if (!candidate.SetKnot(0, knot_index, (double)knot_index) ||
	    !candidate.SetKnot(1, knot_index, (double)knot_index)) {
	    return false;
	}
    }

    for (int u_index = 0; u_index <= patch_count; ++u_index) {
	const double u_coordinate = (double)u_index;
	for (int v_index = 0; v_index <= patch_count; ++v_index) {
	    const double v_coordinate = (double)v_index;
	    const double elevation =
		(u_index == elevated_u_index &&
		 v_index == elevated_v_index) ? SSI_GAP_HEIGHT : 0.0;
	    if (!candidate.SetCV(u_index, v_index,
		ON_3dPoint(u_coordinate, v_coordinate, elevation))) {
		return false;
	    }
	}
    }

    if (!candidate.IsValid()) {
	return false;
    }

    surface = candidate;
    return true;
}


static bool
make_center_gap_surface(ON_NurbsSurface &surface)
{
    return make_single_gap_surface(surface, SSI_PATCH_COUNT,
	SSI_CENTER_CONTROL_INDEX, SSI_CENTER_CONTROL_INDEX);
}


static double
signed_parameter_area(const ON_Curve &curve)
{
    const ON_Interval domain = curve.Domain();
    ON_3dPoint previous = curve.PointAt(domain.Min());
    double doubled_area = 0.0;
    for (int sample_index = 1; sample_index <= SSI_LOOP_SAMPLE_COUNT; ++sample_index) {
	const ON_3dPoint current = curve.PointAt(domain.ParameterAt(
	    (double)sample_index / SSI_LOOP_SAMPLE_COUNT));
	doubled_area += previous.x * current.y - current.x * previous.y;
	previous = current;
    }
    return doubled_area * 0.5;
}


static int
check_nested_overlap_events()
{
    ON_PlaneSurface plane_surface(ON_Plane(ON_3dPoint::Origin,
	ON_3dVector::ZAxis));
    if (!plane_surface.SetExtents(0, ON_Interval(0.0, SSI_PATCH_DOMAIN_LENGTH)) ||
	!plane_surface.SetExtents(1, ON_Interval(0.0, SSI_PATCH_DOMAIN_LENGTH))) {
	return 1;
    }

    ON_NurbsSurface center_gap_surface;
    if (!make_center_gap_surface(center_gap_surface)) {
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> events;
    ON_Intersect(&plane_surface, &center_gap_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	SSI_FITTING_TOLERANCE);

    int failures = 0;
    int positive_loop_count = 0;
    int negative_loop_count = 0;
    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	const ON_SSX_EVENT &event = events[event_index];
	if (event.m_type != ON_SSX_EVENT::ssx_overlap) {
	    continue;
	}
	if (!valid_ssi_curve("nested-overlap 3D", event.m_curve3d) ||
	    !valid_ssi_curve("nested-overlap surface A", event.m_curveA) ||
	    !valid_ssi_curve("nested-overlap surface B", event.m_curveB) ||
	    !coherent_ssi_event(event, &plane_surface, &center_gap_surface)) {
	    failures++;
	    continue;
	}

	const double area = signed_parameter_area(*event.m_curveA);
	if (area > 0.0) {
	    positive_loop_count++;
	} else if (area < 0.0) {
	    negative_loop_count++;
	}
    }

    if (positive_loop_count == 0 || negative_loop_count == 0) {
	bu_log("nested surface overlap did not return both loop orientations\n");
	failures++;
    }

    return failures;
}


static int
check_offset_gap_overlap_events()
{
    ON_PlaneSurface plane_surface(ON_Plane(ON_3dPoint::Origin,
	ON_3dVector::ZAxis));
    if (!plane_surface.SetExtents(0,
	ON_Interval(0.0, SSI_OFFSET_GAP_PATCH_DOMAIN_LENGTH)) ||
	!plane_surface.SetExtents(1,
	ON_Interval(0.0, SSI_OFFSET_GAP_PATCH_DOMAIN_LENGTH))) {
	return 1;
    }

    ON_NurbsSurface offset_gap_surface;
    if (!make_single_gap_surface(offset_gap_surface, SSI_OFFSET_GAP_PATCH_COUNT,
	SSI_OFFSET_GAP_CONTROL_U_INDEX, SSI_OFFSET_GAP_CONTROL_V_INDEX)) {
	return 1;
    }

    ON_ClassArray<ON_SSX_EVENT> events;
    ON_Intersect(&plane_surface, &offset_gap_surface, events,
	SSI_INTERSECTION_TOLERANCE, SSI_OVERLAP_TOLERANCE,
	SSI_FITTING_TOLERANCE);

    int failures = 0;
    int positive_loop_count = 0;
    int negative_loop_count = 0;
    double signed_overlap_area = 0.0;
    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	if (events[event_index].m_type == ON_SSX_EVENT::ssx_overlap) {
	    const ON_SSX_EVENT &event = events[event_index];
	    if (!valid_ssi_curve("offset-overlap 3D", event.m_curve3d) ||
		!valid_ssi_curve("offset-overlap surface A", event.m_curveA) ||
		!valid_ssi_curve("offset-overlap surface B", event.m_curveB) ||
		!coherent_ssi_event(event, &plane_surface, &offset_gap_surface)) {
		failures++;
		continue;
	    }
	    const double area =
		signed_parameter_area(*event.m_curveA);
	    signed_overlap_area += area;
	    if (area > 0.0) {
		positive_loop_count++;
	    } else if (area < 0.0) {
		negative_loop_count++;
	    }
	}
    }

    if (positive_loop_count != 1 || negative_loop_count != 1) {
	bu_log("offset surface overlap returned incorrect loop topology\n");
	failures++;
    }
    if (fabs(signed_overlap_area - SSI_OFFSET_GAP_NORMALIZED_AREA) >
	SSI_OFFSET_GAP_AREA_TOLERANCE) {
	bu_log("offset surface overlap area is %g instead of %g\n",
	       signed_overlap_area, SSI_OFFSET_GAP_NORMALIZED_AREA);
	failures++;
    }

    return failures;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    return check_affine_planar_events() + check_planar_ruled_events() +
	check_planar_parallel_ruled_events() +
	check_planar_oblique_tapered_ruled_events() +
	check_planar_revolution_events() +
	check_planar_elliptical_revolution_events() +
	check_parallel_planar_torus_events() +
	check_coaxial_elliptical_revolution_events(false) +
	check_coaxial_elliptical_revolution_events(true) +
	check_sphere_cylinder_events() +
	check_coincident_cylinder_events() +
	check_closed_curve_surface_overlap() + check_periodic_cylinder_budget() +
	check_nested_overlap_events() + check_offset_gap_overlap_events();
}
