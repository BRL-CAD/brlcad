/*                 O P E N N U R B S P R I M I T I V E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

/** @file step/OpenNurbsPrimitives.cpp
 *
 * Exact OpenNURBS construction for analytic STEP curves, surfaces, swept
 * surfaces, and vertex loops.
 */

#include "common.h"

#include "brep/defines.h"
#include "sdai.h"
class SDAI_Application_instance;
#include "brep.h"
#include "nmg.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "STEPWrapper.h"
#include "Axis1Placement.h"
#include "CartesianPoint.h"
#include "LocalUnits.h"
#include "Point.h"
#include "Vector.h"
#include "Plane.h"
#include "CylindricalSurface.h"
#include "ConicalSurface.h"
#include "Circle.h"
#include "Ellipse.h"
#include "Hyperbola.h"
#include "Parabola.h"
#include "Line.h"
#include "SweptSurface.h"
#include "SurfaceOfLinearExtrusion.h"
#include "SurfaceOfRevolution.h"
#include "SphericalSurface.h"
#include "ToroidalSurface.h"
#include "Vertex.h"
#include "VertexLoop.h"

bool
Plane::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	ON_PlaneSurface *s = dynamic_cast<ON_PlaneSurface *>(brep->m_S[GetONId()]);

	if (s && trim_curve_3d_bbox) {
	    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();

	    // origin may not lie within face so include in extent
	    double maxdist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_max);
	    double mindist = s->m_plane.origin.DistanceTo(trim_curve_3d_bbox->m_min);
	    bbdiag += FMAX(maxdist, mindist);

	    ON_Interval extents(-bbdiag, bbdiag);
	    s->Extend(0,extents);
	    s->Extend(1,extents);
	}

	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    ON_PlaneSurface *s = new ON_PlaneSurface(p);

    if (!trim_curve_3d_bbox) {
	delete s;
	return false;
    }
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    // origin may not lie within face so include in extent
    double maxdist = origin.DistanceTo(trim_curve_3d_bbox->m_max);
    double mindist = origin.DistanceTo(trim_curve_3d_bbox->m_min);
    bbdiag += FMAX(maxdist, mindist);

    //TODO: look into line curves that are just point and direction
    ON_Interval extents(-bbdiag, bbdiag);
    s->SetExtents(0, extents);
    s->SetExtents(1, extents);
    s->SetDomain(0, 0.0, 1.0);
    s->SetDomain(1, 0.0, 1.0);

    SetONId(brep->AddSurface(s));

    return true;
}


bool
CylindricalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // new surface if reused because of bounding
    //if (ON_id >= 0)
    //	return true; // already loaded

    if ((false) && (brep->m_S.Count() == 56)) {
	std::cerr << "LoadONBrep for CylindricalSurface: " << 55 << std::endl;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // make sure origin is part of the bbox
    trim_curve_3d_bbox->Set(origin, true);

    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    origin = origin - bbdiag * norm;
    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    //ON_Cylinder cyl(c, ON_DBL_MAX);
    ON_Cylinder cyl(c, 2.0 * bbdiag);

    ON_RevSurface *s = cyl.RevSurfaceForm();
    if (s) {
	double r = fabs(cyl.circle.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	s->SetDomain(0, 0.0, 2.0 * ON_PI * r);
    }
    SetONId(brep->AddSurface(s));

    return true;
}


bool
ConicalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;    // already loaded
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector norm(GetNormal());

    origin = origin * LocalUnits::length;
    if (!xaxis.Unitize() || !norm.Unitize() || !trim_curve_3d_bbox ||
	    !trim_curve_3d_bbox->IsValid())
	return false;

    const double angle = semi_angle * LocalUnits::planeangle;
    const double tan_semi_angle = tan(angle);
    const double local_radius = radius * LocalUnits::length;

    if (step && step->Verbose())
	std::cerr << "CONICAL_SURFACE #" << id << ": radius=" << radius
	    << " length-factor=" << LocalUnits::length << " semi-angle="
	    << semi_angle << " angle-factor=" << LocalUnits::planeangle
	    << " radians=" << angle << std::endl;

    /* STEP's conical_surface parameter extends through the apex onto the
     * opposite nappe.  ON_Cone::RevSurfaceForm() only constructs one finite
     * nappe, which makes valid trims beyond the apex impossible to project.
     * Revolving the complete generator line reproduces the STEP locus
     * exactly.  The trim box only bounds the evaluation domain; it does not
     * alter the cone angle, radius, or imported topology. */
    ON_3dPoint profile_origin = origin;
    const bool cylindrical_limit = fabs(tan_semi_angle) <= ON_SQRT_EPSILON;
    if (!cylindrical_limit)
	profile_origin = origin - (local_radius / tan_semi_angle) * norm;

    const double dx = std::max(fabs(trim_curve_3d_bbox->m_min.x - profile_origin.x),
	    fabs(trim_curve_3d_bbox->m_max.x - profile_origin.x));
    const double dy = std::max(fabs(trim_curve_3d_bbox->m_min.y - profile_origin.y),
	    fabs(trim_curve_3d_bbox->m_max.y - profile_origin.y));
    const double dz = std::max(fabs(trim_curve_3d_bbox->m_min.z - profile_origin.z),
	    fabs(trim_curve_3d_bbox->m_max.z - profile_origin.z));
    double reach = sqrt(dx * dx + dy * dy + dz * dz);
    reach = std::max(reach, fabs(local_radius));
    if (!(reach > ON_SQRT_EPSILON) || !ON_IsValid(reach))
	return false;
    reach *= 1.01; /* Domain margin only; the analytic surface is unchanged. */

    ON_3dPoint profile_start;
    ON_3dPoint profile_end;
    double profile_parameter_start = -reach;
    double profile_parameter_end = reach;
    if (cylindrical_limit) {
	profile_start = profile_origin - reach * norm + local_radius * xaxis;
	profile_end = profile_origin + reach * norm + local_radius * xaxis;
    } else {
	/* Preserve both STEP cone nappes when the source trims actually cross
	 * the apex.  For a face whose complete trim bounds lie on one nappe,
	 * retaining the unused opposite nappe makes the apex an interior surface
	 * parameter.  openNURBS can then no longer identify the exact singular
	 * side needed to represent a one-circle cone face as a boundary plus a
	 * paired seam/pole cut.  Bound the generated analytic surface at the apex
	 * in that one-sided case; this changes only its evaluation domain, not its
	 * locus, angle, radius, or STEP topology. */
	double minimum_axial = curve_axis_bounds_valid ?
	    curve_axis_minimum : DBL_MAX;
	double maximum_axial = curve_axis_bounds_valid ?
	    curve_axis_maximum : -DBL_MAX;
	if (!curve_axis_bounds_valid) {
	    /* The AABB projection is conservative and may be inconclusive for an
	     * oblique boundary, but remains the safe fallback for a curve type
	     * whose NURBS control hull could not be proven positive-weight. */
	    for (int xside = 0; xside < 2; ++xside) {
		for (int yside = 0; yside < 2; ++yside) {
		    for (int zside = 0; zside < 2; ++zside) {
			const ON_3dPoint corner(
			    xside ? trim_curve_3d_bbox->m_max.x :
				trim_curve_3d_bbox->m_min.x,
			    yside ? trim_curve_3d_bbox->m_max.y :
				trim_curve_3d_bbox->m_min.y,
			    zside ? trim_curve_3d_bbox->m_max.z :
				trim_curve_3d_bbox->m_min.z);
			const double axial = (corner - profile_origin) * norm;
			minimum_axial = std::min(minimum_axial, axial);
			maximum_axial = std::max(maximum_axial, axial);
		    }
		}
	    }
	}
	const double apex_tolerance = std::max(LocalUnits::tolerance,
	    reach * ON_EPSILON);
	if (minimum_axial >= -apex_tolerance) {
	    profile_parameter_start = 0.0;
	    profile_start = profile_origin;
	    profile_end = profile_origin + reach * norm +
		(reach * tan_semi_angle) * xaxis;
	} else if (maximum_axial <= apex_tolerance) {
	    profile_parameter_end = 0.0;
	    profile_start = profile_origin - reach * norm -
		(reach * tan_semi_angle) * xaxis;
	    profile_end = profile_origin;
	} else {
	    profile_start = profile_origin - reach * norm -
		(reach * tan_semi_angle) * xaxis;
	    profile_end = profile_origin + reach * norm +
		(reach * tan_semi_angle) * xaxis;
	}
    }

    ON_LineCurve *profile = new ON_LineCurve(profile_start, profile_end);
    profile->SetDomain(profile_parameter_start, profile_parameter_end);
    ON_RevSurface *surface = ON_RevSurface::New();
    surface->m_curve = profile;
    surface->m_axis = ON_Line(profile_origin, profile_origin + norm);
    surface->SetAngleRadians(0.0, ON_2PI);
    surface->m_t.Set(0.0, ON_2PI);
    surface->m_bTransposed = false;
    surface->BoundingBox();
    if (!surface->IsValid()) {
	delete surface;
	return false;
    }

    SetONId(brep->AddSurface(surface));
    if (GetONId() < 0) {
	delete surface;
	return false;
    }

    return true;
}


int
intersectLines(const ON_Line &l1, const ON_Line &l2, ON_3dPoint &out)
{
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = BN_TOL_DIST;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    point_t l1_from, l2_from;
    VMOVE(l1_from, l1.from);
    VMOVE(l2_from, l2.from);

    ON_3dVector d;
    vect_t l1_dir, l2_dir;

    d = l1.Direction();
    VMOVE(l1_dir, d);
    d = l2.Direction();
    VMOVE(l2_dir, d);

    fastf_t l1_dist, l2_dist;
    int i = bg_isect_line3_line3(&l1_dist, &l2_dist, l1_from, l1_dir,
				 l2_from, l2_dir, &tol);
    if (i == 1) {
	ON_3dVector l1_unit_dir = l1.Direction();
	l1_unit_dir.Unitize();

	out = l1.from + l1_unit_dir * l1_dist;
    }
    return i;
}


void
Circle::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    ON_3dPoint P = c.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;


    P = c.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

#define ANGLE_ZERO_TOL 1.0e-6

static double
simplify_angle(double rad)
{
    double result;

    result = fmod(rad, 2.0 * ON_PI);

    if (NEAR_ZERO(result, ANGLE_ZERO_TOL)) {
	result = 0.0;
    } else if (result < 0.0) {
	result += 2.0 * ON_PI;
    }

    return result;
}

static double
radians_from_xaxis_to_ellipse_point(Conic *conic, ON_3dPoint p, double a = 1.0, double b = 1.0)
{
    ON_3dPoint origin(conic->GetOrigin());
    ON_3dVector xaxis(conic->GetXAxis());
    ON_3dVector yaxis(conic->GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    // get p after translating to origin
    ON_3dPoint canonical_p = p - origin;

    // decompose into x and y components
    double x = canonical_p * xaxis;
    double y = canonical_p * yaxis;

    // ellipse scaling
    x /= a;
    y /= b;

    return atan2(y, x);
}

bool
Circle::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    if ((false) && ((brep->m_C3.Count() == 3) || (id == 1723))) {
	std::cerr << "Debug:LoadONBrep for Circle:ID:" << id << std::endl;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double r = radius * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle circle(plane, origin, r);

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt);
	s = radians_from_xaxis_to_ellipse_point(this, endpt);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = circle.PointAt(t);
	endpt = circle.PointAt(s);
    }

    double theta = s - t;

    /* Geometrically close but topologically distinct vertices can define a
     * short arc across the conic's parameter seam.  A fixed-distance test
     * used to turn those arcs into complete circles, discarding authoritative
     * STEP topology (DRONE #929223).  Only a one-vertex EDGE_CURVE, or an
     * explicitly trimmed conic without topology vertices, means a complete
     * revolution. */
    if ((start && end && start == end) ||
	    ((!start || !end) && VNEAR_EQUAL(startpt, endpt,
		LocalUnits::tolerance))) {
	theta = 2.0 * ON_PI;
    } else if (start && end && start != end &&
	    VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST) && step) {
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    id, "CIRCLE", "edge_vertices",
	    "preserved a short conic arc between distinct STEP topology vertices");
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }

    double dtheta = theta / narcs;
    double w = cos(dtheta / 2.0);
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; /* 2 * max narcs + 1 */
    ON_3dPoint circleP1, isect, circleP2;
    ON_3dVector tangentP1, tangentP2;

    circleP1 = circle.PointAt(angle); // was using 'startpt' from edge_curve but found case where not in tol
    tangentP1 = circle.TangentAt(angle);

    for (int i = 0; i < narcs; i++) {
	angle = angle + dtheta;

	circleP2 = circle.PointAt(angle);
	tangentP2 = circle.TangentAt(angle);
	ON_Line tangent1(circleP1, circleP1 + r * tangentP1);
	ON_Line tangent2(circleP2, circleP2 + r * tangentP2);

	if (intersectLines(tangent1, tangent2, isect) != 1) {
	    /* Tangents of a very short exact arc are numerically parallel even
	     * though the arc itself is well conditioned.  Preserve the same
	     * analytic circle interval instead of rejecting it or inflating it
	     * into a larger approximation. */
	    const ON_Arc exact_arc(circle, ON_Interval(t, s));
	    std::unique_ptr<ON_ArcCurve> exact_curve(exact_arc.IsValid() ?
		new ON_ArcCurve(exact_arc, t, s) : NULL);
	    if (exact_curve && exact_curve->IsValid()) {
		SetONId(brep->AddEdgeCurve(exact_curve.release()));
		if (step) {
		    step->RecordDiagnostic(
			brlcad::step::DiagnosticSeverity::Information, id,
			"CIRCLE", "edge_geometry",
			"used an exact analytic arc when the equivalent short "
			"rational construction was numerically ill-conditioned");
		}
		return GetONId() >= 0;
	    }
	    if (step && step->Verbose())
		std::cerr << entityname << " #" << id
		    << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}

	cpts.Append(circleP1);

	isect *= w; // must pre-weight before putting into NURB
	cpts.Append(isect);

	W[2 * i] = 1.0;
	W[2 * i + 1] = w;

	circleP1 = circleP2;
	tangentP1 = tangentP2;
    }
    cpts.Append(circle.PointAt(s));
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    SetONId(brep->AddEdgeCurve(c));

    return true;
}

void
Ellipse::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    ON_Ellipse ellipse(plane, a, b);

    ON_3dPoint P = ellipse.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    P = ellipse.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

bool
Ellipse::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane plane(origin, xaxis, yaxis);

    ON_3dPoint center = origin;
    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Ellipse ellipse(plane, a, b);

    // double eccentricity = sqrt(1.0 - (b * b) / (a * a));
    // ON_3dPoint focus_1 = center + (eccentricity * a) * xaxis;
    // ON_3dPoint focus_2 = center - (eccentricity * a) * xaxis;

    ON_3dPoint startpt;
    ON_3dPoint endpt;

    // get explicit start and end points
    if (trimmed) {
	if (!parameter_trim) {
	    startpt = trim_startpoint;
	    endpt = trim_endpoint;
	}
    } else if (start && end) {
	startpt = start->Point3d();
	endpt = end->Point3d();

	startpt *= LocalUnits::length;
	endpt *= LocalUnits::length;
    }

    // if we have start and end points, get corresponding t and s
    if ((trimmed && !parameter_trim) || (start && end)) {
	t = radians_from_xaxis_to_ellipse_point(this, startpt, a, b);
	s = radians_from_xaxis_to_ellipse_point(this, endpt, a, b);
    }

    t = simplify_angle(t);
    s = simplify_angle(s);

    if (NEAR_ZERO(s, ANGLE_ZERO_TOL)) {
	s = 2.0 * ON_PI;
    }

    while (s < t) {
	s += 2.0 * ON_PI;
    }

    // if we have only t and s, get corresponding start and end points
    if (parameter_trim) {
	startpt = ellipse.PointAt(t);
	endpt = ellipse.PointAt(s);
    }

    double theta = s - t;

    if ((start && end && start == end) ||
	    ((!start || !end) && VNEAR_EQUAL(startpt, endpt,
		LocalUnits::tolerance))) {
	theta = 2.0 * ON_PI;
    } else if (start && end && start != end &&
	    VNEAR_EQUAL(startpt, endpt, BN_TOL_DIST) && step) {
	step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Information,
	    id, "ELLIPSE", "edge_vertices",
	    "preserved a short conic arc between distinct STEP topology vertices");
    }

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }
    double dtheta = theta / narcs;
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; // 2 * max narcs + 1
    ON_3dPoint Pnt[2 * 4 + 1];
    ON_3dVector Tangent1, Tangent2;
    ON_3dPoint P0, PX, P2, P1;

    P0 = ellipse.PointAt(angle);
    for (int i = 0; i < narcs; i++) {
	Tangent1 = ellipse.TangentAt(angle);
	PX = ellipse.PointAt(angle + dtheta / 2.0);
	// step angle
	angle = angle + dtheta;
	P2 = ellipse.PointAt(angle);
	Tangent2 = ellipse.TangentAt(angle);
	ON_Line tangent1(P0, P0 + Tangent1);
	ON_Line tangent2(P2, P2 + Tangent2);
	if (intersectLines(tangent1, tangent2, P1) != 1) {
	    std::cerr << entityname << " #" << id
		<< ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	ON_Line l1(P1, center);
	ON_Line l2(P0, P2);
	ON_3dPoint PM;
	if (intersectLines(l1, l2, PM) != 1) {
	    std::cerr << entityname << " #" << id
		<< ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	double mx = PM.DistanceTo(PX);
	double mp1 = PM.DistanceTo(P1);
	double R = mx / mp1;
	double w = R / (1 - R);

	cpts.Append(P0);
	Pnt[2 * i] = P0;
	W[2 * i] = 1.0;

	Pnt[2 * i + 1] = P1;
	P1 = (w) * P1; // must pre-weight before putting into NURB
	cpts.Append(P1);
	W[2 * i + 1] = w;

	P0 = P2;
    }
    cpts.Append(P2);
    Pnt[2 * narcs] = P2;
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double)k) / narcs;
    }

    ON_NurbsCurve *c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    SetONId(brep->AddEdgeCurve(c));

    return true;
}

void
Hyperbola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    // ON_3dVector norm(GetNormal());

    ON_3dPoint center = origin;
    double a = semi_axis;
    double b = semi_imag_axis;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }

    double y = b * tan(t);
    double x = a / cos(t);

    ON_3dVector X = x * xaxis;
    ON_3dVector Y = y * yaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    y = b * tan(s);
    x = a / cos(s);

    X = x * xaxis;
    Y = y * yaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Hyperbola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    norm.Unitize();
    xaxis.Unitize();
    yaxis.Unitize();

    ON_3dPoint center = origin * LocalUnits::length;
    double a = semi_axis * LocalUnits::length;
    double b = semi_imag_axis * LocalUnits::length;

    double eccentricity = sqrt(1.0 + (b * b) / (a * a));
    ON_3dPoint focus = center + (eccentricity * a) * xaxis;
    ON_3dPoint focusprime = center - (eccentricity * a) * xaxis;
    // ON_3dPoint vertex = center + a * xaxis;
    // ON_3dPoint directrix = center + (a / eccentricity) * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) {
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: " << entityname << " #" << id
	    << " has no endpoints for LoadONBrep(ON_Brep *brep<" << std::hex
	    << brep << std::dec << ">)" << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 =  pnt2 * LocalUnits::length;

    // calc tangent P0, P2 intersection
    ON_3dVector ToFocus = focus - P0;
    ToFocus.Unitize();

    ON_3dVector ToFocusPrime = focusprime - P0;
    ToFocusPrime.Unitize();

    ON_3dVector bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs0(P0, P0 + bisector);

    ToFocus = focus - P2;
    ToFocus.Unitize();

    ToFocusPrime = focusprime - P2;
    ToFocusPrime.Unitize();

    bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs2(P2, P2 + bisector);
    ON_3dPoint P1;
    if (intersectLines(bs0, bs2, P1) != 1) {
	std::cerr << entityname << " #" << id
	    << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    ON_Line l1(focus, P1);
    ON_Line l2(P0, P2);
    ON_3dPoint M = (P0 + P2) / 2.0;

    ON_Line ctom(center, M);
    ON_3dVector dtom = ctom.Direction();
    dtom.Unitize();
    double m = (dtom * yaxis) / (dtom * xaxis);
    double x1 = a * b * sqrt(1.0 / (b * b - m * m * a * a));
    double y1 = b * sqrt((x1 * x1) / (a * a) - 1.0);
    if (m < 0.0) {
	y1 *= -1.0;
    }
    ON_3dVector X = x1 * xaxis;
    ON_3dVector Y = y1 * yaxis;
    ON_3dPoint Pv = center + X + Y;
    double mx = M.DistanceTo(Pv);
    double mp1 = M.DistanceTo(P1);
    double R = mx / mp1;
    double w = R / (1 - R);

    P1 = (w) * P1; // must pre-weight before putting into NURB

    // add hyperbola weightings
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve *hypernurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*hypernurbscurve);

    SetONId(brep->AddEdgeCurve(hypernurbscurve));

    return true;
}


void
Parabola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    xaxis.Unitize();
    ON_3dVector yaxis(GetYAxis());
    yaxis.Unitize();

    ON_3dPoint center = origin;
    double fd = focal_dist;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    double x = 2.0 * fd * t; // tan(t);
    double y = (x * x) / (4 * fd);

    ON_3dVector X = x * yaxis;
    ON_3dVector Y = y * xaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    x = 2.0 * fd * s; //tan(s);
    y = (x * x) / (4 * fd);

    X = x * yaxis;
    Y = y * xaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Parabola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    // ON_3dVector yaxis(GetYAxis());
    // ON_3dVector normal(GetNormal());

    ON_3dPoint center = origin * LocalUnits::length;
    double fd = focal_dist * LocalUnits::length;
    ON_3dPoint focus = center + fd * xaxis;
    // ON_3dPoint directrix = center - fd * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: " << entityname << " #" << id
	    << " has no endpoints for LoadONBrep(ON_Brep *brep<" << std::hex
	    << brep << std::dec << ">)" << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 = pnt2 * LocalUnits::length;

    // calc tang intersect with transverse axis
    ON_3dVector ToFocus;

    ToFocus = P0 - focus;
    ToFocus.Unitize();
    ON_3dVector bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent1(P0, P0 + 10.0 * bisector);

    ToFocus = P2 - focus;
    ToFocus.Unitize();
    bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent2(P2, P2 + 10.0 * bisector);

    ON_3dPoint P1;
    if (intersectLines(tangent1, tangent2, P1) != 1) {
	std::cerr << entityname << " #" << id
	    << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    // make parabola from bezier
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();

    ON_NurbsCurve *parabnurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*parabnurbscurve);
    SetONId(brep->AddEdgeCurve(parabnurbscurve));

    return true;
}


void
Line::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint ptstart(pnt->Point3d());
    ON_3dVector vdir( dir->Orientation());
    ON_3dPoint ptend = ptstart + (vdir*dir->Magnitude());
    ON_Line l(ptstart, ptend);

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    VMOVE(startpoint,l.PointAt(t));
    VMOVE(endpoint,l.PointAt(s));

    SetPointTrim(startpoint, endpoint);
}


bool
Line::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint startpnt = ON_3dPoint::UnsetPoint;
    ON_3dPoint endpnt = ON_3dPoint::UnsetPoint;

    if (trimmed) { //explicitly trimmed
	startpnt = trim_startpoint;
	endpnt = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	startpnt = start->Point3d();
	endpnt = end->Point3d();
    } else {
	std::cerr << "Error: " << entityname << " #" << id
	    << " has no endpoints for LoadONBrep(ON_Brep *brep<" << std::hex
	    << brep << std::dec << ">)" << std::endl;
	return false;
    }

    startpnt = startpnt * LocalUnits::length;
    endpnt = endpnt * LocalUnits::length;

    ON_LineCurve *l = new ON_LineCurve(startpnt, endpnt);

    SetONId(brep->AddEdgeCurve(l));

    return true;
}


bool
SurfaceOfLinearExtrusion::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << " #" << id
	    << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_Curve *curve = brep->m_C3[swept_curve->GetONId()];

    // use trimming edge bounding box unioned with bounding box of
    // curve being extruded; calc diagonal to make sure extrusion
    // magnitude is well represented
    ON_BoundingBox curvebb = curve->BoundingBox();
    trim_curve_3d_bbox->Union(curvebb);
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length(); // already converted to local units;

    ON_3dPoint dir(extrusion_axis->Orientation());
    double mag = extrusion_axis->Magnitude() * LocalUnits::length;
    mag = FMAX(mag, bbdiag);

    ON_3dPoint startpnt;
    if (swept_curve->PointAtStart() == NULL) {
	startpnt = curve->PointAtStart();
    } else {
	startpnt = swept_curve->PointAtStart();
	startpnt = startpnt * LocalUnits::length;
    }

    // add a little buffer in the surface extrusion distance
    // by extruding "+/-mag" distance along "dir"
    ON_3dPoint extrusion_endpnt = startpnt + mag * dir;
    ON_3dPoint extrusion_startpnt = startpnt - mag * dir;

    ON_LineCurve *l = new ON_LineCurve(extrusion_startpnt, extrusion_endpnt);

    // the following extrude code lifted from OpenNURBS ON_BrepExtrude()
    ON_Line path_line;
    path_line.from = extrusion_startpnt;
    path_line.to = extrusion_endpnt;
    ON_3dVector path_vector = path_line.Direction();
    if (path_vector.IsZero()) {
	delete l;
	return false;
    }

    ON_SumSurface *sum_srf = 0;

    ON_Curve *srf_base_curve = curve->Duplicate();
    srf_base_curve->Translate(-mag * dir);

    ON_3dPoint sum_basepoint = ON_origin - l->PointAtStart();
    sum_srf = new ON_SumSurface();

    if (!sum_srf) {
	delete l;
	return false;
    }
    sum_srf->m_curve[0] = srf_base_curve;
    sum_srf->m_curve[1] = l; //srf_path_curve;
    sum_srf->m_basepoint = sum_basepoint;
    sum_srf->BoundingBox(); // fills in sum_srf->m_bbox


    SetONId(brep->AddSurface(sum_srf));

    return true;
}


bool
SurfaceOfRevolution::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;    // already loaded
    }

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << " #" << id
	    << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_3dPoint start(axis_position->GetOrigin());
    start = start * LocalUnits::length;

    ON_3dVector dir(axis_position->GetNormal());
    ON_3dPoint end = start + dir;

    ON_Line axisline(start, end);
    ON_RevSurface *revsurf = ON_RevSurface::New();

    if (!revsurf) {
	return false;
    }
    revsurf->m_curve = brep->m_C3[swept_curve->GetONId()]->DuplicateCurve();
    revsurf->m_axis = axisline;
    revsurf->BoundingBox(); // fills in sum_srf->m_bbox

    //ON_Brep* b = ON_BrepRevSurface(revsurf, true, true);

    //if (!revsurf)
    //return false;

    SetONId(brep->AddSurface(revsurf));

    return true;
}


bool
SphericalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // get sphere center
    ON_3dPoint center(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    center = center * LocalUnits::length;
    if (!xaxis.Unitize() || !yaxis.Unitize())
	return false;

    // Creates a sphere with given center and radius.
    ON_Sphere sphere(center, radius * LocalUnits::length);
    sphere.plane = ON_Plane(center, xaxis, yaxis);
    if (!sphere.IsValid())
	return false;

    ON_RevSurface *s = sphere.RevSurfaceForm(false, nullptr);
    if (s) {
	double r = fabs(sphere.radius);
	if (r <= ON_SQRT_EPSILON) {
	    r = 1.0;
	}
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	s->SetDomain(1, -r, r);
    }
    SetONId(brep->AddSurface(s));

    return true;
}


bool
ToroidalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_3dPoint origin(GetOrigin());
    ON_3dVector xaxis(GetXAxis());
    ON_3dVector yaxis(GetYAxis());
    ON_3dVector norm(GetNormal());

    if (!xaxis.Unitize() || !yaxis.Unitize() || !norm.Unitize())
	return false;

    origin = origin * LocalUnits::length;

    ON_Plane p(origin, xaxis, yaxis);

    const double source_major = major_radius * LocalUnits::length;
    const double source_minor = minor_radius * LocalUnits::length;
    double major = source_major;
    double minor = source_minor;
    const auto normalize_positive_radius = [this](double source,
	const char *attribute, double *radius) {
	if (!radius || source > 0.0)
	    return radius != NULL;
	const bool safe_repair = step && !step->ImportOptions().exact &&
	    step->ImportOptions().repair == brlcad::step::RepairMode::Safe;
	if (!(source < 0.0) || !safe_repair) {
	    if (step) {
		std::ostringstream message;
		message << attribute << "=" << source
		    << " violates the AP positive_length_measure constraint";
		step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Error,
		    id, "TOROIDAL_SURFACE", attribute, message.str());
	    }
	    return false;
	}
	/* Changing the sign of either torus radius is an exact
	 * reparameterization of the complete torus locus.  Some exporters use a
	 * negative major radius to select the other sheet of a spindle torus,
	 * despite the AP schema's positive_length_measure declaration.  Its raw
	 * parameterization is discontinuous at otherwise shared topology edges;
	 * use the schema-valid magnitude in safe mode and let the completed-shell
	 * orientation checks establish the face sense. */
	*radius = fabs(source);
	if (step) {
	    std::ostringstream message;
	    message << attribute << "=" << source
		<< " violates positive_length_measure; using the exact, "
		   "schema-valid radius magnitude " << *radius;
	    step->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
		id, "TOROIDAL_SURFACE", attribute, message.str());
	    step->RecordRepair(id, "TOROIDAL_SURFACE", attribute,
		"normalized a negative torus radius by exact locus-preserving reparameterization");
	}
	return true;
    };
    if (!normalize_positive_radius(source_major, "major_radius", &major) ||
	    !normalize_positive_radius(source_minor, "minor_radius", &minor))
	return false;
    ON_Surface *surface = NULL;

    // Creates a torus parallel to the plane
    // with given major and minor radius.
    if (fabs(major) > fabs(minor)) {
	    ON_Torus torus(p, major, minor);
	    ON_RevSurface *revolution = torus.RevSurfaceForm();
	    if (revolution) {
		double r = fabs(torus.major_radius);
		if (r <= ON_SQRT_EPSILON) r = 1.0;
		revolution->SetDomain(0, 0.0, 2.0 * ON_PI * r);
		r = fabs(torus.minor_radius);
		if (r <= ON_SQRT_EPSILON) r = 1.0;
		revolution->SetDomain(1, 0.0, 2.0 * ON_PI * r);
		surface = revolution;
	    }
    } else {
	    /* ON_Torus intentionally rejects horn and spindle tori.  STEP permits
	     * them, and the same locus is represented exactly by revolving a
	     * rational NURBS circle whose center is major_radius from the axis.
	     * This retains the self-intersection instead of dropping the face or
	     * substituting approximate geometry. */
	    const ON_3dPoint profile_center = origin + major * xaxis;
	    ON_Plane profile_plane(profile_center, xaxis, norm);
	    ON_Circle profile_circle(profile_plane, fabs(minor));
	    ON_NurbsCurve *profile = ON_NurbsCurve::New();
	    if (!profile_circle.IsValid() || !profile ||
		    !profile_circle.GetNurbForm(*profile)) {
		delete profile;
		return false;
	    }
	    double profile_scale = fabs(minor);
	    if (profile_scale <= ON_SQRT_EPSILON) profile_scale = 1.0;
	    profile->SetDomain(0.0, 2.0 * ON_PI * profile_scale);

	    ON_RevSurface *revolution = ON_RevSurface::New();
	    revolution->m_curve = profile;
	    revolution->m_axis = ON_Line(origin, origin + norm);
	    revolution->m_angle = ON_Interval(0.0, 2.0 * ON_PI);
	    double revolution_scale = fabs(major);
	    if (revolution_scale <= ON_SQRT_EPSILON) revolution_scale = 1.0;
	    revolution->m_t = ON_Interval(0.0,
		2.0 * ON_PI * revolution_scale);
	    revolution->m_bTransposed = false;
	    revolution->BoundingBox();
	    if (!revolution->IsValid()) {
		delete revolution;
		return false;
	    }
	    surface = revolution;
    }

    if (!surface)
	return false;
    SetONId(brep->AddSurface(surface));
    if (GetONId() < 0) {
	delete surface;
	return false;
    }

    return true;
}


bool
VertexLoop::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //load vertex
    loop_vertex->LoadONBrep(brep);

    ON_3dPoint vertex(loop_vertex->Point3d());

    // create singular trim;
    ON_BrepLoop &loop = brep->m_L[ON_loop_index];
    ON_BrepFace *face = loop.Face();
    const ON_Surface *surface = face->SurfaceOf();

    ON_Interval U = surface->Domain(0);
    ON_Interval V = surface->Domain(1);
    ON_3dPoint corner[4];
    corner[0] = surface->PointAt(U.m_t[0], V.m_t[0]);
    corner[1] = surface->PointAt(U.m_t[1], V.m_t[0]);
    corner[2] = surface->PointAt(U.m_t[0], V.m_t[1]);
    corner[3] = surface->PointAt(U.m_t[1], V.m_t[1]);

    ON_2dPoint start, end;
    ON_Surface::ISO iso = ON_Surface::N_iso;
    if (VNEAR_EQUAL(vertex, corner[0], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	    //south;
	    end = ON_2dPoint(U.m_t[1], V.m_t[0]);
	    iso = ON_Surface::S_iso;
	} else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	    //west
	    end = ON_2dPoint(U.m_t[0], V.m_t[1]);
	    iso = ON_Surface::W_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[1], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[1], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //east
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::E_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[2], LocalUnits::tolerance)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[1]);
	if (VNEAR_EQUAL(vertex, corner[3], LocalUnits::tolerance)) {
	    //north
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::N_iso;
	}
    }
    ON_Curve *c2d = new ON_LineCurve(start, end);
    int trimCurve = brep->m_C2.Count();
    brep->m_C2.Append(c2d);

    ON_BrepTrim &trim = brep->NewSingularTrim(
	brep->m_V[loop_vertex->GetONId()], loop, iso, trimCurve);
    trim.m_tolerance[0] = LocalUnits::tolerance;
    trim.m_tolerance[1] = LocalUnits::tolerance;

    return true;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
