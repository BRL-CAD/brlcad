/*                  I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file intersect.cpp
 *
 * Implementation of intersection routines openNURBS left out.
 *
 */

#include "common.h"

#include <assert.h>
#include <vector>
#include <algorithm>
#include "bio.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"

// Whether to output the debug messages about b-rep intersections.
#define DEBUG_BREP_INTERSECT 0


ON_Curve *
sub_curve(const ON_Curve *in, double a, double b)
{
    // approach: call ON_Curve::Split() twice with a and b respectively.
    // [min, max] -> [min, a] & [a, max]
    // [a, max] -> [a, b] & [b, max]

    ON_Interval dom = in->Domain();
    ON_Interval sub(a, b);
    sub.MakeIncreasing();
    if (!sub.Intersection(dom)) {
	return NULL;
    }
    ON_Curve *left = NULL, *right = NULL, *three = NULL;

    in->Split(sub.m_t[0], left, right);
    if (left) {
	delete left;
    }
    left = NULL;
    if (!right) {
	right = in->Duplicate();
    }

    right->Split(sub.m_t[1], left, three);
    if (!left) {
	left = right->Duplicate();
    }

    if (right) {
	delete right;
    }
    if (three) {
	delete three;
    }
    return left;
}


ON_Surface *
sub_surface(const ON_Surface *in, int dir, double a, double b)
{
    // approach: call ON_Surface::Split() twice with a and b respectively.
    // [min, max] -> [min, a] & [a, max]
    // [a, max] -> [a, b] & [b, max]

    ON_Interval dom = in->Domain(dir);
    ON_Interval sub(a, b);
    sub.MakeIncreasing();
    if (!sub.Intersection(dom)) {
	return NULL;
    }
    ON_Surface *left = NULL, *right = NULL, *three = NULL;

    in->Split(dir, sub.m_t[0], left, right);
    if (left) {
	delete left;
    }
    left = NULL;
    if (!right) {
	right = in->Duplicate();
    }

    right->Split(dir, sub.m_t[1], left, three);
    if (!left) {
	left = right->Duplicate();
    }

    if (right) {
	delete right;
    }
    if (three) {
	delete three;
    }
    return left;
}


HIDDEN const ON_Interval
check_domain(const ON_Interval *in, const ON_Interval &domain, const char *name)
{
    // Check the validity of the input domain.
    // Returns the intersection of the input interval and the curve/surface's
    // domain.
    // If the input interval is NULL, just returns the whole domain.

    if (in) {
	if (!in->IsIncreasing()) {
	    bu_log("Bogus %s passed in", name);
	    return domain;
	}
	ON_Interval dom;
	dom.Intersection(*in, domain);
	if (dom.IsEmptyInterval()) {
	    bu_log("%s is disjoint from the whole domain.\n", name);
	} else {
	    return dom;
	}
    }
    return domain;
}


HIDDEN bool
build_curve_root(const ON_Curve *curve, const ON_Interval *domain, Subcurve &root)
{
    // Build the curve tree root within a given domain

    if (curve == NULL) {
	return false;
    }

    if (domain == NULL || *domain == curve->Domain()) {
	root.m_curve = curve->Duplicate();
	root.m_t = curve->Domain();
    } else {
	// Call sub_curve() to get the curve segment inside the input domain.
	root.m_curve = sub_curve(curve, domain->Min(), domain->Max());
	root.m_t = *domain;
    }

    if (root.m_curve) {
	root.SetBBox(root.m_curve->BoundingBox());
	root.m_islinear = root.m_curve->IsLinear();
	return true;
    } else {
	return false;
    }
}


HIDDEN bool
build_surface_root(const ON_Surface *surf, const ON_Interval *u_domain, const ON_Interval *v_domain, Subsurface &root)
{
    // Build the surface tree root within a given domain

    if (surf == NULL) {
	return false;
    }

    const ON_Surface *u_splitted_surf; // the surface after u-split

    // First, do u split
    if (u_domain == NULL || *u_domain == surf->Domain(0)) {
	u_splitted_surf = surf;
	root.m_u = surf->Domain(0);
    } else {
	// Call sub_surface() to get the sub-surface inside the input U domain.
	u_splitted_surf = sub_surface(surf, 0, u_domain->Min(), u_domain->Max());
	root.m_u = *u_domain;
    }

    if (u_splitted_surf == NULL) {
	return false;
    }

    // Then v split
    if (v_domain == NULL || *v_domain == surf->Domain(1)) {
	root.m_surf = u_splitted_surf->Duplicate();
	root.m_v = surf->Domain(1);
    } else {
	// // Call sub_surface() to get the sub-surface inside the input V domain.
	root.m_surf = sub_surface(surf, 1, v_domain->Min(), v_domain->Max());
	root.m_v = *v_domain;
    }

    if (root.m_surf) {
	root.SetBBox(root.m_surf->BoundingBox());
	root.m_isplanar = root.m_surf->IsPlanar();
	return true;
    } else {
	return false;
    }
}


HIDDEN ON_Curve *
curve_fitting(ON_Curve *in, double fitting_tolerance = ON_ZERO_TOLERANCE, bool delete_curve = true)
{
    // Fit a *2D* curve into line, arc, ellipse or other comic curves.

    if (in == NULL) {
	return NULL;
    }

    bool changed = false;

    // First, for a polyline, eliminate some unnecessary points (if three
    // neighbor points are collinear, the middle one can be removed)

    ON_3dPointArray points;
    if (in->IsPolyline(&points)) {
	int point_count = points.Count();
	ON_3dPointArray new_points;
	ON_3dPoint start = points[0];
	new_points.Append(start);
	for (int i = 2; i < point_count; i++) {
	    ON_3dVector v1 = points[i - 1] - start;
	    ON_3dVector v2 = points[i] - points[i - 1];
	    if (!ON_NearZero(ON_CrossProduct(v1, v2).Length()) || ON_DotProduct(v1, v2) < -ON_ZERO_TOLERANCE) {
		// start, points[i-1], points[i] are not collinear,
		// or v1 and v2 have opposite directions.
		// No points can be eliminated
		start = points[i - 1];
		new_points.Append(start);
	    }
	}
	new_points.Append(points[point_count - 1]);
	if (new_points.Count() != point_count) {
	    // Some points have been eliminated
	    if (delete_curve) {
		delete in;
	    }
	    in = new ON_PolylineCurve(new_points);
	    changed = true;
	    if (DEBUG_BREP_INTERSECT) {
		bu_log("fitting: %d => %d points.\n", point_count, new_points.Count());
	    }
	}
    }

    // Linear fitting
    if (in->IsLinear(fitting_tolerance)) {
	ON_LineCurve *linecurve = new ON_LineCurve(in->PointAtStart(), in->PointAtEnd());
	linecurve->ChangeDimension(in->Dimension());
	if (delete_curve || changed) {
	    delete in;
	}
	return linecurve;
    }

    // Conic fitting (ellipse, parabola, hyperbola)
    // Now we only have the ellipse fitting

    // It's only meaningful to fit the curve when it's a complex one
    // For a polyline curve, the number of points should not be less than 10.
    const int fit_minimum_knots = 10;
    int knotcnt = in->SpanCount();
    if (knotcnt < fit_minimum_knots) {
	return in;
    }

    double *knots = new double [knotcnt + 1];
    in->GetSpanVector(knots);

    // Sample 6 points along the curve, and call ON_GetConicEquationThrough6Points().
    double conic[6];
    double sample_pts[12];
    int plotres = in->IsClosed() ? 6 : 5;

    // The sample points should be knots (which are accurate if the curve
    // is a polyline curve).
    for (int i = 0; i < 6; i++) {
	double knot_t = knots[ON_Round((double)i / plotres * knotcnt)];
	ON_3dPoint pt3d = in->PointAt(knot_t);
	sample_pts[2 * i] = pt3d.x;
	sample_pts[2 * i + 1] = pt3d.y;
    }

    if (ON_GetConicEquationThrough6Points(2, sample_pts, conic, NULL, NULL, NULL)) {
	// It may be a conic.
	ON_2dPoint ell_center;
	ON_2dVector ell_A, ell_B;
	double ell_a, ell_b;
	// First, fitting an ellipse. It seems that ON_Curve::IsEllipse()
	// doesn't work, so we use conic fitting first. If this is not ideal,
	// an alternative solution is to use least square fitting on all
	// points.
	if (ON_IsConicEquationAnEllipse(conic, ell_center, ell_A, ell_B, &ell_a, &ell_b)) {
	    // The conic equation is an ellipse. But since we only considered
	    // the 6 sampled points, now we need to check whether all the points
	    // are on the ellipse.

	    ON_Plane ell_plane = ON_Plane(ON_3dPoint(ell_center), ON_3dVector(ell_A), ON_3dVector(ell_B));
	    ON_Ellipse ell(ell_plane, ell_a, ell_b);
	    int i;
	    for (i = 0; i <= knotcnt; i++) {
		ON_3dPoint pt = in->PointAt(knots[i]);
		ON_3dPoint ell_pt;
		ell_pt = ell.ClosestPointTo(pt);
		if (ell_pt.DistanceTo(pt) > fitting_tolerance) {
		    break;
		}
	    }

	    if (i == knotcnt + 1) {
		// All points are on the ellipse
		ON_NurbsCurve nurbscurve;
		ell.GetNurbForm(nurbscurve);

		// It may not be a complete ellipse.
		// We find the domain of its parameter.
		// The params of the nurbscurve is between [0, 2*pi]
		double t_min, t_max;
		if (in->IsClosed()) {
		    t_max = ON_PI * 2.0;
		    t_min = 0.0;
		} else {
		    double t_mid;
		    ell.ClosestPointTo(in->PointAtStart(), &t_min);
		    ell.ClosestPointTo(in->PointAtEnd(), &t_max);
		    ell.ClosestPointTo(in->PointAt(knots[knotcnt / 2]), &t_mid);
		    if (t_min > t_max) {
			std::swap(t_min, t_max);
		    }
		    if (!ON_Interval(t_min, t_max).Includes(t_mid)) {
			// The arc crosses where t = 0 (2*PI).
			// We make the curve "two rounds" of the ellipse.
			t_min += 2.0 * ON_PI;
			std::swap(t_min, t_max);
			nurbscurve.Append(nurbscurve);
		    }
		}

		delete []knots;
		if (delete_curve || changed) {
		    delete in;
		}

		// The domain of the nurbscurve is [0, 4*pi]
		// t_min \in [0, 2*pi], t_max \in [0, 4*pi]
		return sub_curve(&nurbscurve, t_min, t_max);
	    }
	}
    }

    // We have tried all fittings, but none of them succeed.
    // So we just return the original curve.
    delete []knots;
    return in;
}


/**
 * Point-point intersections (PPI)
 *
 * approach:
 *
 * - Calculate the distance of the two points.
 *
 * - If the distance is within the tolerance, the two points
 *   intersect. The mid-point of them is the intersection point,
 *   and half of the distance is the uncertainty radius.
 */


bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_3dPoint &pointB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance)
{
    if (tolerance <= 0.0) {
	tolerance = ON_ZERO_TOLERANCE;
    }

    if (pointA.DistanceTo(pointB) <= tolerance) {
	ON_PX_EVENT Event;
	Event.m_type = ON_PX_EVENT::ppx_point;
	Event.m_A = pointA;
	Event.m_B = pointB;
	Event.m_Mid = (pointA + pointB) * 0.5;
	Event.m_radius = pointA.DistanceTo(pointB) * 0.5;
	x.Append(Event);
	return true;
    }
    return false;
}


/**
 * Point-curve intersections (PCI)
 *
 * approach:
 *
 * - Sub-divide the curve, calculating bounding boxes.
 *
 * - If the bounding box intersects with the point, go deeper until
 *   we reach the maximal depth or the sub-curve is linear.
 *
 * - Use linear approximation to get an estimated intersection point.
 *
 * - Use Newton-Raphson iterations to calculate an accurate intersection
 *   point, with the estimated one as a start.
 */


// The maximal depth for subdivision - trade-off between accuracy and
// performance
#define MAX_PCI_DEPTH 8


// We make the default tolerance for PSI the same as that of curve-curve
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define PCI_DEFAULT_TOLERANCE 0.001


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define PCI_MAX_ITERATIONS 100


HIDDEN bool
newton_pci(double &t, const ON_3dPoint &pointA, const ON_Curve &curveB, double tolerance)
{
    ON_3dPoint closest_point = curveB.PointAt(t);
    double dis = closest_point.DistanceTo(pointA);

    // Use Newton-Raphson method with the starting point from linear
    // approximation.
    // It's similar to point projection (or point inversion, or get
    // closest point on curve). We calculate the root of:
    //       (curve(t) - pointA) \dot tangent(t) = 0 (*)
    // Then curve(t) may be the closest point on the curve to pointA.
    //
    // In some cases, the closest point (intersection point) is the
    // endpoints of the curve, it does not satisfy (*), but that's
    // OK because it already comes out with the linear approximation
    // above.

    double last_t = DBL_MAX;
    int iterations = 0;
    while (!ON_NearZero(last_t - t)
	   && iterations++ < PCI_MAX_ITERATIONS) {
	ON_3dVector tangent, s_deriv;
	last_t = t;
	curveB.Ev2Der(t, closest_point, tangent, s_deriv);
	// F = (curve(t) - pointA) \dot curve'(t)
	// F' = (curve(t) - pointA) \dot curve''(t) + curve'(t) \dot curve'(t)
	double F = ON_DotProduct(closest_point - pointA, tangent);
	double derivF = ON_DotProduct(closest_point - pointA, s_deriv) + ON_DotProduct(tangent, tangent);
	if (!ON_NearZero(derivF)) {
	    t -= F / derivF;
	}
    }

    t = std::min(curveB.Domain().Max(), t);
    t = std::max(curveB.Domain().Min(), t);
    closest_point = curveB.PointAt(t);
    dis = closest_point.DistanceTo(pointA);
    return dis <= tolerance && !isnan(t);
}


bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Curve &curveB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance,
	     const ON_Interval *curveB_domain,
	     Subcurve *treeB)
{
    if (tolerance <= 0.0) {
	tolerance = PCI_DEFAULT_TOLERANCE;
    }
    check_domain(curveB_domain, curveB.Domain(), "curveB_domain");

    Subcurve *root;
    if (treeB != NULL) {
	root = treeB;
    } else {
	root = new Subcurve;
	if (root == NULL) {
	    return false;
	}
	if (!build_curve_root(&curveB, curveB_domain, *root)) {
	    delete root;
	    return false;
	}
    }

    if (!root->IsPointIn(pointA, tolerance)) {
	if (treeB == NULL) {
	    delete root;
	}
	return false;
    }

    std::vector<Subcurve *> candidates, next_candidates;
    candidates.push_back(root);

    // Find the sub-curves that are possibly intersected with the point.
    // (The bounding boxes in the curve tree that include the point)
    for (int i = 0; i < MAX_PCI_DEPTH; i++) {
	next_candidates.clear();
	for (unsigned int j = 0; j < candidates.size(); j++) {
	    if (candidates[j]->m_islinear) {
		next_candidates.push_back(candidates[j]);
	    } else {
		if (candidates[j]->Split() == 0) {
		    if (candidates[j]->m_children[0]->IsPointIn(pointA, tolerance)) {
			next_candidates.push_back(candidates[j]->m_children[0]);
		    }
		    if (candidates[j]->m_children[1]->IsPointIn(pointA, tolerance)) {
			next_candidates.push_back(candidates[j]->m_children[1]);
		    }
		} else {
		    next_candidates.push_back(candidates[j]);
		}
	    }
	}
	candidates = next_candidates;
    }

    for (unsigned int i = 0; i < candidates.size(); i++) {
	// Use linear approximation to get an estimated intersection point
	ON_Line line(candidates[i]->m_curve->PointAtStart(), candidates[i]->m_curve->PointAtEnd());
	double t;
	line.ClosestPointTo(pointA, &t);

	// Make sure line_t belongs to [0, 1]
	double line_t;
	if (t < 0) {
	    line_t = 0;
	} else if (t > 1) {
	    line_t = 1;
	} else {
	    line_t = t;
	}

	double closest_point_t = candidates[i]->m_t.Min() + candidates[i]->m_t.Length() * line_t;

	// Use Newton iterations to get an accurate intersection point
	if (newton_pci(closest_point_t, pointA, curveB, tolerance)) {
	    ON_PX_EVENT Event;
	    Event.m_type = ON_PX_EVENT::pcx_point;
	    Event.m_A = pointA;
	    Event.m_B = curveB.PointAt(closest_point_t);
	    Event.m_b[0] = closest_point_t;
	    Event.m_Mid = (pointA + Event.m_B) * 0.5;
	    Event.m_radius = Event.m_B.DistanceTo(pointA) * 0.5;
	    x.Append(Event);
	    if (treeB == NULL) {
		delete root;
	    }
	    return true;
	}
    }

    // All candidates have no intersection
    if (treeB == NULL) {
	delete root;
    }
    return false;
}


/**
 * Point-surface intersections (PSI)
 *
 * approach:
 *
 * - Sub-divide the surface, calculating bounding boxes.
 *
 * - If the bounding box intersects with the point, go deeper until
 *   we reach the maximal depth or the sub-surface is planar
 *
 * - Use Newton-Raphson iterations to compute the closest point on
 *   the surface to that point.
 *
 * - If the closest point is within the given tolerance, there is an
 *   intersection.
 */


// We make the default tolerance for PSI the same as that of curve-surface
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define PSI_DEFAULT_TOLERANCE 0.001


// The default maximal depth for creating a surface tree (8) is way too
// much - killing performance. Since it's only used for getting an
// estimation, and we use Newton iterations afterwards, it's reasonable
// to use a smaller depth in this step.
#define MAX_PSI_DEPTH 8


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define PSI_MAX_ITERATIONS 100


HIDDEN bool
newton_psi(double &u, double &v, const ON_3dPoint &pointA, const ON_Surface &surfB, double tolerance)
{
    ON_3dPoint closest_point = surfB.PointAt(u, v);
    double dis = closest_point.DistanceTo(pointA);

    // Use Newton-Raphson method to get an accurate point of PSI.
    // We actually calculates the closest point on the surface to that point.
    //       (surface(u, v) - pointA) \dot deriv_u(u, v) = 0 (1)
    //	     (surface(u, v) - pointA) \dot deriv_v(u, v) = 0 (2)
    // Then surface(u, v) may be the closest point on the surface to pointA.

    double last_u = DBL_MAX * .5, last_v = DBL_MAX * .5;
    int iterations = 0;
    while (fabs(last_u - u) + fabs(last_v - v) > ON_ZERO_TOLERANCE
	   && iterations++ < PSI_MAX_ITERATIONS) {
	ON_3dVector du, dv, duu, duv, dvv;
	last_u = u;
	last_v = v;
	surfB.Ev2Der(u, v, closest_point, du, dv, duu, duv, dvv);
	ON_Matrix F(2, 1), J(2, 2);
	// F0 = (surface(u, v) - pointA) \dot du(u, v);
	F[0][0] = ON_DotProduct(closest_point - pointA, du);
	// F1 = (surface(u, v) - pointA) \dot dv(u, v);
	F[1][0] = ON_DotProduct(closest_point - pointA, dv);
	// dF0/du = du(u, v) \dot du(u, v) + (surface(u, v) - pointA) \dot duu(u, v)
	J[0][0] = ON_DotProduct(du, du) + ON_DotProduct(closest_point - pointA, duu);
	// dF0/dv = dv(u, v) \dot du(u, v) + (surface(u, v) - pointA) \dot duv(u, v)
	// dF1/du = du(u, v) \dot dv(u, v) + (surface(u, v) - pointA) \dot dvu(u, v)
	// dF0/dv = dF1/du
	J[0][1] = J[1][0] = ON_DotProduct(du, dv) + ON_DotProduct(closest_point - pointA, duv);
	// dF1/dv = dv(u, v) \dot dv(u, v) + (surface(u, v) - pointA) \dot dvv(u, v)
	J[1][1] = ON_DotProduct(dv, dv) + ON_DotProduct(closest_point - pointA, dvv);

	if (!J.Invert(0.0)) {
	    break;
	}
	ON_Matrix Delta;
	Delta.Multiply(J, F);
	u -= Delta[0][0];
	v -= Delta[1][0];
    }

    u = std::min(u, surfB.Domain(0).Max());
    u = std::max(u, surfB.Domain(0).Min());
    v = std::min(v, surfB.Domain(1).Max());
    v = std::max(v, surfB.Domain(1).Min());
    closest_point = surfB.PointAt(u, v);
    dis = closest_point.DistanceTo(pointA);
    return dis <= tolerance && !isnan(u) && !isnan(v);
}


bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Surface &surfaceB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tolerance,
	     const ON_Interval *surfaceB_udomain,
	     const ON_Interval *surfaceB_vdomain,
	     Subsurface *treeB)
{
    if (tolerance <= 0.0) {
	tolerance = PCI_DEFAULT_TOLERANCE;
    }

    ON_Interval u_domain, v_domain;
    u_domain = check_domain(surfaceB_udomain, surfaceB.Domain(0), "surfaceB_udomain");
    v_domain = check_domain(surfaceB_vdomain, surfaceB.Domain(1), "surfaceB_vdomain");

    Subsurface *root;
    if (treeB != NULL) {
	root = treeB;
    } else {
	root = new Subsurface;
	if (root == NULL) {
	    return false;
	}
	if (!build_surface_root(&surfaceB, &u_domain, &v_domain, *root)) {
	    delete root;
	    return false;
	}
    }

    if (!root->IsPointIn(pointA, tolerance)) {
	if (treeB == NULL) {
	    delete root;
	}
	return false;
    }

    std::vector<Subsurface *> candidates, next_candidates;
    candidates.push_back(root);

    // Find the sub-surfaces that are possibly intersected with the point.
    // (The bounding boxes in the surface tree include the point)
    for (int i = 0; i < MAX_PSI_DEPTH; i++) {
	next_candidates.clear();
	for (unsigned int j = 0; j < candidates.size(); j++) {
	    if (candidates[j]->m_isplanar) {
		next_candidates.push_back(candidates[j]);
	    } else {
		if (candidates[j]->Split() == 0) {
		    for (int k = 0; k < 4; k++) {
			if (candidates[j]->m_children[k] && candidates[j]->m_children[k]->IsPointIn(pointA, tolerance)) {
			    next_candidates.push_back(candidates[j]->m_children[k]);
			}
		    }
		} else {
		    next_candidates.push_back(candidates[j]);
		}
	    }
	}
	candidates = next_candidates;
    }

    for (unsigned int i = 0; i < candidates.size(); i++) {
	// Use the mid point of the bounding box as the starting point,
	// and use Newton iterations to get an accurate intersection.
	double u = candidates[i]->m_u.Mid(), v = candidates[i]->m_v.Mid();
	if (newton_psi(u, v, pointA, surfaceB, tolerance)) {
	    ON_PX_EVENT Event;
	    Event.m_type = ON_PX_EVENT::psx_point;
	    Event.m_A = pointA;
	    Event.m_B = surfaceB.PointAt(u, v);
	    Event.m_b[0] = u;
	    Event.m_b[1] = v;
	    Event.m_Mid = (pointA + Event.m_B) * 0.5;
	    Event.m_radius = Event.m_B.DistanceTo(pointA) * 0.5;
	    x.Append(Event);
	    if (treeB == NULL) {
		delete root;
	    }
	    return true;
	}
    }

    // All candidates have no intersection
    if (treeB == NULL) {
	delete root;
    }
    return false;
}


/**
 * Curve-curve intersection (CCI)
 *
 * approach:
 *
 * - Sub-divide the curves and calculate their bounding boxes.
 *
 * - Calculate the intersection of the bounding boxes, and go deeper if
 *   they intersect, until we reach the maximal depth or they are linear.
 *
 * - Use Newton-Raphson iterations to calculate an accurate intersection
 *   point.
 *
 * - Detect overlaps, eliminate duplications, and merge the continuous
 *   overlap events.
 */


// We make the default tolerance for CCI the same as that of curve-curve
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define CCI_DEFAULT_TOLERANCE 0.001


// The maximal depth for subdivision - trade-off between accuracy and
// performance
#define MAX_CCI_DEPTH 8


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define CCI_MAX_ITERATIONS 100


// We can only test a finite number of points to determine overlap.
// Here we test 16 points uniformly distributed.
#define CCI_OVERLAP_TEST_POINTS 16


HIDDEN void
newton_cci(double &t_a, double &t_b, const ON_Curve *curveA, const ON_Curve *curveB, double intersection_tolerance)
{
    // Equations:
    //   x_a(t_a) - x_b(t_b) = 0 (1)
    //   y_a(t_a) - y_b(t_b) = 0 (2)
    //   z_a(t_a) - z_b(t_b) = 0 (3)
    // It's an over-determined system.
    // We use Newton-Raphson iterations to solve two equations of the three,
    // and use the third for checking.
    double last_t_a = DBL_MAX * .5, last_t_b = DBL_MAX * .5;
    ON_3dPoint pointA = curveA->PointAt(t_a);
    ON_3dPoint pointB = curveB->PointAt(t_b);
    if (pointA.DistanceTo(pointB) < intersection_tolerance) {
	return;
    }

    int iteration = 0;
    while (fabs(last_t_a - t_a) + fabs(last_t_b - t_b) > ON_ZERO_TOLERANCE
	   && iteration++ < CCI_MAX_ITERATIONS) {
	last_t_a = t_a, last_t_b = t_b;
	ON_3dVector derivA, derivB;
	curveA->Ev1Der(t_a, pointA, derivA);
	curveB->Ev1Der(t_b, pointB, derivB);
	ON_Matrix J(2, 2), F(2, 1);

	// First, try to solve (1), (2)
	J[0][0] = derivA.x;
	J[0][1] = -derivB.x;
	J[1][0] = derivA.y;
	J[1][1] = -derivB.y;
	F[0][0] = pointA.x - pointB.x;
	F[1][0] = pointA.y - pointB.y;
	if (!J.Invert(0.0)) {
	    // The first try failed. Try to solve (1), (3)
	    J[0][0] = derivA.x;
	    J[0][1] = -derivB.x;
	    J[1][0] = derivA.z;
	    J[1][1] = -derivB.z;
	    F[0][0] = pointA.x - pointB.x;
	    F[1][0] = pointA.z - pointB.z;
	    if (!J.Invert(0.0)) {
		// Failed again. Try to solve (2), (3)
		J[0][0] = derivA.y;
		J[0][1] = -derivB.y;
		J[1][0] = derivA.z;
		J[1][1] = -derivB.z;
		F[0][0] = pointA.y - pointB.y;
		F[1][0] = pointA.z - pointB.z;
		if (!J.Invert(0.0)) {
		    // Cannot find a root
		    continue;
		}
	    }
	}
	ON_Matrix Delta;
	Delta.Multiply(J, F);
	t_a -= Delta[0][0];
	t_b -= Delta[1][0];
    }

    // Make sure the solution is inside the domains
    t_a = std::min(t_a, curveA->Domain().Max());
    t_a = std::max(t_a, curveA->Domain().Min());
    t_b = std::min(t_b, curveB->Domain().Max());
    t_b = std::max(t_b, curveB->Domain().Min());
}


static int
compare_by_m_a0(const ON_X_EVENT *a, const ON_X_EVENT *b)
{
    if (a->m_a[0] < b->m_a[0]) {
	return -1;
    }
    if (a->m_a[0] > b->m_a[0]) {
	return 1;
    }
    return 0;
}


int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Curve *curveB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     const ON_Interval *curveA_domain,
	     const ON_Interval *curveB_domain,
	     Subcurve *treeA,
	     Subcurve *treeB)
{
    if (curveA == NULL || curveB == NULL) {
	return 0;
    }

    int original_count = x.Count();

    if (intersection_tolerance <= 0.0) {
	intersection_tolerance = CCI_DEFAULT_TOLERANCE;
    }
    if (overlap_tolerance < intersection_tolerance) {
	overlap_tolerance = 2.0 * intersection_tolerance;
    }
    double t1_tolerance = intersection_tolerance;
    double t2_tolerance = intersection_tolerance;

    check_domain(curveA_domain, curveA->Domain(), "curveA_domain");
    check_domain(curveB_domain, curveB->Domain(), "curveB_domain");

    // Handle degenerated cases (one or both of them is a "point")
    bool ispoint_curveA = curveA->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE;
    bool ispoint_curveB = curveB->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE;
    ON_3dPoint startpointA = curveA->PointAtStart();
    ON_3dPoint startpointB = curveB->PointAtStart();
    ON_ClassArray<ON_PX_EVENT> px_event;
    if (ispoint_curveA && ispoint_curveB) {
	// Both curves are degenerated (point-point intersection)
	if (ON_Intersect(startpointA, startpointB, px_event, intersection_tolerance)) {
	    ON_X_EVENT Event;
	    Event.m_A[0] = startpointA;
	    Event.m_A[1] = curveA->PointAtEnd();
	    Event.m_B[0] = startpointB;
	    Event.m_B[1] = curveB->PointAtEnd();
	    Event.m_a[0] = curveA->Domain().Min();
	    Event.m_a[1] = curveA->Domain().Max();
	    Event.m_b[0] = curveB->Domain().Min();
	    Event.m_b[1] = curveB->Domain().Max();
	    Event.m_type = ON_X_EVENT::ccx_overlap;
	    x.Append(Event);
	    return 1;
	} else {
	    return 0;
	}
    } else if (ispoint_curveA) {
	// curveA is degenerated (point-curve intersection)
	if (ON_Intersect(startpointA, *curveB, px_event, intersection_tolerance)) {
	    ON_X_EVENT Event;
	    Event.m_A[0] = startpointA;
	    Event.m_A[1] = curveA->PointAtEnd();
	    Event.m_B[0] = Event.m_B[1] = px_event[0].m_B;
	    Event.m_a[0] = curveA->Domain().Min();
	    Event.m_a[1] = curveA->Domain().Max();
	    Event.m_b[0] = Event.m_b[1] = px_event[0].m_b[0];
	    Event.m_type = ON_X_EVENT::ccx_overlap;
	    x.Append(Event);
	    return 1;
	} else {
	    return 0;
	}
    } else if (ispoint_curveB) {
	// curveB is degenerated (point-curve intersection)
	if (ON_Intersect(startpointB, *curveA, px_event, intersection_tolerance)) {
	    ON_X_EVENT Event;
	    Event.m_A[0] = Event.m_A[1] = px_event[0].m_B;
	    Event.m_B[0] = startpointB;
	    Event.m_B[1] = curveB->PointAtEnd();
	    Event.m_a[0] = Event.m_a[1] = px_event[0].m_b[0];
	    Event.m_b[0] = curveB->Domain().Min();
	    Event.m_b[1] = curveB->Domain().Max();
	    Event.m_type = ON_X_EVENT::ccx_overlap;
	    x.Append(Event);
	    return 1;
	} else {
	    return 0;
	}
    }

    // Generate the roots of the two curve trees (if the trees passed in
    // are NULL)
    Subcurve *rootA, *rootB;
    if (treeA == NULL) {
	rootA = new Subcurve;
	if (rootA == NULL) {
	    return 0;
	}
	if (!build_curve_root(curveA, curveA_domain, *rootA)) {
	    delete rootA;
	    return 0;
	}
    } else {
	rootA = treeA;
    }

    if (treeB == NULL) {
	rootB = new Subcurve;
	if (rootB == NULL) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    return 0;
	}
	if (!build_curve_root(curveB, curveB_domain, *rootB)) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    delete rootB;
	    return 0;
	}
    } else {
	rootB = treeB;
    }

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double l = rootA->m_curve->BoundingBox().Diagonal().Length();
    double dl = curveA_domain ? curveA_domain->Length() : curveA->Domain().Length();
    if (!ON_NearZero(l)) {
	t1_tolerance = intersection_tolerance / l * dl;
    }
    l = rootB->m_curve->BoundingBox().Diagonal().Length();
    dl = curveB_domain ? curveB_domain->Length() : curveB->Domain().Length();
    if (!ON_NearZero(l)) {
	t2_tolerance = intersection_tolerance / l * dl;
    }

    typedef std::vector<std::pair<Subcurve *, Subcurve *> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootA->Intersect(*rootB, intersection_tolerance)) {
	candidates.push_back(std::make_pair(rootA, rootB));
    }

    // Use sub-division and bounding box intersections first.
    for (int h = 0; h <= MAX_CCI_DEPTH; h++) {
	if (candidates.empty()) {
	    break;
	}
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve *> splittedA, splittedB;
	    if ((*i).first->m_islinear || (*i).first->Split() != 0) {
		splittedA.push_back((*i).first);
	    } else {
		splittedA.push_back((*i).first->m_children[0]);
		splittedA.push_back((*i).first->m_children[1]);
	    }
	    if ((*i).second->m_islinear || (*i).second->Split() != 0) {
		splittedB.push_back((*i).second);
	    } else {
		splittedB.push_back((*i).second->m_children[0]);
		splittedB.push_back((*i).second->m_children[1]);
	    }
	    // Intersected children go to the next step
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedA[j]->Intersect(*splittedB[k], intersection_tolerance)) {
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
		    }
	}
	candidates = next_candidates;
    }

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    // For intersected bounding boxes, we calculate an accurate intersection
    // point.
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	// First, special handling for linear curves
	if (i->first->m_islinear && i->second->m_islinear) {
	    // Both of them are linear, we use ON_IntersectLineLine
	    ON_Line lineA(curveA->PointAt(i->first->m_t.Min()), curveA->PointAt(i->first->m_t.Max()));
	    ON_Line lineB(curveB->PointAt(i->second->m_t.Min()), curveB->PointAt(i->second->m_t.Max()));
	    if (lineA.Direction().IsParallelTo(lineB.Direction())) {
		// They are parallel
		if (lineA.MinimumDistanceTo(lineB) < intersection_tolerance) {
		    // we report a ccx_overlap event
		    double startB_on_A, endB_on_A, startA_on_B, endA_on_B;
		    lineA.ClosestPointTo(lineB.from, &startB_on_A);
		    lineA.ClosestPointTo(lineB.to, &endB_on_A);
		    lineB.ClosestPointTo(lineA.from, &startA_on_B);
		    lineB.ClosestPointTo(lineA.to, &endA_on_B);

		    if (startB_on_A > 1 + t1_tolerance || endB_on_A < -t1_tolerance
			|| startA_on_B > 1 + t2_tolerance || endA_on_B < -t2_tolerance) {
			continue;
		    }

		    double t_a1, t_a2, t_b1, t_b2;
		    if (startB_on_A > 0.0) {
			t_a1 = startB_on_A, t_b1 = 0.0;
		    } else {
			t_a1 = 0.0, t_b1 = startA_on_B;
		    }
		    if (endB_on_A < 1.0) {
			t_a2 = endB_on_A, t_b2 = 1.0;
		    } else {
			t_a2 = 1.0, t_b2 = endA_on_B;
		    }

		    ON_X_EVENT Event;
		    Event.m_A[0] = lineA.PointAt(t_a1);
		    Event.m_A[1] = lineA.PointAt(t_a2);
		    Event.m_B[0] = lineB.PointAt(t_b1);
		    Event.m_B[1] = lineB.PointAt(t_b2);
		    Event.m_a[0] = i->first->m_t.ParameterAt(t_a1);
		    Event.m_a[1] = i->first->m_t.ParameterAt(t_a2);
		    Event.m_b[0] = i->second->m_t.ParameterAt(t_b1);
		    Event.m_b[1] = i->second->m_t.ParameterAt(t_b2);
		    Event.m_type = ON_X_EVENT::ccx_overlap;
		    tmp_x.Append(Event);
		}
	    } else {
		// They are not parallel, check intersection point
		double t_lineA, t_lineB;
		double t_a, t_b;
		if (ON_IntersectLineLine(lineA, lineB, &t_lineA, &t_lineB, ON_ZERO_TOLERANCE, true)) {
		    // The line segments intersect
		    t_a = i->first->m_t.ParameterAt(t_lineA);
		    t_b = i->second->m_t.ParameterAt(t_lineB);

		    ON_X_EVENT Event;
		    Event.m_A[0] = Event.m_A[1] = lineA.PointAt(t_lineA);
		    Event.m_B[0] = Event.m_B[1] = lineB.PointAt(t_lineB);
		    Event.m_a[0] = Event.m_a[1] = t_a;
		    Event.m_b[0] = Event.m_b[1] = t_b;
		    Event.m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(Event);
		}
	    }
	} else {
	    // They are not both linear.

	    // Use two different start points - the two end-points of the interval
	    // If they converge to one point, it's considered an intersection
	    // point, otherwise it's considered an overlap event.
	    // FIXME: Find a better mechanism to check overlapping, because this method
	    // may miss some overlap cases. (Overlap events can also converge to one
	    // point)
	    double t_a1 = i->first->m_t.Min(), t_b1 = i->second->m_t.Min();
	    newton_cci(t_a1, t_b1, curveA, curveB, intersection_tolerance);
	    double t_a2 = i->first->m_t.Max(), t_b2 = i->second->m_t.Max();
	    newton_cci(t_a2, t_b2, curveA, curveB, intersection_tolerance);
	    if (isnan(t_a1) || isnan(t_b1)) {
		// The first iteration result is not sufficient
		std::swap(t_a1, t_a2);
		std::swap(t_b1, t_b2);
	    }
	    if (isnan(t_a1) || isnan(t_b1)) {
		continue;
	    }

	    ON_3dPoint pointA1 = curveA->PointAt(t_a1);
	    ON_3dPoint pointB1 = curveB->PointAt(t_b1);
	    ON_3dPoint pointA2 = curveA->PointAt(t_a2);
	    ON_3dPoint pointB2 = curveB->PointAt(t_b2);
	    if ((pointA1.DistanceTo(pointA2) < intersection_tolerance && pointB1.DistanceTo(pointB2) < intersection_tolerance)
		|| (isnan(t_a2) || isnan(t_b2))) {
		// it's considered the same point
		ON_3dPoint pointA = curveA->PointAt(t_a1);
		ON_3dPoint pointB = curveB->PointAt(t_b1);
		double distance = pointA.DistanceTo(pointB);
		// Check the validity of the solution
		if (distance < intersection_tolerance) {
		    ON_X_EVENT Event;
		    Event.m_A[0] = Event.m_A[1] = pointA;
		    Event.m_B[0] = Event.m_B[1] = pointB;
		    Event.m_a[0] = Event.m_a[1] = t_a1;
		    Event.m_b[0] = Event.m_b[1] = t_b1;
		    Event.m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(Event);
		}
	    } else {
		// Check overlap
		// bu_log("Maybe overlap.\n");
		double distance1 = pointA1.DistanceTo(pointB1);
		double distance2 = pointA2.DistanceTo(pointB2);

		// Check the validity of the solution
		if (distance1 < intersection_tolerance && distance2 < intersection_tolerance) {
		    int j;
		    for (j = 1; j < CCI_OVERLAP_TEST_POINTS; j++) {
			double strike = 1.0 / CCI_OVERLAP_TEST_POINTS;
			ON_3dPoint test_point = curveA->PointAt(t_a1 + (t_a2 - t_a1) * j * strike);
			ON_ClassArray<ON_PX_EVENT> pci_x;
			// Use point-curve intersection
			if (!ON_Intersect(test_point, *curveB, pci_x, overlap_tolerance)) {
			    break;
			}
		    }
		    if (j == CCI_OVERLAP_TEST_POINTS) {
			ON_X_EVENT Event;
			// We make sure that m_a[0] <= m_a[1]
			if (t_a1 <= t_a2) {
			    Event.m_A[0] = pointA1;
			    Event.m_A[1] = pointA2;
			    Event.m_B[0] = pointB1;
			    Event.m_B[1] = pointB2;
			    Event.m_a[0] = t_a1;
			    Event.m_a[1] = t_a2;
			    Event.m_b[0] = t_b1;
			    Event.m_b[1] = t_b2;
			} else {
			    Event.m_A[0] = pointA2;
			    Event.m_A[1] = pointA1;
			    Event.m_B[0] = pointB2;
			    Event.m_B[1] = pointB1;
			    Event.m_a[0] = t_a2;
			    Event.m_a[1] = t_a1;
			    Event.m_b[0] = t_b2;
			    Event.m_b[1] = t_b1;
			}
			Event.m_type = ON_X_EVENT::ccx_overlap;
			tmp_x.Append(Event);
			continue;
		    }
		    // if j != CCI_OVERLAP_TEST_POINTS, two ccx_point events should
		    // be generated. Fall through.
		}
		if (distance1 < intersection_tolerance) {
		    ON_X_EVENT Event;
		    Event.m_A[0] = Event.m_A[1] = pointA1;
		    Event.m_B[0] = Event.m_B[1] = pointB1;
		    Event.m_a[0] = Event.m_a[1] = t_a1;
		    Event.m_b[0] = Event.m_b[1] = t_b1;
		    Event.m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(Event);
		}
		if (distance2 < intersection_tolerance) {
		    ON_X_EVENT Event;
		    Event.m_A[0] = Event.m_A[1] = pointA2;
		    Event.m_B[0] = Event.m_B[1] = pointB2;
		    Event.m_a[0] = Event.m_a[1] = t_a2;
		    Event.m_b[0] = Event.m_b[1] = t_b2;
		    Event.m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(Event);
		}
	    }
	}
    }

    // Use an O(n^2) naive approach to eliminate duplications
    ON_SimpleArray<ON_X_EVENT> points, overlap;
    for (int i = 0; i < tmp_x.Count(); i++) {
	int j;
	if (tmp_x[i].m_type == ON_X_EVENT::ccx_overlap) {
	    // handled later (merge)
	    overlap.Append(tmp_x[i]);
	    continue;
	}
	// ccx_point
	for (j = 0; j < points.Count(); j++) {
	    if (tmp_x[i].m_A[0].DistanceTo(points[j].m_A[0]) < intersection_tolerance
		&& tmp_x[i].m_A[1].DistanceTo(points[j].m_A[1]) < intersection_tolerance
		&& tmp_x[i].m_B[0].DistanceTo(points[j].m_B[0]) < intersection_tolerance
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < intersection_tolerance) {
		break;
	    }
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // Merge the overlap events that are continuous
    ON_X_EVENT pending;
    overlap.QuickSort(compare_by_m_a0);
    if (overlap.Count()) {
	pending = overlap[0];
	for (int i = 1; i < overlap.Count(); i++) {
	    if (pending.m_a[1] < overlap[i].m_a[0] - t1_tolerance) {
		// pending[j] and overlap[i] are disjoint. pending[j] has
		// already merged, and should be removed from the list.
		// Because the elements in overlap[] are sorted by m_a[0],
		// the rest elements won't have intersection with pending[j]
		pending.m_A[0] = curveA->PointAt(pending.m_a[0]);
		pending.m_A[1] = curveA->PointAt(pending.m_a[1]);
		pending.m_B[0] = curveB->PointAt(pending.m_b[0]);
		pending.m_B[1] = curveB->PointAt(pending.m_b[1]);
		x.Append(pending);
		pending = overlap[i];
	    } else if (overlap[i].m_a[0] < pending.m_a[1] + t1_tolerance) {
		ON_Interval interval_1(overlap[i].m_b[0], overlap[i].m_b[1]);
		ON_Interval interval_2(pending.m_b[0], pending.m_b[1]);
		interval_1.MakeIncreasing();
		interval_1.m_t[0] -= t2_tolerance;
		interval_1.m_t[1] += t2_tolerance;
		if (interval_1.Intersection(interval_2)) {
		    // Need to merge: pending[j] = union(pending[j], overlap[i])
		    pending.m_a[1] = std::max(overlap[i].m_a[1], pending.m_a[1]);
		    pending.m_b[0] = std::min(overlap[i].m_b[0], pending.m_b[0]);
		    pending.m_b[1] = std::max(overlap[i].m_b[1], pending.m_b[1]);
		}
	    }
	}
	pending.m_A[0] = curveA->PointAt(pending.m_a[0]);
	pending.m_A[1] = curveA->PointAt(pending.m_a[1]);
	pending.m_B[0] = curveB->PointAt(pending.m_b[0]);
	pending.m_B[1] = curveB->PointAt(pending.m_b[1]);
	x.Append(pending);
    }

    // The intersection points shouldn't be inside the overlapped parts.
    int overlap_events = x.Count();
    for (int i = 0; i < points.Count(); i++) {
	int j;
	for (j = original_count; j < overlap_events; j++) {
	    if (points[i].m_a[0] > x[j].m_a[0] - t1_tolerance
		&& points[i].m_a[0] < x[j].m_a[1] + t1_tolerance) {
		break;
	    }
	}
	if (j == overlap_events) {
	    x.Append(points[i]);
	}
    }

    if (treeA == NULL) {
	delete rootA;
    }
    if (treeB == NULL) {
	delete rootB;
    }
    return x.Count() - original_count;
}


/**
 * Curve-surface intersection (CSI)
 *
 * approach:
 *
 * - Sub-divide the curve and the surface, calculate bounding boxes.
 *
 * - If their bounding boxes intersect, go deeper until we reach the maximal
 *   depth, or the sub-curve is linear and the sub-surface is planar.
 *
 * - Use two starting points within the bounding boxes, and perform Newton-
 *   Raphson iterations to get an accurate result.
 *
 * - Detect overlaps, eliminate duplications, and merge the continuous
 *   overlap events.
 */


// We make the default tolerance for CSI the same as that of curve-surface
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define CSI_DEFAULT_TOLERANCE 0.001


// The maximal depth for subdivision - trade-off between accuracy and
// performance
#define MAX_CSI_DEPTH 8


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define CSI_MAX_ITERATIONS 100


// We can only test a finite number of points to determine overlap.
// Here we test 2 points uniformly distributed.
#define CSI_OVERLAP_TEST_POINTS 2


HIDDEN void
newton_csi(double &t, double &u, double &v, const ON_Curve *curveA, const ON_Surface *surfB, double intersection_tolerance, Subsurface *tree)
{
    // Equations:
    //   C_x(t) - S_x(u,v) = 0
    //   C_y(t) - S_y(u,v) = 0
    //   C_z(t) - S_z(u,v) = 0
    // We use Newton-Raphson iterations to solve these equations.
    double last_t = DBL_MAX / 3, last_u = DBL_MAX / 3, last_v = DBL_MAX / 3;
    ON_3dPoint pointA = curveA->PointAt(t);
    ON_3dPoint pointB = surfB->PointAt(u, v);
    if (pointA.DistanceTo(pointB) < intersection_tolerance) {
	return;
    }

    // If curveA(t) is already on surfB... Newton iterations may not succeed
    // with this case. Use point-surface intersections.
    ON_ClassArray<ON_PX_EVENT> px_event;
    if (ON_Intersect(pointA, *surfB, px_event, intersection_tolerance, 0, 0, tree)) {
	u = px_event[0].m_b.x;
	v = px_event[0].m_b.y;
	return;
    }

    int iteration = 0;
    while (fabs(last_t - t) + fabs(last_u - u) + fabs(last_v - v) > ON_ZERO_TOLERANCE
	   && iteration++ < CSI_MAX_ITERATIONS) {
	last_t = t, last_u = u, last_v = v;
	// F[0] = C_x(t) - S_x(u,v)
	// F[1] = C_y(t) - S_y(u,v)
	// F[2] = C_z(t) - S_z(u,v)
	// J[i][0] = dF[i]/dt = C'(t)
	// J[i][1] = dF[i]/du = -dS(u,v)/du
	// J[i][2] = dF[i]/dv = -dS(u,v)/dv
	ON_3dVector derivA, derivBu, derivBv;
	curveA->Ev1Der(t, pointA, derivA);
	surfB->Ev1Der(u, v, pointB, derivBu, derivBv);
	ON_Matrix J(3, 3), F(3, 1);
	for (int i = 0; i < 3; i++) {
	    J[i][0] = derivA[i];
	    J[i][1] = -derivBu[i];
	    J[i][2] = -derivBv[i];
	    F[i][0] = pointA[i] - pointB[i];
	}
	if (!J.Invert(0.0)) {
	    break;
	}
	ON_Matrix Delta;
	Delta.Multiply(J, F);
	t -= Delta[0][0];
	u -= Delta[1][0];
	v -= Delta[2][0];
    }

    // Make sure the solution is inside the domains
    t = std::min(t, curveA->Domain().Max());
    t = std::max(t, curveA->Domain().Min());
    u = std::min(u, surfB->Domain(0).Max());
    u = std::max(u, surfB->Domain(0).Min());
    v = std::min(v, surfB->Domain(1).Max());
    v = std::max(v, surfB->Domain(1).Min());
}


int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Surface *surfaceB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     const ON_Interval *curveA_domain,
	     const ON_Interval *surfaceB_udomain,
	     const ON_Interval *surfaceB_vdomain,
	     ON_CurveArray *overlap2d,
	     Subcurve *treeA,
	     Subsurface *treeB)
{
    if (curveA == NULL || surfaceB == NULL) {
	return 0;
    }

    int original_count = x.Count();

    if (intersection_tolerance <= 0.0) {
	intersection_tolerance = CSI_DEFAULT_TOLERANCE;
    }
    if (overlap_tolerance < intersection_tolerance) {
	overlap_tolerance = 2.0 * intersection_tolerance;
    }

    check_domain(curveA_domain, curveA->Domain(), "curveA_domain");
    check_domain(surfaceB_udomain, surfaceB->Domain(0), "surfaceB_udomain");
    check_domain(surfaceB_vdomain, surfaceB->Domain(1), "surfaceB_vdomain");

    // If the curve is degenerated to a point...
    if (curveA->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE) {
	// point-surface intersections
	ON_3dPoint pointA = curveA->PointAtStart();
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (ON_Intersect(pointA, *surfaceB, px_event, intersection_tolerance)) {
	    ON_X_EVENT Event;
	    Event.m_A[0] = pointA;
	    Event.m_A[1] = curveA->PointAtEnd();
	    Event.m_B[0] = Event.m_B[1] = px_event[0].m_B;
	    Event.m_a[0] = curveA->Domain().Min();
	    Event.m_a[1] = curveA->Domain().Max();
	    Event.m_b[0] = Event.m_b[2] = px_event[0].m_b[0];
	    Event.m_b[1] = Event.m_b[3] = px_event[0].m_b[1];
	    Event.m_type = ON_X_EVENT::csx_overlap;
	    x.Append(Event);
	    if (overlap2d) {
		overlap2d->Append(new ON_LineCurve(Event.m_B[0], Event.m_B[1]));
	    }
	    return 1;
	} else {
	    return 0;
	}
    }

    // Generate the roots of the curve tree and the surface tree
    // (If they are not passed in...)
    Subcurve *rootA;
    Subsurface *rootB;

    if (treeA == NULL) {
	rootA = new Subcurve;
	if (rootA == NULL) {
	    return 0;
	}
	if (!build_curve_root(curveA, curveA_domain, *rootA)) {
	    delete rootA;
	    return 0;
	}
    } else {
	rootA = treeA;
    }

    if (treeB == NULL) {
	rootB = new Subsurface;
	if (rootB == NULL) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    return 0;
	}
	if (!build_surface_root(surfaceB, surfaceB_udomain, surfaceB_vdomain, *rootB)) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    delete rootB;
	    return 0;
	}
    } else {
	rootB = treeB;
    }

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double t_tolerance = intersection_tolerance;
    double u_tolerance = intersection_tolerance;
    double v_tolerance = intersection_tolerance;
    double l = rootA->m_curve->BoundingBox().Diagonal().Length();
    double dl = curveA_domain ? curveA_domain->Length() : curveA->Domain().Length();
    if (!ON_NearZero(l)) {
	t_tolerance = intersection_tolerance / l * dl;
    }
    l = rootB->m_surf->BoundingBox().Diagonal().Length();
    dl = surfaceB_udomain ? surfaceB_udomain->Length() : surfaceB->Domain(0).Length();
    if (!ON_NearZero(l)) {
	u_tolerance = intersection_tolerance / l * dl;
    }
    dl = surfaceB_vdomain ? surfaceB_vdomain->Length() : surfaceB->Domain(1).Length();
    if (!ON_NearZero(l)) {
	v_tolerance = intersection_tolerance / l * dl;
    }

    typedef std::vector<std::pair<Subcurve *, Subsurface *> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootB->Intersect(*rootA, intersection_tolerance)) {
	candidates.push_back(std::make_pair(rootA, rootB));
    }

    // Use sub-division and bounding box intersections first.
    for (int h = 0; h <= MAX_CSI_DEPTH; h++) {
	if (candidates.empty()) {
	    break;
	}
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve *> splittedA;
	    std::vector<Subsurface *> splittedB;
	    if (i->first->m_islinear && i->second->m_isplanar) {
		splittedA.push_back((*i).first);
		splittedB.push_back((*i).second);
	    } else {
		if ((*i).first->Split() != 0) {
		    splittedA.push_back((*i).first);
		} else {
		    splittedA.push_back((*i).first->m_children[0]);
		    splittedA.push_back((*i).first->m_children[1]);
		}
		if ((*i).second->Split() != 0) {
		    splittedB.push_back((*i).second);
		} else {
		    for (int j = 0; j < 4; j++) {
			splittedB.push_back((*i).second->m_children[j]);
		    }
		}
	    }
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedB[k]->Intersect(*splittedA[j], intersection_tolerance)) {
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
		    }
	}
	candidates = next_candidates;
    }

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    // For intersected bounding boxes, we calculate an accurate intersection
    // point.
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	if (i->first->m_islinear && i->second->m_isplanar) {
	    // Intersections of a line and a plane
	    // We can have some optimizations
	    ON_Line line(curveA->PointAt(i->first->m_t.Min()), curveA->PointAt(i->first->m_t.Max()));
	    ON_Plane plane;
	    bool success = true;
	    ON_3dPoint point1 = surfaceB->PointAt(i->second->m_u.Min(), i->second->m_v.Min());
	    ON_3dPoint point2 = surfaceB->PointAt(i->second->m_u.Min(), i->second->m_v.Max());
	    ON_3dPoint point3 = surfaceB->PointAt(i->second->m_u.Max(), i->second->m_v.Max());
	    ON_3dPoint point4 = surfaceB->PointAt(i->second->m_u.Max(), i->second->m_v.Min());
	    if (!plane.CreateFromPoints(point1, point2, point3))
		if (!plane.CreateFromPoints(point1, point2, point4))
		    if (!plane.CreateFromPoints(point1, point3, point4))
			if (!plane.CreateFromPoints(point2, point3, point4)) {
			    success = false;
			}

	    if (success && !ON_NearZero(line.Length())) {
		if (line.Direction().IsPerpendicularTo(plane.Normal())) {
		    // They are parallel (or overlap)

		    if (line.InPlane(plane, intersection_tolerance)) {
			// The line is on the surface's plane. The end-points of
			// the overlap must be the linecurve's end-points or
			// the intersection between the linecurve and the boundary
			// of the surface

			ON_X_EVENT Event;

			// First, we check the endpoints of the line segment (PSI)
			ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
			int intersections = 0;
			if (ON_Intersect(line.from, *surfaceB, px_event1, intersection_tolerance, 0, 0, treeB)) {
			    Event.m_A[intersections] = line.from;
			    Event.m_B[intersections] = px_event1[0].m_B;
			    Event.m_a[intersections] = i->first->m_t.Min();
			    Event.m_b[2 * intersections] = px_event1[0].m_b[0];
			    Event.m_b[2 * intersections + 1] = px_event1[0].m_b[1];
			    intersections++;
			}
			if (ON_Intersect(line.to, *surfaceB, px_event2, intersection_tolerance, 0, 0, treeB)) {
			    Event.m_A[intersections] = line.to;
			    Event.m_B[intersections] = px_event2[0].m_B;
			    Event.m_a[intersections] = i->first->m_t.Max();
			    Event.m_b[2 * intersections] = px_event2[0].m_b[0];
			    Event.m_b[2 * intersections + 1] = px_event2[0].m_b[1];
			    intersections++;
			}

			// Then, we check the intersection of the line segment
			// and the boundaries of the surface (CCI)
			ON_Line boundary[4];
			ON_SimpleArray<double> line_t, boundary_t;
			ON_SimpleArray<int> boundary_index;
			boundary[0] = ON_Line(point1, point2);
			boundary[1] = ON_Line(point2, point3);
			boundary[2] = ON_Line(point4, point3);
			boundary[3] = ON_Line(point1, point4);
			for (int j = 0; j < 4; j++) {
			    double t1, t2;
			    if (ON_IntersectLineLine(line, boundary[j], &t1, &t2, ON_ZERO_TOLERANCE, true)) {
				int k;
				for (k = 0; k < line_t.Count(); k++) {
				    // Check duplication
				    if (ON_NearZero(t1 - line_t[k])) {
					break;
				    }
				}
				if (k == line_t.Count()) {
				    line_t.Append(t1);
				    boundary_t.Append(t2);
				    boundary_index.Append(j);
				}
			    }
			}
			int count = line_t.Count();

			for (int j = 0; j < count; j++) {
			    if (intersections >= 2) {
				break;
			    }
			    // Convert from the boxes' domain to the whole surface's domain
			    double surf_u = 0.0, surf_v = 0.0;
			    switch (boundary_index[j]) {
			    case 0:
				surf_u = i->second->m_u.Min();
				surf_v = i->second->m_v.ParameterAt(boundary_t[j]);
				break;
			    case 1:
				surf_u = i->second->m_u.ParameterAt(boundary_t[j]);
				surf_v = i->second->m_v.Max();
				break;
			    case 2:
				surf_u = i->second->m_u.Max();
				surf_v = i->second->m_v.ParameterAt(boundary_t[j]);
				break;
			    case 3:
				surf_u = i->second->m_u.ParameterAt(boundary_t[j]);
				surf_v = i->second->m_v.Min();
				break;
			    }
			    Event.m_A[intersections] = line.PointAt(line_t[j]);
			    Event.m_B[intersections] = surfaceB->PointAt(surf_u, surf_v);
			    Event.m_a[intersections] = i->first->m_t.ParameterAt(line_t[j]);
			    Event.m_b[2 * intersections] = surf_u;
			    Event.m_b[2 * intersections + 1] = surf_v;
			    intersections++;
			}

			// Generate an ON_X_EVENT
			if (intersections == 0) {
			    continue;
			}
			if (intersections == 1) {
			    Event.m_type = ON_X_EVENT::csx_point;
			    Event.m_A[1] = Event.m_A[0];
			    Event.m_B[1] = Event.m_B[0];
			    Event.m_a[1] = Event.m_a[0];
			    Event.m_b[2] = Event.m_b[0];
			    Event.m_b[3] = Event.m_b[1];
			} else {
			    Event.m_type = ON_X_EVENT::csx_overlap;
			    if (Event.m_a[0] > Event.m_a[1]) {
				std::swap(Event.m_A[0], Event.m_A[1]);
				std::swap(Event.m_B[0], Event.m_B[1]);
				std::swap(Event.m_a[0], Event.m_a[1]);
				std::swap(Event.m_b[0], Event.m_b[2]);
				std::swap(Event.m_b[1], Event.m_b[3]);
			    }
			}
			tmp_x.Append(Event);
			continue;
		    } else {
			continue;
		    }
		} else {
		    // They are not parallel, check intersection point
		    double cos_angle = fabs(ON_DotProduct(plane.Normal(), line.Direction())) / (plane.Normal().Length() * line.Direction().Length());
		    // cos_angle != 0, otherwise the curve and the plane are parallel.
		    double distance = fabs(plane.DistanceTo(line.from));
		    double line_t = distance / cos_angle / line.Length();
		    if (line_t > 1.0 + t_tolerance) {
			continue;
		    }
		    ON_3dPoint intersection = line.from + line.Direction() * line_t;
		    ON_ClassArray<ON_PX_EVENT> px_event;
		    if (!ON_Intersect(intersection, *(i->second->m_surf), px_event, intersection_tolerance, 0, 0, treeB)) {
			continue;
		    }

		    ON_X_EVENT Event;
		    Event.m_A[0] = Event.m_A[1] = intersection;
		    Event.m_B[0] = Event.m_B[1] = px_event[0].m_B;
		    Event.m_a[0] = Event.m_a[1] = i->first->m_t.ParameterAt(line_t);
		    Event.m_b[0] = Event.m_b[2] = px_event[0].m_b.x;
		    Event.m_b[1] = Event.m_b[3] = px_event[0].m_b.y;
		    Event.m_type = ON_X_EVENT::csx_point;
		    tmp_x.Append(Event);
		}
	    }
	}

	// They are not both linear or planar, or the above try failed.

	// Use two different start points - the two end-points of the interval
	// of the Subcurve's m_t.
	// If they converge to one point, it's considered an intersection
	// point, otherwise it's considered an overlap event.
	// FIXME: Find a better mechanism to check overlapping, because this method
	// may miss some overlap cases. (Overlap events can also converge to one
	// point)

	double u1 = i->second->m_u.Mid(), v1 = i->second->m_v.Mid();
	double t1 = i->first->m_t.Min();
	newton_csi(t1, u1, v1, curveA, surfaceB, intersection_tolerance, treeB);
	double u2 = i->second->m_u.Mid(), v2 = i->second->m_v.Mid();
	double t2 = i->first->m_t.Max();
	newton_csi(t2, u2, v2, curveA, surfaceB, intersection_tolerance, treeB);

	if (isnan(u1) || isnan(v1) || isnan(t1)) {
	    u1 = u2;
	    v1 = v2;
	    t1 = t2;
	}

	if (isnan(u1) || isnan(v1) || isnan(t1)) {
	    continue;
	}

	ON_3dPoint pointA1 = curveA->PointAt(t1);
	ON_3dPoint pointB1 = surfaceB->PointAt(u1, v1);
	ON_3dPoint pointA2 = curveA->PointAt(t2);
	ON_3dPoint pointB2 = surfaceB->PointAt(u2, v2);
	if (pointA1.DistanceTo(pointA2) < intersection_tolerance
	    && pointB1.DistanceTo(pointB2) < intersection_tolerance) {
	    // it's considered the same point (both converge)
	    double distance = pointA1.DistanceTo(pointB1);
	    // Check the validity of the solution
	    if (distance < intersection_tolerance) {
		ON_X_EVENT Event;
		Event.m_A[0] = Event.m_A[1] = pointA1;
		Event.m_B[0] = Event.m_B[1] = pointB1;
		Event.m_a[0] = Event.m_a[1] = t1;
		Event.m_b[0] = Event.m_b[2] = u1;
		Event.m_b[1] = Event.m_b[3] = v1;
		Event.m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(Event);
	    }
	} else {
	    // Check overlap
	    // bu_log("Maybe overlap.\n");
	    double distance1 = pointA1.DistanceTo(pointB1);
	    double distance2 = pointA2.DistanceTo(pointB2);

	    // Check the validity of the solution
	    if (distance1 < intersection_tolerance && distance2 < intersection_tolerance) {
		ON_X_EVENT Event;
		// We make sure that m_a[0] <= m_a[1]
		if (t1 <= t2) {
		    Event.m_A[0] = pointA1;
		    Event.m_A[1] = pointA2;
		    Event.m_B[0] = pointB1;
		    Event.m_B[1] = pointB2;
		    Event.m_a[0] = t1;
		    Event.m_a[1] = t2;
		    Event.m_b[0] = u1;
		    Event.m_b[1] = v1;
		    Event.m_b[2] = u2;
		    Event.m_b[3] = v2;
		} else {
		    Event.m_A[0] = pointA2;
		    Event.m_A[1] = pointA1;
		    Event.m_B[0] = pointB2;
		    Event.m_B[1] = pointB1;
		    Event.m_a[0] = t2;
		    Event.m_a[1] = t1;
		    Event.m_b[0] = u2;
		    Event.m_b[1] = v2;
		    Event.m_b[2] = u1;
		    Event.m_b[3] = v1;
		}
		int j;
		for (j = 1; j < CSI_OVERLAP_TEST_POINTS; j++) {
		    double strike = 1.0 / CSI_OVERLAP_TEST_POINTS;
		    ON_3dPoint test_point = curveA->PointAt(t1 + (t2 - t1) * j * strike);
		    ON_ClassArray<ON_PX_EVENT> psi_x;
		    // Use point-surface intersection
		    if (!ON_Intersect(test_point, *surfaceB, psi_x, overlap_tolerance, 0, 0, treeB)) {
			break;
		    }
		}
		if (j == CSI_OVERLAP_TEST_POINTS) {
		    Event.m_type = ON_X_EVENT::csx_overlap;
		    tmp_x.Append(Event);
		    continue;
		}
		// if j != CSI_OVERLAP_TEST_POINTS, two csx_point events may
		// be generated. Fall through.
	    }
	    if (distance1 < intersection_tolerance) {
		ON_X_EVENT Event;
		Event.m_A[0] = Event.m_A[1] = pointA1;
		Event.m_B[0] = Event.m_B[1] = pointB1;
		Event.m_a[0] = Event.m_a[1] = t1;
		Event.m_b[0] = Event.m_b[2] = u1;
		Event.m_b[1] = Event.m_b[3] = v1;
		Event.m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(Event);
	    }
	    if (distance2 < intersection_tolerance) {
		ON_X_EVENT Event;
		Event.m_A[0] = Event.m_A[1] = pointA2;
		Event.m_B[0] = Event.m_B[1] = pointB2;
		Event.m_a[0] = Event.m_a[1] = t2;
		Event.m_b[0] = Event.m_b[2] = u2;
		Event.m_b[1] = Event.m_b[3] = v2;
		Event.m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(Event);
	    }
	}
    }

    ON_SimpleArray<ON_X_EVENT> points, overlap;
    // Use an O(n^2) naive approach to eliminate duplications
    for (int i = 0; i < tmp_x.Count(); i++) {
	int j;
	if (tmp_x[i].m_type == ON_X_EVENT::csx_overlap) {
	    overlap.Append(tmp_x[i]);
	    continue;
	}
	// csx_point
	for (j = 0; j < points.Count(); j++) {
	    if (tmp_x[i].m_A[0].DistanceTo(points[j].m_A[0]) < intersection_tolerance
		&& tmp_x[i].m_A[1].DistanceTo(points[j].m_A[1]) < intersection_tolerance
		&& tmp_x[i].m_B[0].DistanceTo(points[j].m_B[0]) < intersection_tolerance
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < intersection_tolerance) {
		break;
	    }
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // Merge the overlap events that are continuous
    ON_X_EVENT pending;
    ON_3dPointArray ptarrayB;
    overlap.QuickSort(compare_by_m_a0);
    if (overlap.Count()) {
	pending = overlap[0];
	ptarrayB.Append(ON_3dPoint(pending.m_b[0], pending.m_b[1], 0.0));
	for (int i = 1; i < overlap.Count(); i++) {
	    if (pending.m_a[1] < overlap[i].m_a[0] - t_tolerance) {
		// pending[j] and overlap[i] are disjoint. pending[j] has
		// already merged, and should be removed from the list.
		// Because the elements in overlap[] are sorted by m_a[0],
		// the rest elements won't have intersection with pending[j]
		x.Append(pending);
		if (overlap2d) {
		    ptarrayB.Append(ON_3dPoint(pending.m_b[2], pending.m_b[3], 0.0));
		    ON_PolylineCurve *polyline = new ON_PolylineCurve(ptarrayB);
		    polyline->ChangeDimension(2);
		    overlap2d->Append(curve_fitting(polyline));
		    ptarrayB.Empty();
		    ptarrayB.Append(ON_3dPoint(overlap[i].m_b[0], overlap[i].m_b[1], 0.0));
		}
		pending = overlap[i];
	    } else if (overlap[i].m_a[0] < pending.m_a[1] + t_tolerance) {
		if (overlap2d) {
		    ptarrayB.Append(ON_3dPoint(overlap[i].m_b[0], overlap[i].m_b[1], 0.0));
		}
		ON_Interval interval_u1(overlap[i].m_b[0], overlap[i].m_b[2]);
		ON_Interval interval_u2(pending.m_b[0], pending.m_b[2]);
		ON_Interval interval_v1(overlap[i].m_b[1], overlap[i].m_b[3]);
		ON_Interval interval_v2(pending.m_b[1], pending.m_b[3]);
		interval_u1.MakeIncreasing();
		interval_u1.m_t[0] -= u_tolerance;
		interval_u1.m_t[1] += u_tolerance;
		interval_v1.MakeIncreasing();
		interval_v1.m_t[0] -= v_tolerance;
		interval_v1.m_t[1] += v_tolerance;
		if (interval_u1.Intersection(interval_u2) && interval_v1.Intersection(interval_v2)) {
		    // if the uv rectangle of them intersects, it's consider overlap.
		    // Need to merge: pending[j] = union(pending[j], overlap[i])
		    if (overlap[i].m_a[1] > pending.m_a[1]) {
			pending.m_a[1] = overlap[i].m_a[1];
			pending.m_b[2] = overlap[i].m_b[2];
			pending.m_b[3] = overlap[i].m_b[3];
			pending.m_A[1] = overlap[i].m_A[1];
			pending.m_B[1] = overlap[i].m_B[1];
		    }
		}
	    }
	}
	x.Append(pending);
	if (overlap2d) {
	    ptarrayB.Append(ON_3dPoint(pending.m_b[2], pending.m_b[3], 0.0));
	    ON_PolylineCurve *polyline = new ON_PolylineCurve(ptarrayB);
	    polyline->ChangeDimension(2);
	    overlap2d->Append(curve_fitting(polyline));
	}
    }

    // The intersection points shouldn't be inside the overlapped parts.
    int overlap_events = x.Count();
    for (int i = 0; i < points.Count(); i++) {
	int j;
	for (j = original_count; j < overlap_events; j++) {
	    if (points[i].m_a[0] > x[j].m_a[0] - t_tolerance
		&& points[i].m_a[0] < x[j].m_a[1] + t_tolerance) {
		break;
	    }
	}
	if (j == overlap_events) {
	    x.Append(points[i]);
	    if (overlap2d) {
		overlap2d->Append(NULL);
	    }
	}
    }

    if (treeA == NULL) {
	delete rootA;
    }
    if (treeB == NULL) {
	delete rootB;
    }
    return x.Count() - original_count;
}


/**
 * Surface-surface intersections (SSI)
 *
 * approach:
 *
 * - Generate the bounding box of the two surfaces.
 *
 * - If their bounding boxes intersect:
 *
 * -- Split the two surfaces, both into four parts, and calculate the
 *    sub-surfaces' bounding boxes
 *
 * -- Calculate the intersection of sub-surfaces' bboxes, if they do
 *    intersect, go deeper with splitting surfaces and smaller bboxes,
 *    otherwise trace back.
 *
 * - After getting the intersecting bboxes, approximate the surface
 *   inside the bbox as two triangles, and calculate the intersection
 *   points of the triangles (both in 3d space and two surfaces' UV
 *   space)
 *
 * - If an intersection point is accurate enough (within the tolerance),
 *   we append it to the list. Otherwise, we use Newton-Raphson iterations
 *   to solve an under-determined system to get an more accurate inter-
 *   section point.
 *
 * - Fit the intersection points into polyline curves, and then to NURBS
 *   curves. Points with distance less than max_dis are considered in
 *   one curve. Points that cannot be fitted to a curve are regarded as
 *   single points.
 *
 * - Linear fittings and conic fittings are done to simplify the curves'
 *   representations.
 *
 * - Note: the overlap cases are handled at the beginning separately.
 *
 * See: Adarsh Krishnamurthy, Rahul Khardekar, Sara McMains, Kirk Haller,
 * and Gershon Elber. 2008.
 * Performing efficient NURBS modeling operations on the GPU.
 * In Proceedings of the 2008 ACM symposium on Solid and physical modeling
 * (SPM '08). ACM, New York, NY, USA, 257-268. DOI=10.1145/1364901.1364937
 * http://doi.acm.org/10.1145/1364901.1364937
 */


// The maximal depth for subdivision - trade-off between accuracy and
// performance
#define MAX_SSI_DEPTH 8


// We make the default tolerance for SSI the same as that of surface-surface
// intersections defined by openNURBS (see other/openNURBS/opennurbs_surface.h)
#define SSI_DEFAULT_TOLERANCE 0.001


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define SSI_MAX_ITERATIONS 100


struct Triangle {
    ON_3dPoint A, B, C;
    inline void CreateFromPoints(ON_3dPoint &p_A, ON_3dPoint &p_B, ON_3dPoint &p_C)
    {
	A = p_A;
	B = p_B;
	C = p_C;
    }
    ON_3dPoint BarycentricCoordinate(ON_3dPoint &pt)
    {
	double x, y, z, pivot_ratio;
	if (ON_Solve2x2(A[0] - C[0], B[0] - C[0], A[1] - C[1], B[1] - C[1], pt[0] - C[0], pt[1] - C[1], &x, &y, &pivot_ratio) == 2) {
	    return ON_3dPoint(x, y, 1 - x - y);
	}
	if (ON_Solve2x2(A[0] - C[0], B[0] - C[0], A[2] - C[2], B[2] - C[2], pt[0] - C[0], pt[2] - C[2], &x, &z, &pivot_ratio) == 2) {
	    return ON_3dPoint(x, 1 - x - z, z);
	}
	if (ON_Solve2x2(A[1] - C[1], B[1] - C[1], A[2] - C[2], B[2] - C[2], pt[1] - C[1], pt[2] - C[2], &y, &z, &pivot_ratio) == 2) {
	    return ON_3dPoint(1 - y - z, y, z);
	}
	return ON_3dPoint::UnsetPoint;
    }
    void GetLineSegments(ON_Line line[3]) const
    {
	line[0] = ON_Line(A, B);
	line[1] = ON_Line(A, C);
	line[2] = ON_Line(B, C);
    }
};


HIDDEN bool
triangle_intersection(const struct Triangle &TriA, const struct Triangle &TriB, ON_3dPoint &center)
{
    ON_Plane plane_a(TriA.A, TriA.B, TriA.C);
    ON_Plane plane_b(TriB.A, TriB.B, TriB.C);
    ON_Line intersect;
    if (plane_a.Normal().IsParallelTo(plane_b.Normal())) {
	if (!ON_NearZero(plane_a.DistanceTo(plane_b.origin))) {
	    // parallel
	    return false;
	}
	// The two triangles are in the same plane.
	ON_3dPoint pt(0.0, 0.0, 0.0);
	int count = 0;
	ON_Line lineA[3], lineB[3];
	TriA.GetLineSegments(lineA);
	TriB.GetLineSegments(lineB);
	for (int i = 0; i < 3; i++) {
	    for (int j = 0; j < 3; j++) {
		double t_a, t_b;
		if (ON_IntersectLineLine(lineA[i], lineB[j], &t_a, &t_b, ON_ZERO_TOLERANCE, true)) {
		    // we don't care if they are parallel or coincident
		    pt += lineA[i].PointAt(t_a);
		    count++;
		}
	    }
	}
	if (!count) {
	    return false;
	}
	center = pt / count;
	return true;
    }

    if (!ON_Intersect(plane_a, plane_b, intersect)) {
	return false;
    }
    ON_3dVector line_normal = ON_CrossProduct(plane_a.Normal(), intersect.Direction());

    // dpi - >0: one side of the line, <0: another side, ==0: on the line
    double dp1 = ON_DotProduct(TriA.A - intersect.from, line_normal);
    double dp2 = ON_DotProduct(TriA.B - intersect.from, line_normal);
    double dp3 = ON_DotProduct(TriA.C - intersect.from, line_normal);

    int points_on_one_side = (dp1 >= ON_ZERO_TOLERANCE) + (dp2 >= ON_ZERO_TOLERANCE) + (dp3 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3) {
	return false;
    }

    line_normal = ON_CrossProduct(plane_b.Normal(), intersect.Direction());
    double dp4 = ON_DotProduct(TriB.A - intersect.from, line_normal);
    double dp5 = ON_DotProduct(TriB.B - intersect.from, line_normal);
    double dp6 = ON_DotProduct(TriB.C - intersect.from, line_normal);
    points_on_one_side = (dp4 >= ON_ZERO_TOLERANCE) + (dp5 >= ON_ZERO_TOLERANCE) + (dp6 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3) {
	return false;
    }

    double t[4];
    int count = 0;
    double t1, t2;
    // dpi*dpj <= 0 - the corresponding points are on different sides,
    // or at least one of them are on the intersection line.
    // - the line segment between them are intersected by the plane-plane
    // intersection line
    if (dp1 * dp2 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.B, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp1 * dp3 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp2 * dp3 < ON_ZERO_TOLERANCE && count != 2) {
	intersect.ClosestPointTo(TriA.B, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (count != 2) {
	return false;
    }
    if (dp4 * dp5 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.B, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp4 * dp6 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp5 * dp6 < ON_ZERO_TOLERANCE && count != 4) {
	intersect.ClosestPointTo(TriB.B, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (count != 4) {
	return false;
    }
    if (t[0] > t[1]) {
	std::swap(t[1], t[0]);
    }
    if (t[2] > t[3]) {
	std::swap(t[3], t[2]);
    }
    double left = std::max(t[0], t[2]);
    double right = std::min(t[1], t[3]);
    if (left > right) {
	return false;
    }
    center = intersect.PointAt((left + right) / 2);
    return true;
}


struct PointPair {
    int indexA, indexB;
    double distance3d, distanceU, distanceV, distanceS, distanceT;
    double tol;
    bool operator < (const PointPair &_pp) const
    {
	if (ON_NearZero(distance3d - _pp.distance3d, tol)) {
	    return distanceU + distanceV + distanceS + distanceT < _pp.distanceU + _pp.distanceV + _pp.distanceS + _pp.distanceT;
	}
	return distance3d < _pp.distance3d;
    }
};


HIDDEN void
closed_domain(int type, const ON_Surface *surfA, const ON_Surface *surfB, ON_3dPointArray &curvept, ON_2dPointArray &curveuv, ON_2dPointArray &curvest)
{
    // type =
    // 0: uv.x, 1: uv.y, 2: st.x, 3: st.y

    ON_BOOL32 is_closed = (type < 2 ? surfA : surfB)->IsClosed(type % 2);
    const ON_Interval domain = (type < 2 ? surfA : surfB)->Domain(type % 2);
    if (!is_closed) {
	return;
    }

    int count = curvept.Count();
    for (int i = 0; i < count; i++) {
	ON_3dPoint pt3d = ON_3dPoint::UnsetPoint;
	ON_2dPoint ptuv, ptst;
	ptuv = curveuv[i];
	ptst = curvest[i];
	double &to_compare = (type < 2 ? ptuv : ptst)[type % 2];
	if (ON_NearZero(to_compare - domain.Min())) {
	    pt3d = curvept[i];
	    to_compare = domain.Max();
	} else if (ON_NearZero(to_compare - domain.Max())) {
	    pt3d = curvept[i];
	    to_compare = domain.Min();
	}
	if (!pt3d.IsUnsetPoint()) {
	    curvept.Append(pt3d);
	    curveuv.Append(ptuv);
	    curvest.Append(ptst);
	}
    }
}


HIDDEN bool
newton_ssi(double &u, double &v, double &s, double &t, const ON_Surface *surfA, const ON_Surface *surfB, double intersection_tolerance)
{
    ON_3dPoint pointA = surfA->PointAt(u, v);
    ON_3dPoint pointB = surfB->PointAt(s, t);
    if (pointA.DistanceTo(pointB) < intersection_tolerance) {
	return true;
    }

    // Equations:
    //   x_a(u,v) - x_b(s,t) = 0
    //   y_a(u,v) - y_b(s,t) = 0
    //   z_a(u,v) - z_b(s,t) = 0
    // It's an under-determined system. We use Moore-Penrose pseudoinverse:
    // pinv(A) = transpose(A) * inv(A * transpose(A)) (A is a 3x4 Jacobian)
    // A * pinv(A) = I_3
    double last_u = DBL_MAX / 4, last_v = DBL_MAX / 4, last_s = DBL_MAX / 4, last_t = DBL_MAX / 4;

    int iteration = 0;
    while (fabs(last_u - u) + fabs(last_v - v) + fabs(last_s - s) + fabs(last_t - t) > ON_ZERO_TOLERANCE
	   && iteration++ < SSI_MAX_ITERATIONS) {
	last_u = u, last_v = v, last_s = s, last_t = t;
	ON_3dVector deriv_u, deriv_v, deriv_s, deriv_t;
	surfA->Ev1Der(u, v, pointA, deriv_u, deriv_v);
	surfB->Ev1Der(s, t, pointB, deriv_s, deriv_t);
	ON_Matrix J(3, 4), F(3, 1);
	for (int i = 0; i < 3; i++) {
	    J[i][0] = deriv_u[i];
	    J[i][1] = deriv_v[i];
	    J[i][2] = -deriv_s[i];
	    J[i][3] = -deriv_t[i];
	    F[i][0] = pointA[i] - pointB[i];
	}
	ON_Matrix tranJ = J;
	tranJ.Transpose();
	ON_Matrix JotranJ;
	JotranJ.Multiply(J, tranJ);
	if (!JotranJ.Invert(0.0)) {
	    break;
	}
	ON_Matrix pinvJ;
	pinvJ.Multiply(tranJ, JotranJ);
	ON_Matrix Delta;
	Delta.Multiply(pinvJ, F);
	u -= Delta[0][0];
	v -= Delta[1][0];
	s -= Delta[2][0];
	t -= Delta[3][0];
    }

    // Make sure the solution is inside the domains
    u = std::min(u, surfA->Domain(0).Max());
    u = std::max(u, surfA->Domain(0).Min());
    v = std::min(v, surfA->Domain(1).Max());
    v = std::max(v, surfA->Domain(1).Min());
    s = std::min(s, surfB->Domain(0).Max());
    s = std::max(s, surfB->Domain(0).Min());
    t = std::min(t, surfB->Domain(1).Max());
    t = std::max(t, surfB->Domain(1).Min());

    pointA = surfA->PointAt(u, v);
    pointB = surfB->PointAt(s, t);
    return (pointA.DistanceTo(pointB) < intersection_tolerance) && !isnan(u) && !isnan(v) && !isnan(s) & !isnan(t);
}


HIDDEN ON_Curve *
link_curves(ON_Curve *&c1, ON_Curve *&c2)
{
    ON_NurbsCurve *nc1 = c1->NurbsCurve();
    ON_NurbsCurve *nc2 = c2->NurbsCurve();
    if (nc1 && nc2) {
	nc1->Append(*nc2);
	delete c1;
	delete c2;
	c1 = NULL;
	c2 = NULL;
	delete nc2;
	return nc1;
    } else if (nc1) {
	delete nc1;
    } else if (nc2) {
	delete nc2;
    }
    return NULL;
}


struct OverlapSegment {
    ON_Curve *m_curve3d, *m_curveA, *m_curveB;

    int m_from;	    // 0: iso-curve from A, 1: from B
    int m_dir;	    // 0: u iso-curve, 1: v iso-curve
    double m_fix;   // the param fixed in the iso-curve
    // m_dir==0: m_fix==u; m_dir==1: m_fix==v
    double m_min;   // minimum of the variable param in the iso-curve
    double m_max;   // maximum of the variable param in the iso-curve

    ON_2dPoint Get2DParam(double t)
    {
	return m_dir ? ON_2dPoint(t, m_fix) : ON_2dPoint(m_fix, t);
    }

    void SetCurvesToNull()
    {
	m_curve3d = m_curveA = m_curveB = NULL;
    }

    ~OverlapSegment()
    {
	if (m_curve3d) {
	    delete m_curve3d;
	}
	m_curve3d = NULL;
	if (m_curveA) {
	    delete m_curveA;
	}
	m_curveA = NULL;
	if (m_curveB) {
	    delete m_curveB;
	}
	m_curveB = NULL;
    }
};


struct OverlapEvent {
    ON_SSX_EVENT *m_event;  // Use pointer to the ON_SSX_EVENT in case that
    // ~ON_SSX_EVENT() is called.
    ON_BoundingBox m_bboxA; // 2D surfA bounding box of the closed region.
    enum TYPE {
	undefined = 0,
	outer = 1,
	inner = 2
    } m_type;

    std::vector<OverlapEvent *> m_inside;

    OverlapEvent() {}

    OverlapEvent(ON_SSX_EVENT *_e) : m_event(_e)
    {
	m_type = undefined;
	m_bboxA = _e->m_curveA->BoundingBox();
    }

    bool IsPointOnBoundary(const ON_2dPoint &_pt)
    {
	ON_ClassArray<ON_PX_EVENT> px_event;
	return ON_Intersect(ON_3dPoint(_pt), *(m_event->m_curveA), px_event);
    }

    // Test whether a point is inside the closed region (including boundary).
    // Approach: Use line-curve intersections. If the line between this point
    // and a point outside the b-box has an odd number of intersections with
    // the boundary, it's considered inside, otherwise outside. (Tangent
    // intersection are counted 2.)
    bool IsPointIn(const ON_2dPoint &_pt)
    {
	if (!m_bboxA.IsPointIn(_pt)) {
	    return false;
	}
	if (IsPointOnBoundary(_pt)) {
	    return true;
	}
	ON_ClassArray<ON_PX_EVENT> px_event;
	// This point must be outside the closed region.
	ON_2dPoint out = _pt + ON_2dVector(m_bboxA.Diagonal());
	ON_LineCurve linecurve(_pt, out);
	ON_SimpleArray<ON_X_EVENT> x_event;
	ON_Intersect(&linecurve, m_event->m_curveA, x_event);
	int count = x_event.Count();
	for (int i = 0; i < x_event.Count(); i++) {
	    // Find tangent intersections.
	    // What should we do if it's ccx_overlap?
	    if (m_event->m_curveA->TangentAt(x_event[i].m_a[0]).IsParallelTo(linecurve.m_line.Direction())) {
		count++;
	    }
	}
	return count % 2 ? true : false;
    }

    // Return false if the point is on the boundary.
    bool IsPointOut(const ON_2dPoint &_pt)
    {
	return !IsPointIn(_pt);
    }

    // Test whether there are intersections between the box boundaries
    // and the boundary curve.
    // If there are not intersection, there can be 3 cases:
    // 1) The box is completely in.
    // 2) The box is completely out.
    // 3) The box completely contains the closed region bounded by that curve.
    bool IsBoxIntersected(const ON_2dPoint &_min, const ON_2dPoint &_max)
    {
	ON_2dPoint corner[4] = {
	    _min,
	    ON_2dPoint(_max.x, _min.y),
	    _max,
	    ON_2dPoint(_min.x, _max.y)
	};
	ON_LineCurve linecurve[4] = {
	    ON_LineCurve(corner[0], corner[1]),
	    ON_LineCurve(corner[1], corner[2]),
	    ON_LineCurve(corner[2], corner[3]),
	    ON_LineCurve(corner[3], corner[0])
	};
	ON_SimpleArray<ON_X_EVENT> x_event[4];
	for (int i = 0; i < 4; i++) {
	    if (ON_Intersect(&linecurve[i], m_event->m_curveA, x_event[i])) {
		return true;
	    }
	}
	return false;
    }

    bool IsBoxCompletelyIn(const ON_2dPoint &_min, const ON_2dPoint &_max)
    {
	// There should not be intersections between the box boundaries
	// and the boundary curve.
	if (IsBoxIntersected(_min, _max)) {
	    return false;
	}
	ON_2dPoint center = (_min + _max) * 0.5;
	return m_bboxA.Includes(ON_BoundingBox(_min, _max)) && IsPointIn(center);
    }

    bool IsBoxCompletelyOut(const ON_2dPoint &_min, const ON_2dPoint &_max)
    {
	if (IsBoxIntersected(_min, _max)) {
	    return false;
	}
	ON_2dPoint center = (_min + _max) * 0.5;
	return !IsPointIn(center) && !ON_BoundingBox(_min, _max).Includes(m_bboxA);
    }

    bool IsCurveCompletelyIn(const ON_Curve *_curve)
    {
	ON_SimpleArray<ON_X_EVENT> x_event;
	return !ON_Intersect(m_event->m_curve3d, _curve, x_event) && IsPointIn(_curve->PointAtStart());
    }
};


int
ON_Intersect(const ON_Surface *surfA,
	     const ON_Surface *surfB,
	     ON_ClassArray<ON_SSX_EVENT> &x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     double fitting_tolerance,
	     const ON_Interval *surfaceA_udomain,
	     const ON_Interval *surfaceA_vdomain,
	     const ON_Interval *surfaceB_udomain,
	     const ON_Interval *surfaceB_vdomain,
	     Subsurface *treeA,
	     Subsurface *treeB)
{
    if (surfA == NULL || surfB == NULL) {
	return 0;
    }

    int original_count = x.Count();

    if (intersection_tolerance <= 0.0) {
	intersection_tolerance = SSI_DEFAULT_TOLERANCE;
    }
    if (overlap_tolerance < intersection_tolerance) {
	overlap_tolerance = 2.0 * intersection_tolerance;
    }
    if (fitting_tolerance < intersection_tolerance) {
	fitting_tolerance = intersection_tolerance;
    }

    check_domain(surfaceA_udomain, surfA->Domain(0), "surfaceA_udomain");
    check_domain(surfaceA_vdomain, surfA->Domain(1), "surfaceA_vdomain");
    check_domain(surfaceB_udomain, surfB->Domain(0), "surfaceB_udomain");
    check_domain(surfaceB_vdomain, surfB->Domain(1), "surfaceB_vdomain");

    /* First step: Initialize the first two Subsurface.
     * It's just like getting the root of the surface tree.
     */
    typedef std::vector<std::pair<Subsurface *, Subsurface *> > NodePairs;
    NodePairs candidates, next_candidates;
    Subsurface *rootA, *rootB;
    if (treeA == NULL) {
	rootA = new Subsurface;
	if (rootA == NULL) {
	    return 0;
	}
	if (!build_surface_root(surfA, surfaceA_udomain, surfaceA_vdomain, *rootA)) {
	    delete rootA;
	}
    } else {
	rootA = treeA;
    }

    if (treeB == NULL) {
	rootB = new Subsurface;
	if (rootB == NULL) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    return 0;
	}
	if (!build_surface_root(surfB, surfaceB_udomain, surfaceB_vdomain, *rootB)) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    delete rootB;
	}
    } else {
	rootB = treeB;
    }

    if (rootA->Intersect(*rootB, intersection_tolerance)) {
	candidates.push_back(std::make_pair(rootA, rootB));
    } else {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return 0;
    }

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double intersection_tolerance_A = intersection_tolerance;
    double intersection_tolerance_B = intersection_tolerance;
    double fitting_tolerance_A = fitting_tolerance;
    double fitting_tolerance_B = fitting_tolerance;
    double l = rootA->m_surf->BoundingBox().Diagonal().Length();
    double ul = surfaceA_udomain ? surfaceA_udomain->Length() : surfA->Domain(0).Length();
    double vl = surfaceA_vdomain ? surfaceA_vdomain->Length() : surfA->Domain(1).Length();
    double dl = ON_2dVector(ul, vl).Length();
    if (!ON_NearZero(l)) {
	intersection_tolerance_A = intersection_tolerance / l * dl;
	fitting_tolerance_A = fitting_tolerance / l * dl;
    }
    ul = surfaceB_udomain ? surfaceB_udomain->Length() : surfB->Domain(0).Length();
    vl = surfaceB_vdomain ? surfaceB_vdomain->Length() : surfB->Domain(1).Length();
    dl = ON_2dVector(ul, vl).Length();
    if (!ON_NearZero(l)) {
	intersection_tolerance_B = intersection_tolerance / l * dl;
	fitting_tolerance_B = fitting_tolerance / l * dl;
    }

    ON_3dPointArray curvept, tmp_curvept;
    ON_2dPointArray curveuv, curvest, tmp_curveuv, tmp_curvest;

    // Overlap detection:
    // According to the Theorem 3 in paper:
    // http://libgen.org/scimag1/10.1016/S0010-4485%252896%252900099-1.pdf
    // For two Bezier patches, if they overlap over a region, then the overlap
    // region must be bounded by parts of the boundaries of the two patches.
    // The Bezier patches for a NURBS surface are always bounded at the knots.
    // In other words, given two pairs of neighbor knots (in two dimensions),
    // we can get a Bezier patch between them.
    // (See ON_NurbsSurface::ConvertSpanToBezier()).
    // So we actually don't need to generate the Bezier patches explicitly,
    // and we can get the boundaries of them using IsoCurve() on knots.
    // Deal with boundaries with curve-surface intersections.

    ON_SimpleArray<OverlapSegment *> overlaps;
    for (int i = 0; i < 4; i++) {
	const ON_Surface *surf1 = i >= 2 ? surfB : surfA;
	const ON_Surface *surf2 = i >= 2 ? surfA : surfB;
	Subsurface *tree = i >= 2 ? treeA : treeB;
	ON_2dPointArray &ptarray1 = i >= 2 ? tmp_curvest : tmp_curveuv;
	ON_2dPointArray &ptarray2 = i >= 2 ? tmp_curveuv : tmp_curvest;
	int dir = 1 - i % 2;
	int knotcnt = surf1->SpanCount(dir);
	double *knots = new double [knotcnt + 1];
	surf1->GetSpanVector(dir, knots);
	// knots that can be boundaries of Bezier patches
	ON_SimpleArray<double> b_knots;
	b_knots.Append(knots[0]);
	for (int j = 1; j <= knotcnt; j++) {
	    if (knots[j] > *(b_knots.Last())) {
		b_knots.Append(knots[j]);
	    }
	}
	if (surf1->IsClosed(dir)) {
	    b_knots.Remove();
	}

	for (int j = 0; j < b_knots.Count(); j++) {
	    ON_Curve *boundary = surf1->IsoCurve(i % 2, b_knots[j]);
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_CurveArray overlap2d;
	    ON_Intersect(boundary, surf2, x_event, intersection_tolerance, overlap_tolerance, 0, 0, 0, &overlap2d);
	    for (int k = 0; k < x_event.Count(); k++) {
		tmp_curvept.Append(x_event[k].m_A[0]);
		ON_2dPoint iso_pt1, iso_pt2;
		iso_pt1.x = i % 2 ? b_knots[j] : x_event[k].m_a[0];
		iso_pt1.y = i % 2 ? x_event[k].m_a[0] : b_knots[j];
		ptarray1.Append(iso_pt1);
		ptarray2.Append(ON_2dPoint(x_event[k].m_b[0], x_event[k].m_b[1]));
		if (x_event[k].m_type == ON_X_EVENT::csx_overlap) {
		    // Append the other end-point
		    tmp_curvept.Append(x_event[k].m_A[1]);
		    iso_pt2.x = i % 2 ? b_knots[j] : x_event[k].m_a[1];
		    iso_pt2.y = i % 2 ? x_event[k].m_a[1] : b_knots[j];
		    ptarray1.Append(iso_pt2);
		    ptarray2.Append(ON_2dPoint(x_event[k].m_b[2], x_event[k].m_b[3]));
		    // Get the overlap curve
		    if (x_event[k].m_a[0] < x_event[k].m_a[1]) {
			// Check whether it's an inner curve segment, that is,
			// this curve is completely inside the overlap region,
			// not part of the boundary curve.
			// We test whether the two sides of that curve are shared.
			// For the boundary of whole surface, this test is not needed.
			bool isvalid = false;
			if (j == 0 || (j == b_knots.Count() - 1 && !surf1->IsClosed(dir))) {
			    isvalid = true;
			} else {
			    double test_distance = 0.01;
			    // TODO: more sample points
			    double U1 = i % 2 ? b_knots[j] - test_distance : (x_event[k].m_a[0] + x_event[k].m_a[1]) * 0.5;
			    double V1 = i % 2 ? (x_event[k].m_a[0] + x_event[k].m_a[1]) * 0.5 : b_knots[j] - test_distance;
			    double U2 = i % 2 ? b_knots[j] + test_distance : (x_event[k].m_a[0] + x_event[k].m_a[1]) * 0.5;
			    double V2 = i % 2 ? (x_event[k].m_a[0] + x_event[k].m_a[1]) * 0.5 : b_knots[j] + test_distance;
			    bool in1, in2;
			    ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
			    in1 = ON_Intersect(surf1->PointAt(U1, V1), *surf2, px_event1, 1e-5, 0, 0, tree)
				  && surf1->NormalAt(U1, V1).IsParallelTo(surf2->NormalAt(px_event1[0].m_b[0], px_event1[0].m_b[1]));
			    in2 = ON_Intersect(surf1->PointAt(U2, V2), *surf2, px_event2, 1e-5, 0, 0, tree)
				  && surf1->NormalAt(U2, V2).IsParallelTo(surf2->NormalAt(px_event2[0].m_b[0], px_event2[0].m_b[1]));
			    if ((in1 && !in2) || (!in1 && in2)) {
				isvalid = true;
			    }
			}
			if (isvalid) {
			    // One side of it is shared and the other side is non-shared.
			    OverlapSegment *seg = new OverlapSegment;
			    seg->m_curve3d = sub_curve(boundary, x_event[k].m_a[0], x_event[k].m_a[1]);
			    if (i < 2) {
				seg->m_curveA = new ON_LineCurve(iso_pt1, iso_pt2);
				seg->m_curveB = overlap2d[k];
			    } else {
				seg->m_curveB = new ON_LineCurve(iso_pt1, iso_pt2);
				seg->m_curveA = overlap2d[k];
			    }
			    seg->m_dir = dir;
			    seg->m_fix = b_knots[j];
			    overlaps.Append(seg);
			    if (j == 0 && surf1->IsClosed(dir)) {
				// something like close_domain().
				// If the domain is closed, the iso-curve on the
				// first knot and the last knot is the same, so
				// we don't need to compute the intersections twice.
				seg = new OverlapSegment;
				iso_pt1.x = i % 2 ? knots[knotcnt] : x_event[k].m_a[0];
				iso_pt1.y = i % 2 ? x_event[k].m_a[0] : knots[knotcnt];
				iso_pt2.x = i % 2 ? knots[knotcnt] : x_event[k].m_a[1];
				iso_pt2.y = i % 2 ? x_event[k].m_a[1] : knots[knotcnt];
				seg->m_curve3d = (*overlaps.Last())->m_curve3d->Duplicate();
				if (i < 2) {
				    seg->m_curveA = new ON_LineCurve(iso_pt1, iso_pt2);
				    seg->m_curveB = overlap2d[k]->Duplicate();
				} else {
				    seg->m_curveB = new ON_LineCurve(iso_pt1, iso_pt2);
				    seg->m_curveA = overlap2d[k]->Duplicate();
				}
				seg->m_dir = dir;
				seg->m_fix = knots[knotcnt];
				overlaps.Append(seg);
			    }
			    // We set overlap2d[k] to NULL, is case that the curve
			    // is delete by the destructor of overlap2d. (See ~ON_CurveArray())
			    overlap2d[k] = NULL;
			}
		    }
		}
	    }
	    delete boundary;
	}
	delete []knots;
    }

    int count_before = overlaps.Count();
    // Not points, but a bundle of params:
    // params[i] - the separating params on overlaps[i]
    // x - curve3d, y - curveA, z - curveB

    ON_ClassArray<ON_3dPointArray> params(count_before);
    for (int i = 0; i < count_before; i++) {
	// Split the curves from the intersection points between them.
	int count = params[i].Count();
	for (int j = i + 1; j < count_before; j++) {
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d, x_event, intersection_tolerance);
	    count += x_event.Count();
	    for (int k = 0; k < x_event.Count(); k++) {
		ON_3dPoint param;
		ON_ClassArray<ON_PX_EVENT> e1, e2, e3, e4;
		ON_2dPoint uv;
		ON_2dPoint st;
		ON_ClassArray<ON_PX_EVENT> pe1, pe2;
		if (ON_Intersect(x_event[k].m_A[0], *surfA, pe1, intersection_tolerance_A, 0, 0, treeA)
		    && ON_Intersect(x_event[k].m_B[0], *surfB, pe2, intersection_tolerance_B, 0, 0, treeB)) {
		    // Pull the 3D curve back to the 2D space
		    uv = pe1[0].m_b;
		    st = pe2[0].m_b;
		    if (ON_Intersect(uv, *(overlaps[i]->m_curveA), e1, intersection_tolerance_A)
			&& ON_Intersect(st, *(overlaps[i]->m_curveB), e2, intersection_tolerance_B)) {
			param.x = x_event[k].m_a[0];
			param.y = e1[0].m_b[0];
			param.z = e2[0].m_b[0];
			params[i].Append(param);
		    }
		    if (ON_Intersect(uv, *(overlaps[j]->m_curveA), e3, intersection_tolerance_A)
			&& ON_Intersect(st, *(overlaps[j]->m_curveB), e4, intersection_tolerance_B)) {
			// The same routine for overlaps[j]
			param.x = x_event[k].m_b[0];
			param.y = e3[0].m_b[0];
			param.z = e4[0].m_b[0];
			params[j].Append(param);
		    }
		}
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap) {
		    ON_ClassArray<ON_PX_EVENT> psx3, psx4, e5, e6, e7, e8;
		    if (ON_Intersect(x_event[k].m_A[0], *surfA, psx3, intersection_tolerance, 0, 0, treeA)
			&& ON_Intersect(x_event[k].m_B[0], *surfB, psx4, intersection_tolerance, 0, 0, treeB)) {
			// Pull the 3D curve back to the 2D space
			uv = psx3[0].m_b;
			st = psx4[0].m_b;
			if (ON_Intersect(x_event[k].m_A[1], *(overlaps[i]->m_curveA), e5, intersection_tolerance_A)
			    && ON_Intersect(x_event[k].m_A[1], *(overlaps[i]->m_curveB), e6, intersection_tolerance_B)) {
			    param.x = x_event[k].m_a[1];
			    param.y = e5[0].m_b[0];
			    param.z = e6[0].m_b[0];
			    params[i].Append(param);
			}
			if (ON_Intersect(x_event[k].m_B[1], *(overlaps[j]->m_curveA), e7, intersection_tolerance_A)
			    && ON_Intersect(x_event[k].m_B[1], *(overlaps[j]->m_curveB), e8, intersection_tolerance_B)) {
			    // The same routine for overlaps[j]
			    param.x = x_event[k].m_b[1];
			    param.y = e7[0].m_b[0];
			    param.z = e8[0].m_b[0];
			    params[j].Append(param);
			}
		    }
		}
	    }
	}
    }

    for (int i = 0; i < count_before; i++) {
	params[i].QuickSort(ON_CompareIncreasing);
	int start = 0;
	bool splitted = false;
	for (int j = 1; j < params[i].Count(); j++) {
	    if (params[i][j].x - params[i][start].x < intersection_tolerance) {
		continue;
	    }
	    ON_Curve *subcurveA = sub_curve(overlaps[i]->m_curveA, params[i][start].y, params[i][j].y);
	    if (subcurveA == NULL) {
		continue;
	    }
	    bool isvalid = false, isreversed = false;
	    double test_distance = 0.01;
	    // TODO: more sample points
	    ON_2dPoint UV1, UV2;
	    UV1 = UV2 = subcurveA->PointAt(subcurveA->Domain().Mid());
	    ON_3dVector normal = ON_CrossProduct(subcurveA->TangentAt(subcurveA->Domain().Mid()), ON_3dVector::ZAxis);
	    normal.Unitize();
	    UV1 -= normal * test_distance;	// left
	    UV2 += normal * test_distance;	// right
	    bool in1, in2;
	    ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
	    in1 = surfA->Domain(0).Includes(UV1.x)
		  && surfA->Domain(1).Includes(UV1.y)
		  && ON_Intersect(surfA->PointAt(UV1.x, UV1.y), *surfB, px_event1, 1e-5, 0, 0, treeB)
		  && surfA->NormalAt(UV1.x, UV1.y).IsParallelTo(surfB->NormalAt(px_event1[0].m_b[0], px_event1[0].m_b[1]));
	    in2 = surfA->Domain(0).Includes(UV2.x)
		  && surfA->Domain(1).Includes(UV2.y)
		  && ON_Intersect(surfA->PointAt(UV2.x, UV2.y), *surfB, px_event2, 1e-5, 0, 0, treeB)
		  && surfA->NormalAt(UV2.x, UV2.y).IsParallelTo(surfB->NormalAt(px_event2[0].m_b[0], px_event2[0].m_b[1]));
	    if (in1 && !in2) {
		isvalid = true;
	    } else if (!in1 && in2) {
		// the right side is overlapped.
		isvalid = true;
		isreversed = true;
	    }
	    if (isvalid) {
		OverlapSegment *seg = new OverlapSegment;
		seg->m_curve3d = sub_curve(overlaps[i]->m_curve3d, params[i][start].x, params[i][j].x);
		seg->m_curveA = subcurveA;
		seg->m_curveB = sub_curve(overlaps[i]->m_curveB, params[i][start].z, params[i][j].z);
		if (isreversed) {
		    seg->m_curve3d->Reverse();
		    seg->m_curveA->Reverse();
		    seg->m_curveB->Reverse();
		}
		seg->m_dir = overlaps[i]->m_dir;
		seg->m_fix = overlaps[i]->m_fix;
		overlaps.Append(seg);
	    }
	    start = j;
	    splitted = true;
	}
	if (splitted) {
	    delete overlaps[i];
	    overlaps[i] = NULL;
	}
    }

    for (int i = 0; i < overlaps.Count(); i++) {
	for (int j = i + 1; j < overlaps.Count(); j++) {
	    if (!overlaps[i] || !overlaps[i]->m_curve3d || !overlaps[i]->m_curveA || !overlaps[i]->m_curveB || !overlaps[j] || !overlaps[j]->m_curve3d || !overlaps[i]->m_curveA || !overlaps[i]->m_curveB) {
		continue;
	    }
	    // Eliminate duplications.
	    ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
	    if (ON_Intersect(overlaps[i]->m_curveA, overlaps[j]->m_curveA, x_event1, intersection_tolerance_A)
		&& ON_Intersect(overlaps[i]->m_curveB, overlaps[j]->m_curveB, x_event2, intersection_tolerance_B)) {
		if (x_event1[0].m_type == ON_X_EVENT::ccx_overlap
		    && x_event2[0].m_type == ON_X_EVENT::ccx_overlap) {
		    if (ON_NearZero(x_event1[0].m_a[0] - overlaps[i]->m_curveA->Domain().Min())
			&& ON_NearZero(x_event1[0].m_a[1] - overlaps[i]->m_curveA->Domain().Max())
			&& ON_NearZero(x_event2[0].m_a[0] - overlaps[i]->m_curveB->Domain().Min())
			&& ON_NearZero(x_event2[0].m_a[1] - overlaps[i]->m_curveB->Domain().Max())) {
			// overlaps[i] is completely inside overlaps[j]
			delete overlaps[i];
			overlaps[i] = NULL;
		    } else if (ON_NearZero(x_event1[0].m_b[0] - overlaps[j]->m_curveA->Domain().Min())
			       && ON_NearZero(x_event1[0].m_b[1] - overlaps[j]->m_curveA->Domain().Max())
			       && ON_NearZero(x_event2[0].m_b[0] - overlaps[j]->m_curveB->Domain().Min())
			       && ON_NearZero(x_event2[0].m_b[1] - overlaps[j]->m_curveB->Domain().Max())) {
			// overlaps[j] is completely inside overlaps[i]
			delete overlaps[j];
			overlaps[j] = NULL;
		    }
		}
	    }
	}
    }

    // Find the neighbors for every overlap segment.
    ON_SimpleArray<bool> start_linked(overlaps.Count()), end_linked(overlaps.Count());
    for (int i = 0; i < overlaps.Count(); i++) {
	// Initialization
	start_linked[i] = end_linked[i] = false;
    }
    for (int i = 0; i < overlaps.Count(); i++) {
	if (!overlaps[i] || !overlaps[i]->m_curveA || !overlaps[i]->m_curveB || !overlaps[i]->m_curve3d) {
	    continue;
	}

	if (overlaps[i]->m_curve3d->IsClosed() && overlaps[i]->m_curveA->IsClosed() && overlaps[i]->m_curveB->IsClosed()) {
	    start_linked[i] = end_linked[i] = true;
	}

	for (int j = i + 1; j < overlaps.Count(); j++) {
	    if (!overlaps[j] || !overlaps[j]->m_curveA || !overlaps[j]->m_curveB || !overlaps[j]->m_curve3d) {
		continue;
	    }

	    // Check whether the start point and end point is linked to
	    // another curve.
	    if (overlaps[i]->m_curve3d->PointAtStart().DistanceTo(overlaps[j]->m_curve3d->PointAtEnd()) < intersection_tolerance
		&& overlaps[i]->m_curveA->PointAtStart().DistanceTo(overlaps[j]->m_curveA->PointAtEnd()) < intersection_tolerance_A
		&& overlaps[i]->m_curveB->PointAtStart().DistanceTo(overlaps[j]->m_curveB->PointAtEnd()) < intersection_tolerance_B) {
		// end -- start -- end -- start
		start_linked[i] = end_linked[j] = true;
	    } else if (overlaps[i]->m_curve3d->PointAtEnd().DistanceTo(overlaps[j]->m_curve3d->PointAtStart()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtEnd().DistanceTo(overlaps[j]->m_curveA->PointAtStart()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtEnd().DistanceTo(overlaps[j]->m_curveB->PointAtStart()) < intersection_tolerance_B) {
		// start -- end -- start -- end
		start_linked[j] = end_linked[i] = true;
	    } else if (overlaps[i]->m_curve3d->PointAtStart().DistanceTo(overlaps[j]->m_curve3d->PointAtStart()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtStart().DistanceTo(overlaps[j]->m_curveA->PointAtStart()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtStart().DistanceTo(overlaps[j]->m_curveB->PointAtStart()) < intersection_tolerance_B) {
		// end -- start -- start -- end
		start_linked[i] = start_linked[j] = true;
	    } else if (overlaps[i]->m_curve3d->PointAtEnd().DistanceTo(overlaps[j]->m_curve3d->PointAtEnd()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtEnd().DistanceTo(overlaps[j]->m_curveA->PointAtEnd()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtEnd().DistanceTo(overlaps[j]->m_curveB->PointAtEnd()) < intersection_tolerance_B) {
		// start -- end -- end -- start
		end_linked[i] = end_linked[j] = true;
	    }
	}
    }

    for (int i = 0; i < overlaps.Count(); i++) {
	if (!overlaps[i] || !overlaps[i]->m_curveA || !overlaps[i]->m_curveB || !overlaps[i]->m_curve3d) {
	    continue;
	}
	if (!start_linked[i] || !end_linked[i]) {
	    delete overlaps[i];
	    overlaps[i] = NULL;
	}
    }

    for (int i = 0; i < overlaps.Count(); i++) {
	if (!overlaps[i] || !overlaps[i]->m_curveA || !overlaps[i]->m_curveB || !overlaps[i]->m_curve3d) {
	    continue;
	}

	for (int j = 0; j <= overlaps.Count(); j++) {
	    if (overlaps[i]->m_curve3d->IsClosed() && overlaps[i]->m_curveA->IsClosed() && overlaps[i]->m_curveB->IsClosed()) {
		// The i-th curve is close loop, we get a complete boundary of
		// that overlap region.
		ON_SSX_EVENT Event;
		Event.m_curve3d = curve_fitting(overlaps[i]->m_curve3d, fitting_tolerance);
		Event.m_curveA = curve_fitting(overlaps[i]->m_curveA, fitting_tolerance_A);
		Event.m_curveB = curve_fitting(overlaps[i]->m_curveB, fitting_tolerance_B);
		Event.m_type = ON_SSX_EVENT::ssx_overlap;
		// Normalize the curves
		Event.m_curve3d->SetDomain(ON_Interval(0.0, 1.0));
		Event.m_curveA->SetDomain(ON_Interval(0.0, 1.0));
		Event.m_curveB->SetDomain(ON_Interval(0.0, 1.0));
		x.Append(Event);
		// Set the curves to NULL in case that they are deleted by
		// ~ON_SSX_EVENT() or ~ON_CurveArray().
		Event.m_curve3d = Event.m_curveA = Event.m_curveB = NULL;
		overlaps[i]->SetCurvesToNull();
		delete overlaps[i];
		overlaps[i] = NULL;
		break;
	    }

	    if (j == overlaps.Count() || j == i || !overlaps[j] || !overlaps[j]->m_curveA || !overlaps[j]->m_curveB || !overlaps[j]->m_curve3d) {
		continue;
	    }

	    // Merge the curves that link together.
	    if (overlaps[i]->m_curve3d->PointAtStart().DistanceTo(overlaps[j]->m_curve3d->PointAtEnd()) < intersection_tolerance
		&& overlaps[i]->m_curveA->PointAtStart().DistanceTo(overlaps[j]->m_curveA->PointAtEnd()) < intersection_tolerance_A
		&& overlaps[i]->m_curveB->PointAtStart().DistanceTo(overlaps[j]->m_curveB->PointAtEnd()) < intersection_tolerance_B) {
		// end -- start -- end -- start
		overlaps[i]->m_curve3d = link_curves(overlaps[j]->m_curve3d, overlaps[i]->m_curve3d);
		overlaps[i]->m_curveA = link_curves(overlaps[j]->m_curveA, overlaps[i]->m_curveA);
		overlaps[i]->m_curveB = link_curves(overlaps[j]->m_curveB, overlaps[i]->m_curveB);
	    } else if (overlaps[i]->m_curve3d->PointAtEnd().DistanceTo(overlaps[j]->m_curve3d->PointAtStart()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtEnd().DistanceTo(overlaps[j]->m_curveA->PointAtStart()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtEnd().DistanceTo(overlaps[j]->m_curveB->PointAtStart()) < intersection_tolerance_B) {
		// start -- end -- start -- end
		overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
	    } else if (overlaps[i]->m_curve3d->PointAtStart().DistanceTo(overlaps[j]->m_curve3d->PointAtStart()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtStart().DistanceTo(overlaps[j]->m_curveA->PointAtStart()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtStart().DistanceTo(overlaps[j]->m_curveB->PointAtStart()) < intersection_tolerance_B) {
		// end -- start -- start -- end
		if (overlaps[i]->m_curve3d->Reverse() && overlaps[i]->m_curveA->Reverse() && overlaps[i]->m_curveB->Reverse()) {
		    overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		    overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		    overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
		}
	    } else if (overlaps[i]->m_curve3d->PointAtEnd().DistanceTo(overlaps[j]->m_curve3d->PointAtEnd()) < intersection_tolerance
		       && overlaps[i]->m_curveA->PointAtEnd().DistanceTo(overlaps[j]->m_curveA->PointAtEnd()) < intersection_tolerance_A
		       && overlaps[i]->m_curveB->PointAtEnd().DistanceTo(overlaps[j]->m_curveB->PointAtEnd()) < intersection_tolerance_B) {
		// start -- end -- end -- start
		if (overlaps[j]->m_curve3d->Reverse() && overlaps[j]->m_curveA->Reverse() && overlaps[j]->m_curveB->Reverse()) {
		    overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		    overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		    overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
		}
	    }
	    if (!overlaps[j]->m_curve3d || !overlaps[j]->m_curveA || !overlaps[j]->m_curveB) {
		delete overlaps[j];
		overlaps[j] = NULL;
	    }
	}
    }

    // Generate OverlapEvents.
    ON_SimpleArray<OverlapEvent> overlapevents;
    for (int i = original_count; i < x.Count(); i++) {
	overlapevents.Append(OverlapEvent(&x[i]));
    }

    for (int i = original_count; i < x.Count(); i++) {
	// The overlap region should be to the LEFT of that *m_curveA*.
	// (See opennurbs/opennurbs_x.h)
	double midA = x[i].m_curveA->Domain().Mid();
	if (!x[i].m_curveA->IsContinuous(ON::G1_continuous, midA)) {
	    // using the middle point is not sufficient, we try another options.
	    midA = x[i].m_curveA->Domain().NormalizedParameterAt(1.0 / 3.0);
	    if (!x[i].m_curveA->IsContinuous(ON::G1_continuous, midA)) {
		midA = x[i].m_curveA->Domain().NormalizedParameterAt(2.0 / 3.0);
	    }
	}
	ON_3dVector normalA = ON_CrossProduct(ON_3dVector::ZAxis, x[i].m_curveA->TangentAt(midA));
	ON_3dPoint left_ptA, right_ptA, mid_ptA;
	mid_ptA = x[i].m_curveA->PointAt(midA);
	left_ptA = mid_ptA + normalA * (1 + x[i].m_curveA->BoundingBox().Diagonal().Length());
	right_ptA = mid_ptA - normalA * (1 + x[i].m_curveA->BoundingBox().Diagonal().Length());
	// should be outside the closed region
	ON_LineCurve linecurve1(mid_ptA, left_ptA), linecurve2(mid_ptA, right_ptA);
	ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
	ON_Intersect(&linecurve1, x[i].m_curveA, x_event1, intersection_tolerance);
	ON_Intersect(&linecurve2, x[i].m_curveA, x_event2, intersection_tolerance);
	std::vector<double> line_t;
	if (x_event1.Count() != 2 && x_event2.Count() == 2) {
	    // the direction of the curve should be opposite
	    x[i].m_curve3d->Reverse();
	    x[i].m_curveA->Reverse();
	    x[i].m_curveB->Reverse();
	    linecurve1 = linecurve2;
	    line_t.push_back(x_event2[0].m_a[0]);
	    line_t.push_back(x_event2[1].m_a[0]);
	} else if (x_event1.Count() == 2 && x_event2.Count() != 2) {
	    line_t.push_back(x_event1[0].m_a[0]);
	    line_t.push_back(x_event1[1].m_a[0]);
	} else {
	    bu_log("error: Cannot determine the correct direction of overlap event %d\n", i);
	    continue;
	}

	// We need to reverse the curve again, if it's an inner loop
	// (The overlap is outside the closed region)
	for (int j = original_count; j < x.Count(); j++) {
	    // intersect the line with other curves, in case that it may be
	    // other loops (inner loops) inside.
	    if (i == j) {
		continue;
	    }
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(&linecurve1, x[j].m_curveA, x_event, intersection_tolerance);
	    for (int k = 0; k < x_event.Count(); k++) {
		line_t.push_back(x_event[k].m_a[0]);
	    }
	}

	// Normalize all params in line_t
	for (unsigned int j = 0; j < line_t.size(); j++) {
	    line_t[j] = linecurve1.Domain().NormalizedParameterAt(line_t[j]);
	}

	std::sort(line_t.begin(), line_t.end());
	if (!ON_NearZero(line_t[0])) {
	    bu_log("Error: param 0 is not in line_t!\n");
	    continue;
	}

	// Get a test point inside the region
	ON_3dPoint test_pt2d = linecurve1.PointAt(line_t[1] * 0.5);
	ON_3dPoint test_pt = surfA->PointAt(test_pt2d.x, test_pt2d.y);
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (!ON_Intersect(test_pt, *surfB, px_event, overlap_tolerance, 0, 0, treeB)) {
	    // The test point is not overlapped.
	    x[i].m_curve3d->Reverse();
	    x[i].m_curveA->Reverse();
	    x[i].m_curveB->Reverse();
	    overlapevents[i].m_type = OverlapEvent::inner;
	} else {
	    // The test point inside that region is an overlap point.
	    overlapevents[i].m_type = OverlapEvent::outer;
	}
    }

    // Find the inner events inside each outer event.
    for (int i = 0; i < overlapevents.Count(); i++) {
	for (int j = 0; j < overlapevents.Count(); j++) {
	    if (i == j || overlapevents[i].m_type != OverlapEvent::outer || overlapevents[j].m_type != OverlapEvent::inner) {
		continue;
	    }
	    if (overlapevents[i].IsCurveCompletelyIn(overlapevents[j].m_event->m_curve3d)) {
		overlapevents[i].m_inside.push_back(&(overlapevents[j]));
	    }
	}
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d overlap events.\n", overlapevents.Count());
    }

    if (surfA->IsPlanar() && surfB->IsPlanar() && overlapevents.Count()) {
	return x.Count() - original_count;
    }

    /* Second step: calculate the intersection of the bounding boxes.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     */
    for (int h = 0; h <= MAX_SSI_DEPTH; h++) {
	if (candidates.empty()) {
	    break;
	}
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    // If the box is considered already belonging to the overlap
	    // regions, we don't need to further sub-divide it.
	    ON_2dPoint min2d(i->first->m_u.Min() + intersection_tolerance_A, i->first->m_v.Min() + intersection_tolerance_A);
	    ON_2dPoint max2d(i->first->m_u.Max() - intersection_tolerance_A, i->first->m_v.Max() - intersection_tolerance_A);
	    // A box is inside the overlap region, if and only if it is completely
	    // inside an outer loop, and outside all inner loops within the outer
	    // loop.
	    bool inside_overlap = false;
	    for (int j = 0; j < overlapevents.Count(); j++) {
		if (overlapevents[j].m_type == OverlapEvent::outer
		    && overlapevents[j].IsBoxCompletelyIn(min2d, max2d)) {
		    bool out_of_all_inner = true;
		    for (unsigned int k = 0; k < overlapevents[j].m_inside.size(); k++) {
			OverlapEvent *event = overlapevents[j].m_inside[k];
			if (event->m_type == OverlapEvent::inner
			    && !event->IsBoxCompletelyOut(min2d, max2d)) {
			    out_of_all_inner = false;
			    break;
			}
		    }
		    if (out_of_all_inner) {
			inside_overlap = true;
			break;
		    }
		}
	    }
	    if (inside_overlap) {
		// We only do this optimization of surfA, because node pairs
		// need both boxes from surfA and surfB, and eliminating one of
		// them is enough.
		continue;
	    }
	    std::vector<Subsurface *> splittedA, splittedB;
	    if ((*i).first->Split() != 0) {
		splittedA.push_back((*i).first);
	    } else {
		for (int j = 0; j < 4; j++) {
		    splittedA.push_back((*i).first->m_children[j]);
		}
	    }
	    if ((*i).second->Split() != 0) {
		splittedB.push_back((*i).second);
	    } else {
		for (int j = 0; j < 4; j++) {
		    splittedB.push_back((*i).second->m_children[j]);
		}
	    }
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedB[k]->Intersect(*splittedA[j], intersection_tolerance)) {
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
		    }
	}
	candidates = next_candidates;
    }
    if (DEBUG_BREP_INTERSECT) {
	bu_log("We get %lu intersection bounding boxes.\n", candidates.size());
    }

    /* Third step: get the intersection points using triangular approximation,
     * and then Newton iterations.
     */
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	// We have arrived at the bottom of the trees.
	// Get an estimate of the intersection point lying on the intersection curve

	// Get the corners of each surface sub-patch inside the bounding-box.
	ON_BoundingBox box_intersect;
	i->first->Intersect(*(i->second), intersection_tolerance, &box_intersect);
	ON_3dPoint cornerA[4], cornerB[4];
	double u_min = (*i).first->m_u.Min(), u_max = (*i).first->m_u.Max();
	double v_min = (*i).first->m_v.Min(), v_max = (*i).first->m_v.Max();
	double s_min = (*i).second->m_u.Min(), s_max = (*i).second->m_u.Max();
	double t_min = (*i).second->m_v.Min(), t_max = (*i).second->m_v.Max();
	cornerA[0] = surfA->PointAt(u_min, v_min);
	cornerA[1] = surfA->PointAt(u_min, v_max);
	cornerA[2] = surfA->PointAt(u_max, v_min);
	cornerA[3] = surfA->PointAt(u_max, v_max);
	cornerB[0] = surfB->PointAt(s_min, t_min);
	cornerB[1] = surfB->PointAt(s_min, t_max);
	cornerB[2] = surfB->PointAt(s_max, t_min);
	cornerB[3] = surfB->PointAt(s_max, t_max);

	/* We approximate each surface sub-patch inside the bounding-box with two
	 * triangles that share an edge.
	 * The intersection of the surface sub-patches is approximated as the
	 * intersection of triangles.
	 */
	struct Triangle triangle[4];
	triangle[0].CreateFromPoints(cornerA[0], cornerA[1], cornerA[2]);
	triangle[1].CreateFromPoints(cornerA[1], cornerA[2], cornerA[3]);
	triangle[2].CreateFromPoints(cornerB[0], cornerB[1], cornerB[2]);
	triangle[3].CreateFromPoints(cornerB[1], cornerB[2], cornerB[3]);
	bool is_intersect[4];
	ON_3dPoint intersect_center[4];
	is_intersect[0] = triangle_intersection(triangle[0], triangle[2], intersect_center[0]);
	is_intersect[1] = triangle_intersection(triangle[0], triangle[3], intersect_center[1]);
	is_intersect[2] = triangle_intersection(triangle[1], triangle[2], intersect_center[2]);
	is_intersect[3] = triangle_intersection(triangle[1], triangle[3], intersect_center[3]);

	// Calculate the intersection centers' uv (st) coordinates.
	ON_3dPoint bcoor[8];
	ON_2dPoint uv[4]/*surfA*/, st[4]/*surfB*/;
	if (is_intersect[0]) {
	    bcoor[0] = triangle[0].BarycentricCoordinate(intersect_center[0]);
	    bcoor[1] = triangle[2].BarycentricCoordinate(intersect_center[0]);
	    uv[0].x = bcoor[0].x * u_min + bcoor[0].y * u_min + bcoor[0].z * u_max;
	    uv[0].y = bcoor[0].x * v_min + bcoor[0].y * v_max + bcoor[0].z * v_min;
	    st[0].x = bcoor[1].x * s_min + bcoor[1].y * s_min + bcoor[1].z * s_max;
	    st[0].y = bcoor[1].x * t_min + bcoor[1].y * t_max + bcoor[1].z * t_min;
	}
	if (is_intersect[1]) {
	    bcoor[2] = triangle[0].BarycentricCoordinate(intersect_center[1]);
	    bcoor[3] = triangle[3].BarycentricCoordinate(intersect_center[1]);
	    uv[1].x = bcoor[2].x * u_min + bcoor[2].y * u_min + bcoor[2].z * u_max;
	    uv[1].y = bcoor[2].x * v_min + bcoor[2].y * v_max + bcoor[2].z * v_min;
	    st[1].x = bcoor[3].x * s_min + bcoor[3].y * s_max + bcoor[3].z * s_max;
	    st[1].y = bcoor[3].x * t_max + bcoor[3].y * t_min + bcoor[3].z * t_max;
	}
	if (is_intersect[2]) {
	    bcoor[4] = triangle[1].BarycentricCoordinate(intersect_center[2]);
	    bcoor[5] = triangle[2].BarycentricCoordinate(intersect_center[2]);
	    uv[2].x = bcoor[4].x * u_min + bcoor[4].y * u_max + bcoor[4].z * u_max;
	    uv[2].y = bcoor[4].x * v_max + bcoor[4].y * v_min + bcoor[4].z * v_max;
	    st[2].x = bcoor[5].x * s_min + bcoor[5].y * s_min + bcoor[5].z * s_max;
	    st[2].y = bcoor[5].x * t_min + bcoor[5].y * t_max + bcoor[5].z * t_min;
	}
	if (is_intersect[3]) {
	    bcoor[6] = triangle[1].BarycentricCoordinate(intersect_center[3]);
	    bcoor[7] = triangle[3].BarycentricCoordinate(intersect_center[3]);
	    uv[3].x = bcoor[6].x * u_min + bcoor[6].y * u_max + bcoor[6].z * u_max;
	    uv[3].y = bcoor[6].x * v_max + bcoor[6].y * v_min + bcoor[6].z * v_max;
	    st[3].x = bcoor[7].x * s_min + bcoor[7].y * s_max + bcoor[7].z * s_max;
	    st[3].y = bcoor[7].x * t_max + bcoor[7].y * t_min + bcoor[7].z * t_max;
	}

	// The centroid of these intersection centers is the
	// intersection point we want.
	int num_intersects = 0;
	ON_3dPoint average(0.0, 0.0, 0.0);
	ON_2dPoint avguv(0.0, 0.0), avgst(0.0, 0.0);
	for (int j = 0; j < 4; j++) {
	    if (is_intersect[j]) {
		average += intersect_center[j];
		avguv += uv[j];
		avgst += st[j];
		num_intersects++;
	    }
	}
	if (num_intersects) {
	    average /= num_intersects;
	    avguv /= num_intersects;
	    avgst /= num_intersects;
	    if (box_intersect.IsPointIn(average)) {
		// Use Newton iterations to get an accurate intersection.
		if (newton_ssi(avguv.x, avguv.y, avgst.x, avgst.y, surfA, surfB, intersection_tolerance)) {
		    average = surfA->PointAt(avguv.x, avguv.y);
		    tmp_curvept.Append(average);
		    tmp_curveuv.Append(avguv);
		    tmp_curvest.Append(avgst);
		}
	    }
	}
    }
    for (int i = 0; i < 4; i++) {
	closed_domain(i, surfA, surfB, tmp_curvept, tmp_curveuv, tmp_curvest);
    }

    // Use an O(n^2) naive approach to eliminate duplication
    for (int i = 0; i < tmp_curvept.Count(); i++) {
	int j;
	for (j = 0; j < curvept.Count(); j++)
	    if (tmp_curvept[i].DistanceTo(curvept[j]) < intersection_tolerance
		&& tmp_curveuv[i].DistanceTo(curveuv[j]) < intersection_tolerance_A
		&& tmp_curvest[i].DistanceTo(curvest[j]) < intersection_tolerance_B) {
		break;
	    }
	if (j == curvept.Count()) {
	    // Test whether this point belongs to the overlap regions.
	    bool inside_overlap = false;
	    for (int k = 0; k < overlapevents.Count(); k++) {
		if (overlapevents[k].m_type == OverlapEvent::outer
		    && overlapevents[k].IsPointIn(tmp_curveuv[i])) {
		    bool out_of_all_inner = true;
		    for (unsigned int m = 0; m < overlapevents[k].m_inside.size(); m++) {
			OverlapEvent *event = overlapevents[k].m_inside[m];
			if (event->m_type == OverlapEvent::inner
			    && event->IsPointIn(tmp_curveuv[i])
			    && !event->IsPointOnBoundary(tmp_curveuv[i])) {
			    out_of_all_inner = false;
			    break;
			}
		    }
		    if (out_of_all_inner) {
			inside_overlap = true;
			break;
		    }
		}
	    }
	    if (!inside_overlap) {
		curvept.Append(tmp_curvept[i]);
		curveuv.Append(tmp_curveuv[i]);
		curvest.Append(tmp_curvest[i]);
	    }
	}
    }
    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d points on the intersection curves.\n", curvept.Count());
    }

    if (!curvept.Count()) {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}

	// Should not return 0 as there might be overlap events.
	return x.Count() - original_count;
    }

    // Fourth step: Fit the points in curvept into NURBS curves.
    // Here we use polyline approximation.
    // TODO: Find a better fitting algorithm unless this is good enough.
    // Time complexity: O(n^2*log(n)), really slow when n is large.
    // (n is the number of points generated above)

    // We need to automatically generate a threshold.
    double max_dis = std::min(rootA->m_surf->BoundingBox().Diagonal().Length(), rootB->m_surf->BoundingBox().Diagonal().Length()) * 0.1;

    double max_dis_u = surfA->Domain(0).Length() * 0.05;
    double max_dis_v = surfA->Domain(1).Length() * 0.05;
    double max_dis_s = surfB->Domain(0).Length() * 0.05;
    double max_dis_t = surfB->Domain(1).Length() * 0.05;
    if (DEBUG_BREP_INTERSECT) {
	bu_log("max_dis: %f\n", max_dis);
	bu_log("max_dis_u: %f\n", max_dis_u);
	bu_log("max_dis_v: %f\n", max_dis_v);
	bu_log("max_dis_s: %f\n", max_dis_s);
	bu_log("max_dis_t: %f\n", max_dis_t);
    }
    // NOTE: More tests are needed to find a better threshold.

    std::vector<PointPair> ptpairs;
    for (int i = 0; i < curvept.Count(); i++) {
	for (int j = i + 1; j < curvept.Count(); j++) {
	    PointPair ppair;
	    ppair.distance3d = curvept[i].DistanceTo(curvept[j]);
	    ppair.distanceU = fabs(curveuv[i].x - curveuv[j].x);
	    ppair.distanceV = fabs(curveuv[i].y - curveuv[j].y);
	    ppair.distanceS = fabs(curvest[i].x - curvest[j].x);
	    ppair.distanceT = fabs(curvest[i].y - curvest[j].y);
	    ppair.tol = intersection_tolerance;
	    if (ppair.distanceU < max_dis_u && ppair.distanceV < max_dis_v && ppair.distanceS < max_dis_s && ppair.distanceT < max_dis_t && ppair.distance3d < max_dis) {
		ppair.indexA = i;
		ppair.indexB = j;
		ptpairs.push_back(ppair);
	    }
	}
    }
    std::sort(ptpairs.begin(), ptpairs.end());

    std::vector<ON_SimpleArray<int>*> polylines(curvept.Count());
    int *index = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");
    // index[i] = j means curvept[i] is a startpoint/endpoint of polylines[j]
    int *startpt = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");
    int *endpt = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");
    // index of startpoints and endpoints of polylines[i]

    // Initialize each polyline with only one point.
    for (int i = 0; i < curvept.Count(); i++) {
	ON_SimpleArray<int> *single = new ON_SimpleArray<int>();
	single->Append(i);
	polylines[i] = single;
	index[i] = i;
	startpt[i] = i;
	endpt[i] = i;
    }

    // Merge polylines with distance less than max_dis.
    for (unsigned int i = 0; i < ptpairs.size(); i++) {
	int index1 = index[ptpairs[i].indexA], index2 = index[ptpairs[i].indexB];
	if (index1 == -1 || index2 == -1) {
	    continue;
	}
	index[startpt[index1]] = index[endpt[index1]] = index1;
	index[startpt[index2]] = index[endpt[index2]] = index1;
	ON_SimpleArray<int> *line1 = polylines[index1];
	ON_SimpleArray<int> *line2 = polylines[index2];
	if (line1 != NULL && line2 != NULL && line1 != line2) {
	    ON_SimpleArray<int> *unionline = new ON_SimpleArray<int>();
	    if ((*line1)[0] == ptpairs[i].indexA) {
		if ((*line2)[0] == ptpairs[i].indexB) {
		    // Case 1: endA -- startA -- startB -- endB
		    line1->Reverse();
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = endpt[index1];
		    endpt[index1] = endpt[index2];
		} else {
		    // Case 2: startB -- endB -- startA -- endA
		    unionline->Append(line2->Count(), line2->Array());
		    unionline->Append(line1->Count(), line1->Array());
		    startpt[index1] = startpt[index2];
		    endpt[index1] = endpt[index1];
		}
	    } else {
		if ((*line2)[0] == ptpairs[i].indexB) {
		    // Case 3: startA -- endA -- startB -- endB
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = startpt[index1];
		    endpt[index1] = endpt[index2];
		} else {
		    // Case 4: start -- endA -- endB -- startB
		    unionline->Append(line1->Count(), line1->Array());
		    line2->Reverse();
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[index1] = startpt[index1];
		    endpt[index1] = startpt[index2];
		}
	    }
	    polylines[index1] = unionline;
	    polylines[index2] = NULL;
	    if (line1->Count() >= 2) {
		index[ptpairs[i].indexA] = -1;
	    }
	    if (line2->Count() >= 2) {
		index[ptpairs[i].indexB] = -1;
	    }
	    delete line1;
	    delete line2;
	}
    }

    // In some cases, the intersection curves will intersect with each other.
    // But with our merging mechanism, one point can only belong to one curve,
    // so if they need to share one intersection point, this cannot work
    // properly. So we need some "seaming" segments to make sure the inter-
    // section.
    unsigned int num_curves = polylines.size();
    for (unsigned int i = 0; i < num_curves; i++) {
	if (!polylines[i]) {
	    continue;
	}
	for (unsigned int j = i + 1; j < num_curves; j++) {
	    if (!polylines[j]) {
		continue;
	    }
	    int point_count1 = polylines[i]->Count();
	    int point_count2 = polylines[j]->Count();
	    PointPair pair;
	    pair.distance3d = DBL_MAX;
	    for (int k = 0; k < point_count1; k++) {
		for (int m = 0; m < point_count2; m++) {
		    // We use PointPair for comparison.
		    PointPair newpair;
		    int start = (*polylines[i])[k], end = (*polylines[j])[m];
		    newpair.distance3d = curvept[start].DistanceTo(curvept[end]);
		    newpair.distanceU = fabs(curveuv[start].x - curveuv[end].x);
		    newpair.distanceV = fabs(curveuv[start].y - curveuv[end].y);
		    newpair.distanceS = fabs(curvest[start].x - curvest[end].x);
		    newpair.distanceT = fabs(curvest[start].y - curvest[end].y);
		    newpair.tol = intersection_tolerance;
		    if (newpair.distanceU < max_dis_u && newpair.distanceV < max_dis_v && newpair.distanceS < max_dis_s && newpair.distanceT < max_dis_t) {
			if (newpair < pair) {
			    newpair.indexA = start;
			    newpair.indexB = end;
			    pair = newpair;
			}
		    }
		}
	    }
	    if (pair.distance3d < max_dis) {
		// Generate a seaming curve, if there is currently no intersections.
		// Use curve-curve intersections (There are too much computations...)
		// Is this really necessary?
		ON_3dPointArray ptarray1, ptarray2, ptarray3, ptarray4;
		for (int k = 0; k < polylines[i]->Count(); k++) {
		    ptarray1.Append(curveuv[(*polylines[i])[k]]);
		    ptarray3.Append(curvest[(*polylines[i])[k]]);
		}
		for (int k = 0; k < polylines[j]->Count(); k++) {
		    ptarray2.Append(curveuv[(*polylines[j])[k]]);
		    ptarray4.Append(curvest[(*polylines[j])[k]]);
		}
		ON_PolylineCurve polyA(ptarray1), polyB(ptarray2), polyC(ptarray3), polyD(ptarray4);
		ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
		if (ON_Intersect(&polyA, &polyB, x_event1, intersection_tolerance)
		    && ON_Intersect(&polyC, &polyD, x_event2, intersection_tolerance)) {
		    continue;
		}

		// If the seaming curve is continuous to one of polylines[i] and
		// polylines[j], we don't need to generate a new segment, just
		// merging them.
		if (pair.indexA == (*polylines[i])[point_count1 - 1]) {
		    polylines[i]->Append(pair.indexB);
		} else if (pair.indexB == (*polylines[i])[point_count1 - 1]) {
		    polylines[i]->Append(pair.indexA);
		} else if (pair.indexA == (*polylines[i])[0]) {
		    polylines[i]->Insert(0, pair.indexB);
		} else if (pair.indexB == (*polylines[i])[0]) {
		    polylines[i]->Insert(0, pair.indexA);
		} else if (pair.indexA == (*polylines[j])[point_count2 - 1]) {
		    polylines[j]->Append(pair.indexB);
		} else if (pair.indexB == (*polylines[j])[point_count2 - 1]) {
		    polylines[j]->Append(pair.indexA);
		} else if (pair.indexA == (*polylines[j])[0]) {
		    polylines[j]->Insert(0, pair.indexB);
		} else if (pair.indexB == (*polylines[j])[0]) {
		    polylines[j]->Insert(0, pair.indexA);
		} else {
		    ON_SimpleArray<int> *seam = new ON_SimpleArray<int>;
		    seam->Append(pair.indexA);
		    seam->Append(pair.indexB);
		    polylines.push_back(seam);
		}
	    }
	}
    }

    // Generate ON_Curves from the polylines.
    ON_SimpleArray<ON_Curve *> intersect3d, intersect_uv2d, intersect_st2d;
    ON_SimpleArray<int> single_pts;
    for (unsigned int i = 0; i < polylines.size(); i++) {
	if (polylines[i] != NULL) {
	    int startpoint = (*polylines[i])[0];
	    int endpoint = (*polylines[i])[polylines[i]->Count() - 1];

	    if (polylines[i]->Count() == 1) {
		// It's a single point
		single_pts.Append(startpoint);
		delete polylines[i];
		continue;
	    }

	    // The intersection curves in the 3d space
	    ON_3dPointArray ptarray;
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ptarray.Append(curvept[(*polylines[i])[j]]);
	    }
	    // If it form a loop in the 3D space (except seaming curves)
	    if (curvept[startpoint].DistanceTo(curvept[endpoint]) < max_dis && i < num_curves) {
		ptarray.Append(curvept[startpoint]);
	    }
	    ON_PolylineCurve *curve = new ON_PolylineCurve(ptarray);
	    intersect3d.Append(curve);

	    // The intersection curves in the 2d UV parameter space (surfA)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curveuv[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    // If it form a loop in the 2D space (happens rarely compared to 3D)
	    if (fabs(curveuv[startpoint].x - curveuv[endpoint].x) < max_dis_u
		&& fabs(curveuv[startpoint].y - curveuv[endpoint].y) < max_dis_v
		&& i < num_curves) {
		ON_2dPoint &pt2d = curveuv[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = new ON_PolylineCurve(ptarray);
	    curve->ChangeDimension(2);
	    intersect_uv2d.Append(curve_fitting(curve, fitting_tolerance_A));

	    // The intersection curves in the 2d UV parameter space (surfB)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curvest[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    // If it form a loop in the 2D space (happens rarely compared to 3D)
	    if (fabs(curvest[startpoint].x - curvest[endpoint].x) < max_dis_s
		&& fabs(curvest[startpoint].y - curvest[endpoint].y) < max_dis_t
		&& i < num_curves) {
		ON_2dPoint &pt2d = curvest[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = new ON_PolylineCurve(ptarray);
	    curve->ChangeDimension(2);
	    intersect_st2d.Append(curve_fitting(curve, fitting_tolerance_B));

	    delete polylines[i];
	}
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d curve segments and %d single points.\n", intersect3d.Count(), single_pts.Count());
    }
    bu_free(index, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");

    // Generate ON_SSX_EVENTs
    if (intersect3d.Count()) {
	for (int i = 0; i < intersect3d.Count(); i++) {
	    ON_SSX_EVENT Event;
	    Event.m_curve3d = intersect3d[i];
	    Event.m_curveA = intersect_uv2d[i];
	    Event.m_curveB = intersect_st2d[i];
	    // Normalize the curves, so that their domains are the same,
	    // which is required by ON_SSX_EVENT::IsValid().
	    Event.m_curve3d->SetDomain(ON_Interval(0.0, 1.0));
	    Event.m_curveA->SetDomain(ON_Interval(0.0, 1.0));
	    Event.m_curveB->SetDomain(ON_Interval(0.0, 1.0));
	    // Check if the intersection is ssx_tangent
	    // If the at all points on the curves are of the same direction
	    // or opposite direction, the intersection is considered tangent.
	    int count = std::min(Event.m_curveA->SpanCount(), Event.m_curveB->SpanCount());
	    int j;
	    for (j = 0; j <= count; j++) {
		ON_3dVector normalA, normalB;
		ON_3dPoint pointA = Event.m_curveA->PointAt((double)j / count);
		ON_3dPoint pointB = Event.m_curveB->PointAt((double)j / count);
		if (!(surfA->EvNormal(pointA.x, pointA.y, normalA)
		      && surfB->EvNormal(pointB.x, pointB.y, normalB)
		      && normalA.IsParallelTo(normalB))) {
		    break;
		}
	    }
	    if (j == count + 1) {
		// For ssx_transverse events, the 3d curve direction
		// agrees with SurfaceNormalB x SurfaceNormalA
		// For ssx_tangent events, the orientation is random.
		// (See opennurbs/opennurbs_x.h)
		ON_3dVector direction = Event.m_curve3d->TangentAt(0);
		ON_3dVector SurfaceNormalA = surfA->NormalAt(Event.m_curveA->PointAtStart().x, Event.m_curveA->PointAtStart().y);
		ON_3dVector SurfaceNormalB = surfB->NormalAt(Event.m_curveB->PointAtStart().x, Event.m_curveB->PointAtStart().y);
		if (ON_DotProduct(direction, ON_CrossProduct(SurfaceNormalB, SurfaceNormalA)) < 0) {
		    if (!(Event.m_curve3d->Reverse()
			  && Event.m_curveA->Reverse()
			  && Event.m_curveB->Reverse()))
			bu_log("warning: reverse failed. The direction of %d might be wrong.\n",
			       x.Count() - original_count);
		}
		Event.m_type = ON_SSX_EVENT::ssx_tangent;
	    } else {
		Event.m_type = ON_SSX_EVENT::ssx_transverse;
	    }
	    // ssx_overlap is handled above.
	    x.Append(Event);
	    // Set the curves to NULL in case that they are deleted by ~ON_SSX_EVENT()
	    Event.m_curve3d = Event.m_curveA = Event.m_curveB = NULL;
	}
    }

    for (int i = 0; i < single_pts.Count(); i++) {
	// Check if the single point is duplicated (point-curve intersection)
	int j;
	for (j = 0; j < intersect3d.Count(); j++) {
	    ON_ClassArray<ON_PX_EVENT> px_event;
	    if (ON_Intersect(curvept[single_pts[i]], *intersect3d[j], px_event)) {
		break;
	    }
	}
	if (j == intersect3d.Count()) {
	    ON_SSX_EVENT Event;
	    Event.m_point3d = curvept[single_pts[i]];
	    Event.m_pointA = curveuv[single_pts[i]];
	    Event.m_pointB = curvest[single_pts[i]];
	    // Check if the intersection is ssx_tangent_point
	    // If the tangent planes of them are coincident (the normals
	    // are of the same direction or opposite direction), the
	    // intersection is considered tangent.
	    ON_3dVector normalA, normalB;
	    if (surfA->EvNormal(Event.m_pointA.x, Event.m_pointA.y, normalA)
		&& surfB->EvNormal(Event.m_pointB.x, Event.m_pointB.y, normalB)
		&& normalA.IsParallelTo(normalB)) {
		Event.m_type = ON_SSX_EVENT::ssx_tangent_point;
	    } else {
		Event.m_type = ON_SSX_EVENT::ssx_transverse_point;
	    }
	    x.Append(Event);
	}
    }

    if (treeA == NULL) {
	delete rootA;
    }
    if (treeB == NULL) {
	delete rootB;
    }
    return x.Count() - original_count;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
