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

// The maximal depth for subdivision - trade-off between accuracy and
// performance.
#define NR_MAX_DEPTH 8
#define MAX_PCI_DEPTH NR_MAX_DEPTH
#define MAX_PSI_DEPTH NR_MAX_DEPTH
#define MAX_CCI_DEPTH NR_MAX_DEPTH
#define MAX_CSI_DEPTH NR_MAX_DEPTH
#define MAX_SSI_DEPTH NR_MAX_DEPTH


// We make the default tolerance for PSI the same as that of curve and
// surface intersections defined by openNURBS (see opennurbs_curve.h
// and opennurbs_surface.h).
#define NR_DEFAULT_TOLERANCE 0.001
#define PCI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define PSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define CCI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define CSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE
#define SSI_DEFAULT_TOLERANCE NR_DEFAULT_TOLERANCE

// Used to prevent an infinite loop in the unlikely event that we
// can't provide a good starting point for the Newton-Raphson
// Iteration.
#define NR_MAX_ITERATIONS 100
#define PCI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define PSI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define CCI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define CSI_MAX_ITERATIONS NR_MAX_ITERATIONS
#define SSI_MAX_ITERATIONS NR_MAX_ITERATIONS

class XEventProxy {
public:
    XEventProxy(ON_X_EVENT::TYPE type);

    void SetAPoint(const ON_3dPoint &pt);
    void SetAPoints(const ON_3dPoint &start, const ON_3dPoint &end);

    void SetBPoint(const ON_3dPoint &pt);
    void SetBPoints(const ON_3dPoint &start, const ON_3dPoint &end);

    void SetAOverlapRange(const ON_Interval &interval);
    void SetAOverlapRange(double start, double end);
    void SetAOverlapRange(double t);
    void SetACurveParameter(double t);

    void SetBOverlapRange(const ON_Interval &interval);
    void SetBOverlapRange(double start, double end);
    void SetBOverlapRange(double t);
    void SetBCurveParameter(double t);
    void SetBSurfaceParameter(double u, double v);
    void SetBSurfaceParameters(double u1, double v1, double u2, double v2);

    ON_X_EVENT Event(void);

private:
    ON_X_EVENT event;
};

XEventProxy::XEventProxy(ON_X_EVENT::TYPE type)
{
    event.m_type = type;
}

void
XEventProxy::SetAPoint(const ON_3dPoint &pt)
{
    event.m_A[0] = event.m_A[1] = pt;
}

void
XEventProxy::SetAPoints(const ON_3dPoint &start, const ON_3dPoint &end)
{
    event.m_A[0] = start;
    event.m_A[1] = end;
}

void
XEventProxy::SetBPoint(const ON_3dPoint &pt)
{
    event.m_B[0] = event.m_B[1] = pt;
}

void
XEventProxy::SetBPoints(const ON_3dPoint &start, const ON_3dPoint &end)
{
    event.m_B[0] = start;
    event.m_B[1] = end;
}

void
XEventProxy::SetAOverlapRange(const ON_Interval &interval)
{
    event.m_a[0] = interval.Min();
    event.m_a[1] = interval.Max();
}

void
XEventProxy::SetAOverlapRange(double start, double end)
{
    SetAOverlapRange(ON_Interval(start, end));
}

void
XEventProxy::SetAOverlapRange(double t)
{
    SetAOverlapRange(t, t);
}

void
XEventProxy::SetACurveParameter(double t)
{
    event.m_a[0] = event.m_a[1] = t;
}

void
XEventProxy::SetBOverlapRange(const ON_Interval &interval)
{
    event.m_b[0] = event.m_b[2] = interval.Min();
    event.m_b[1] = event.m_b[3] = interval.Max();
}

void
XEventProxy::SetBOverlapRange(double start, double end)
{
    SetBOverlapRange(ON_Interval(start, end));
}

void
XEventProxy::SetBOverlapRange(double t)
{
    SetBOverlapRange(t, t);
}

void
XEventProxy::SetBCurveParameter(double t)
{
    event.m_b[0] = event.m_b[1] = t;
}

void
XEventProxy::SetBSurfaceParameters(double u1, double v1, double u2, double v2)
{
    event.m_b[0] = u1;
    event.m_b[1] = v1;
    event.m_b[2] = u2;
    event.m_b[3] = v2;
}

void
XEventProxy::SetBSurfaceParameter(double u, double v)
{
    SetBSurfaceParameters(u, v, u, v);
}

ON_X_EVENT
XEventProxy::Event(void)
{
    return event;
}

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

HIDDEN Subcurve *
get_curve_root(
	const ON_Curve *curve,
	const ON_Interval *domain)
{
    Subcurve *root = new Subcurve;
    if (!build_curve_root(curve, domain, *root)) {
	delete root;
	root = NULL;
    }
    if (root == NULL) {
	throw std::bad_alloc();
    }
    return root;
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

HIDDEN Subsurface *
get_surface_root(
	const ON_Surface *surf,
	const ON_Interval *u_domain,
	const ON_Interval *v_domain)
{
    Subsurface *root = new Subsurface;
    if (!build_surface_root(surf, u_domain, v_domain, *root)) {
	delete root;
	root = NULL;
    }
    if (root == NULL) {
	throw std::bad_alloc();
    }
    return root;
}

HIDDEN ON_Curve *
curve_fitting(ON_Curve *in, double fitting_tol = ON_ZERO_TOLERANCE, bool delete_curve = true)
{
    // Fit a *2D* curve into line, arc, ellipse or other conic curves.

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
    if (in->IsLinear(fitting_tol)) {
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
    int knot_count = in->SpanCount();
    if (knot_count < fit_minimum_knots) {
	return in;
    }

    double *knots = new double [knot_count + 1];
    in->GetSpanVector(knots);

    // Sample 6 points along the curve, and call ON_GetConicEquationThrough6Points().
    double conic[6];
    double sample_pts[12];
    int plotres = in->IsClosed() ? 6 : 5;

    // The sample points should be knots (which are accurate if the curve
    // is a polyline curve).
    for (int i = 0; i < 6; i++) {
	double knot_t = knots[ON_Round((double)i / plotres * knot_count)];
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
	// an alternative solution is to use least squares fitting on all
	// points.
	if (ON_IsConicEquationAnEllipse(conic, ell_center, ell_A, ell_B, &ell_a, &ell_b)) {
	    // The conic equation is an ellipse. But since we only considered
	    // the 6 sampled points, now we need to check whether all the points
	    // are on the ellipse.

	    ON_Plane ell_plane = ON_Plane(ON_3dPoint(ell_center), ON_3dVector(ell_A), ON_3dVector(ell_B));
	    ON_Ellipse ell(ell_plane, ell_a, ell_b);
	    int i;
	    for (i = 0; i <= knot_count; i++) {
		ON_3dPoint pt = in->PointAt(knots[i]);
		ON_3dPoint ell_pt;
		ell_pt = ell.ClosestPointTo(pt);
		if (ell_pt.DistanceTo(pt) > fitting_tol) {
		    break;
		}
	    }

	    if (i == knot_count + 1) {
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
		    ell.ClosestPointTo(in->PointAt(knots[knot_count / 2]), &t_mid);
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
	     double tol)
{
    if (tol <= 0.0) {
	tol = ON_ZERO_TOLERANCE;
    }

    if (pointA.DistanceTo(pointB) <= tol) {
	ON_PX_EVENT event;
	event.m_type = ON_PX_EVENT::ppx_point;
	event.m_A = pointA;
	event.m_B = pointB;
	event.m_Mid = (pointA + pointB) * 0.5;
	event.m_radius = pointA.DistanceTo(pointB) * 0.5;
	x.Append(event);
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
HIDDEN bool
newton_pci(double &t, const ON_3dPoint &pointA, const ON_Curve &curveB, double tol)
{
    ON_3dPoint closest_point = curveB.PointAt(t);
    double dist = closest_point.DistanceTo(pointA);

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
    dist = closest_point.DistanceTo(pointA);
    return dist <= tol && !isnan(t);
}


bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Curve &curveB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tol,
	     const ON_Interval *curveB_domain,
	     Subcurve *treeB)
{
    if (tol <= 0.0) {
	tol = PCI_DEFAULT_TOLERANCE;
    }
    check_domain(curveB_domain, curveB.Domain(), "curveB_domain");

    Subcurve *root = treeB;
    try {
	if (root == NULL) {
	    root = get_curve_root(&curveB, curveB_domain);
	}
    } catch (std::bad_alloc &e) {
	bu_log("%s", e.what());
	return false;
    }

    if (!root->IsPointIn(pointA, tol)) {
	if (treeB == NULL) {
	    delete root;
	}
	return false;
    }

    std::vector<Subcurve *> candidates, next_candidates;
    candidates.push_back(root);

    // find the sub-curves that possibly intersect the point
    for (int i = 0; i < MAX_PCI_DEPTH; i++) {
	next_candidates.clear();
	for (size_t j = 0; j < candidates.size(); j++) {
	    if (candidates[j]->m_islinear) {
		next_candidates.push_back(candidates[j]);
	    } else {
		if (candidates[j]->Split() == 0) {
		    if (candidates[j]->m_children[0]->IsPointIn(pointA, tol)) {
			next_candidates.push_back(candidates[j]->m_children[0]);
		    }
		    if (candidates[j]->m_children[1]->IsPointIn(pointA, tol)) {
			next_candidates.push_back(candidates[j]->m_children[1]);
		    }
		} else {
		    next_candidates.push_back(candidates[j]);
		}
	    }
	}
	candidates = next_candidates;
    }

    for (size_t i = 0; i < candidates.size(); i++) {
	// use linear approximation to get an estimated intersection point
	ON_Line line(candidates[i]->m_curve->PointAtStart(), candidates[i]->m_curve->PointAtEnd());
	double t;
	line.ClosestPointTo(pointA, &t);

	// make sure line_t belongs to [0, 1]
	double line_t;
	if (t < 0) {
	    line_t = 0;
	} else if (t > 1) {
	    line_t = 1;
	} else {
	    line_t = t;
	}

	double closest_point_t = candidates[i]->m_t.Min() + candidates[i]->m_t.Length() * line_t;

	// use Newton iterations to get an accurate intersection point
	if (newton_pci(closest_point_t, pointA, curveB, tol)) {
	    ON_PX_EVENT event;
	    event.m_type = ON_PX_EVENT::pcx_point;
	    event.m_A = pointA;
	    event.m_B = curveB.PointAt(closest_point_t);
	    event.m_b[0] = closest_point_t;
	    event.m_Mid = (pointA + event.m_B) * 0.5;
	    event.m_radius = event.m_B.DistanceTo(pointA) * 0.5;
	    x.Append(event);
	    if (treeB == NULL) {
		delete root;
	    }
	    return true;
	}
    }

    // all candidates have no intersection
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
 *   we reach the maximal depth or the sub-surface is planar.
 *
 * - Use Newton-Raphson iterations to compute the closest point on
 *   the surface to that point.
 *
 * - If the closest point is within the given tolerance, there is an
 *   intersection.
 */
HIDDEN bool
newton_psi(double &u, double &v, const ON_3dPoint &pointA, const ON_Surface &surfB, double tol)
{
    ON_3dPoint closest_point = surfB.PointAt(u, v);
    double dist = closest_point.DistanceTo(pointA);

    // Use Newton-Raphson method to get an accurate point of PSI.
    // We actually calculate the closest point on the surface to that point.
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
	ON_Matrix f(2, 1), j(2, 2);
	// F0 = (surface(u, v) - pointA) \dot du(u, v);
	f[0][0] = ON_DotProduct(closest_point - pointA, du);
	// F1 = (surface(u, v) - pointA) \dot dv(u, v);
	f[1][0] = ON_DotProduct(closest_point - pointA, dv);
	// dF0/du = du(u, v) \dot du(u, v) + (surface(u, v) - pointA) \dot duu(u, v)
	j[0][0] = ON_DotProduct(du, du) + ON_DotProduct(closest_point - pointA, duu);
	// dF0/dv = dv(u, v) \dot du(u, v) + (surface(u, v) - pointA) \dot duv(u, v)
	// dF1/du = du(u, v) \dot dv(u, v) + (surface(u, v) - pointA) \dot dvu(u, v)
	// dF0/dv = dF1/du
	j[0][1] = j[1][0] = ON_DotProduct(du, dv) + ON_DotProduct(closest_point - pointA, duv);
	// dF1/dv = dv(u, v) \dot dv(u, v) + (surface(u, v) - pointA) \dot dvv(u, v)
	j[1][1] = ON_DotProduct(dv, dv) + ON_DotProduct(closest_point - pointA, dvv);

	if (!j.Invert(0.0)) {
	    break;
	}
	ON_Matrix delta;
	delta.Multiply(j, f);
	u -= delta[0][0];
	v -= delta[1][0];
    }

    u = std::min(u, surfB.Domain(0).Max());
    u = std::max(u, surfB.Domain(0).Min());
    v = std::min(v, surfB.Domain(1).Max());
    v = std::max(v, surfB.Domain(1).Min());
    closest_point = surfB.PointAt(u, v);
    dist = closest_point.DistanceTo(pointA);
    return dist <= tol && !isnan(u) && !isnan(v);
}


bool
ON_Intersect(const ON_3dPoint &pointA,
	     const ON_Surface &surfaceB,
	     ON_ClassArray<ON_PX_EVENT> &x,
	     double tol,
	     const ON_Interval *surfaceB_udomain,
	     const ON_Interval *surfaceB_vdomain,
	     Subsurface *treeB)
{
    if (tol <= 0.0) {
	tol = PCI_DEFAULT_TOLERANCE;
    }

    ON_Interval u_domain, v_domain;
    u_domain = check_domain(surfaceB_udomain, surfaceB.Domain(0), "surfaceB_udomain");
    v_domain = check_domain(surfaceB_vdomain, surfaceB.Domain(1), "surfaceB_vdomain");

    Subsurface *root = treeB;
    try {
	if (root == NULL) {
	    root = get_surface_root(&surfaceB, &u_domain, &v_domain);
	}
    } catch (std::bad_alloc &e) {
	bu_log("%s", e.what());
	return false;
    }

    if (!root->IsPointIn(pointA, tol)) {
	if (treeB == NULL) {
	    delete root;
	}
	return false;
    }

    std::vector<Subsurface *> candidates, next_candidates;
    candidates.push_back(root);

    // find the sub-surfaces that possibly intersect the point
    for (int i = 0; i < MAX_PSI_DEPTH; i++) {
	next_candidates.clear();
	for (size_t j = 0; j < candidates.size(); j++) {
	    if (candidates[j]->m_isplanar) {
		next_candidates.push_back(candidates[j]);
	    } else {
		if (candidates[j]->Split() == 0) {
		    for (int k = 0; k < 4; k++) {
			if (candidates[j]->m_children[k] && candidates[j]->m_children[k]->IsPointIn(pointA, tol)) {
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

    for (size_t i = 0; i < candidates.size(); i++) {
	// Use the mid point of the bounding box as the starting point,
	// and use Newton iterations to get an accurate intersection.
	double u = candidates[i]->m_u.Mid(), v = candidates[i]->m_v.Mid();
	if (newton_psi(u, v, pointA, surfaceB, tol)) {
	    ON_PX_EVENT event;
	    event.m_type = ON_PX_EVENT::psx_point;
	    event.m_A = pointA;
	    event.m_B = surfaceB.PointAt(u, v);
	    event.m_b[0] = u;
	    event.m_b[1] = v;
	    event.m_Mid = (pointA + event.m_B) * 0.5;
	    event.m_radius = event.m_B.DistanceTo(pointA) * 0.5;
	    x.Append(event);
	    if (treeB == NULL) {
		delete root;
	    }
	    return true;
	}
    }

    // all candidates have no intersection
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

// We can only test a finite number of points to determine overlap.
// Here we test 16 points uniformly distributed.
#define CCI_OVERLAP_TEST_POINTS 16

HIDDEN void
newton_cci(double &t_a, double &t_b, const ON_Curve *curveA, const ON_Curve *curveB, double isect_tol)
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
    if (pointA.DistanceTo(pointB) < isect_tol) {
	return;
    }

    int iteration = 0;
    while (fabs(last_t_a - t_a) + fabs(last_t_b - t_b) > ON_ZERO_TOLERANCE
	   && iteration++ < CCI_MAX_ITERATIONS) {
	last_t_a = t_a, last_t_b = t_b;
	ON_3dVector derivA, derivB;
	curveA->Ev1Der(t_a, pointA, derivA);
	curveB->Ev1Der(t_b, pointB, derivB);
	ON_Matrix j(2, 2), f(2, 1);

	// First try to solve (1), (2).
	j[0][0] = derivA.x;
	j[0][1] = -derivB.x;
	j[1][0] = derivA.y;
	j[1][1] = -derivB.y;
	f[0][0] = pointA.x - pointB.x;
	f[1][0] = pointA.y - pointB.y;
	if (!j.Invert(0.0)) {
	    // The first try failed. Try to solve (1), (3).
	    j[0][0] = derivA.x;
	    j[0][1] = -derivB.x;
	    j[1][0] = derivA.z;
	    j[1][1] = -derivB.z;
	    f[0][0] = pointA.x - pointB.x;
	    f[1][0] = pointA.z - pointB.z;
	    if (!j.Invert(0.0)) {
		// Failed again. Try to solve (2), (3).
		j[0][0] = derivA.y;
		j[0][1] = -derivB.y;
		j[1][0] = derivA.z;
		j[1][1] = -derivB.z;
		f[0][0] = pointA.y - pointB.y;
		f[1][0] = pointA.z - pointB.z;
		if (!j.Invert(0.0)) {
		    // cannot find a root
		    continue;
		}
	    }
	}
	ON_Matrix delta;
	delta.Multiply(j, f);
	t_a -= delta[0][0];
	t_b -= delta[1][0];
    }

    // make sure the solution is inside the domains
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

double
tolerance_2d_from_3d(
	double tol_3d,
	Subcurve *root,
	const ON_Curve *curve,
	const ON_Interval *curve_domain)
{
    double tol_2d = tol_3d;
    double bbox_diagonal_len = root->m_curve->BoundingBox().Diagonal().Length();
    if (!ON_NearZero(bbox_diagonal_len)) {
	const ON_Interval dom = curve_domain ? *curve_domain : curve->Domain();
	tol_2d = tol_3d / (bbox_diagonal_len * dom.Length());
    }
    return tol_2d;
}

int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Curve *curveB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double isect_tol,
	     double overlap_tol,
	     const ON_Interval *curveA_domain,
	     const ON_Interval *curveB_domain,
	     Subcurve *treeA,
	     Subcurve *treeB)
{
    if (curveA == NULL || curveB == NULL) {
	return 0;
    }

    int original_count = x.Count();

    if (isect_tol <= 0.0) {
	isect_tol = CCI_DEFAULT_TOLERANCE;
    }
    if (overlap_tol < isect_tol) {
	overlap_tol = 2.0 * isect_tol;
    }
    double t1_tol = isect_tol;
    double t2_tol = isect_tol;

    check_domain(curveA_domain, curveA->Domain(), "curveA_domain");
    check_domain(curveB_domain, curveB->Domain(), "curveB_domain");

    // handle degenerate cases (one or both of them is a "point")
    bool ispoint_curveA = curveA->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE;
    bool ispoint_curveB = curveB->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE;
    ON_3dPoint startpointA = curveA->PointAtStart();
    ON_3dPoint startpointB = curveB->PointAtStart();
    ON_ClassArray<ON_PX_EVENT> px_event;
    if (ispoint_curveA && ispoint_curveB) {
	// both curves are degenerate (point-point intersection)
	if (ON_Intersect(startpointA, startpointB, px_event, isect_tol)) {
	    XEventProxy event(ON_X_EVENT::ccx_overlap);
	    event.SetAPoints(startpointA, curveA->PointAtEnd());
	    event.SetBPoints(startpointB, curveB->PointAtEnd());
	    event.SetAOverlapRange(curveA->Domain());
	    event.SetBOverlapRange(curveB->Domain());
	    x.Append(event.Event());
	    return 1;
	} else {
	    return 0;
	}
    } else if (ispoint_curveA) {
	// curveA is degenerate (point-curve intersection)
	if (ON_Intersect(startpointA, *curveB, px_event, isect_tol)) {
	    XEventProxy event(ON_X_EVENT::ccx_overlap);
	    event.SetAPoints(startpointA, curveA->PointAtEnd());
	    event.SetBPoint(px_event[0].m_B);
	    event.SetAOverlapRange(curveA->Domain());
	    event.SetBOverlapRange(px_event[0].m_b[0]);
	    x.Append(event.Event());
	    return 1;
	} else {
	    return 0;
	}
    } else if (ispoint_curveB) {
	// curveB is degenerate (point-curve intersection)
	if (ON_Intersect(startpointB, *curveA, px_event, isect_tol)) {
	    XEventProxy event(ON_X_EVENT::ccx_overlap);
	    event.SetAPoint(px_event[0].m_B);
	    event.SetBPoints(startpointB, curveB->PointAtEnd());
	    event.SetAOverlapRange(px_event[0].m_b[0]);
	    event.SetBOverlapRange(curveB->Domain());
	    x.Append(event.Event());
	    return 1;
	} else {
	    return 0;
	}
    }

    // generate the roots of the two curve trees if necessary
    Subcurve *rootA = treeA;
    Subcurve *rootB = treeB;
    try {
	if (rootA == NULL) {
	    rootA = get_curve_root(curveA, curveA_domain);
	}
	if (rootB == NULL) {
	    rootB = get_curve_root(curveB, curveB_domain);
	}
    } catch (std::bad_alloc &e) {
	bu_log("%s", e.what());
	if (treeA == NULL) {
	    delete rootA;
	}
	return 0;
    }

    t1_tol = tolerance_2d_from_3d(isect_tol, rootA, curveA,
					curveA_domain);
    t2_tol = tolerance_2d_from_3d(isect_tol, rootB, curveB,
					curveB_domain);

    typedef std::vector<std::pair<Subcurve *, Subcurve *> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootA->Intersect(*rootB, isect_tol)) {
	candidates.push_back(std::make_pair(rootA, rootB));
    }

    // use sub-division and bounding box intersections first
    for (int h = 0; h <= MAX_CCI_DEPTH; h++) {
	if (candidates.empty()) {
	    break;
	}
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve *> splittedA, splittedB;
	    if (i->first->m_islinear || i->first->Split() != 0) {
		splittedA.push_back(i->first);
	    } else {
		splittedA.push_back(i->first->m_children[0]);
		splittedA.push_back(i->first->m_children[1]);
	    }
	    if (i->second->m_islinear || i->second->Split() != 0) {
		splittedB.push_back(i->second);
	    } else {
		splittedB.push_back(i->second->m_children[0]);
		splittedB.push_back(i->second->m_children[1]);
	    }
	    // intersected children go to the next step
	    for (size_t j = 0; j < splittedA.size(); j++)
		for (size_t k = 0; k < splittedB.size(); k++)
		    if (splittedA[j]->Intersect(*splittedB[k], isect_tol)) {
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
		    }
	}
	candidates = next_candidates;
    }

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    // calculate an accurate intersection point for intersected
    // bounding boxes
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	// special handling for linear curves
	if (i->first->m_islinear && i->second->m_islinear) {
	    // if both are linear we use ON_IntersectLineLine
	    ON_Line lineA(curveA->PointAt(i->first->m_t.Min()), curveA->PointAt(i->first->m_t.Max()));
	    ON_Line lineB(curveB->PointAt(i->second->m_t.Min()), curveB->PointAt(i->second->m_t.Max()));
	    if (lineA.Direction().IsParallelTo(lineB.Direction())) {
		if (lineA.MinimumDistanceTo(lineB) < isect_tol) {
		    // report a ccx_overlap event
		    double startB_on_A, endB_on_A, startA_on_B, endA_on_B;
		    lineA.ClosestPointTo(lineB.from, &startB_on_A);
		    lineA.ClosestPointTo(lineB.to, &endB_on_A);
		    lineB.ClosestPointTo(lineA.from, &startA_on_B);
		    lineB.ClosestPointTo(lineA.to, &endA_on_B);

		    if (startB_on_A > 1 + t1_tol || endB_on_A < -t1_tol
			|| startA_on_B > 1 + t2_tol || endA_on_B < -t2_tol) {
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

		    XEventProxy event(ON_X_EVENT::ccx_overlap);
		    event.SetAPoints(lineA.PointAt(t_a1), lineA.PointAt(t_a2));
		    event.SetBPoints(lineB.PointAt(t_b1), lineB.PointAt(t_b2));
		    event.SetAOverlapRange(i->first->m_t.ParameterAt(t_a1),
			    i->first->m_t.ParameterAt(t_a2));
		    event.SetBOverlapRange(i->first->m_t.ParameterAt(t_b1),
			    i->first->m_t.ParameterAt(t_b2));
		    tmp_x.Append(event.Event());
		}
	    } else {
		// not parallel, check intersection point
		double t_lineA, t_lineB;
		double t_a, t_b;
		if (ON_IntersectLineLine(lineA, lineB, &t_lineA, &t_lineB, ON_ZERO_TOLERANCE, true)) {
		    t_a = i->first->m_t.ParameterAt(t_lineA);
		    t_b = i->second->m_t.ParameterAt(t_lineB);

		    XEventProxy event(ON_X_EVENT::ccx_point);
		    event.SetAPoint(lineA.PointAt(t_lineA));
		    event.SetBPoint(lineB.PointAt(t_lineB));
		    event.SetACurveParameter(t_a);
		    event.SetBCurveParameter(t_b);
		    tmp_x.Append(event.Event());
		}
	    }
	} else {
	    // They are not both linear.

	    // Use two different start points - the two end-points of the interval
	    // If they converge to one point, it's considered an intersection
	    // point, otherwise it's considered an overlap event.
	    // FIXME: Find a better mechanism to check overlapping, because this method
	    // may miss some overlap cases. (Overlap events can also converge to one
	    // point).
	    double t_a1 = i->first->m_t.Min(), t_b1 = i->second->m_t.Min();
	    newton_cci(t_a1, t_b1, curveA, curveB, isect_tol);
	    double t_a2 = i->first->m_t.Max(), t_b2 = i->second->m_t.Max();
	    newton_cci(t_a2, t_b2, curveA, curveB, isect_tol);
	    if (isnan(t_a1) || isnan(t_b1)) {
		// the first iteration result is not sufficient
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
	    if ((pointA1.DistanceTo(pointA2) < isect_tol && pointB1.DistanceTo(pointB2) < isect_tol)
		|| (isnan(t_a2) || isnan(t_b2))) {
		// it's considered the same point
		ON_3dPoint pointA = curveA->PointAt(t_a1);
		ON_3dPoint pointB = curveB->PointAt(t_b1);
		double distance = pointA.DistanceTo(pointB);
		// check the validity of the solution
		if (distance < isect_tol) {
		    XEventProxy event(ON_X_EVENT::ccx_point);
		    event.SetAPoint(pointA);
		    event.SetBPoint(pointB);
		    event.SetACurveParameter(t_a1);
		    event.SetBCurveParameter(t_b1);
		    tmp_x.Append(event.Event());
		}
	    } else {
		// check overlap
		// bu_log("Maybe overlap.\n");
		double distance1 = pointA1.DistanceTo(pointB1);
		double distance2 = pointA2.DistanceTo(pointB2);

		// check the validity of the solution
		if (distance1 < isect_tol && distance2 < isect_tol) {
		    int j;
		    for (j = 1; j < CCI_OVERLAP_TEST_POINTS; j++) {
			double strike = 1.0 / CCI_OVERLAP_TEST_POINTS;
			ON_3dPoint test_point = curveA->PointAt(t_a1 + (t_a2 - t_a1) * j * strike);
			ON_ClassArray<ON_PX_EVENT> pci_x;
			if (!ON_Intersect(test_point, *curveB, pci_x, overlap_tol)) {
			    break;
			}
		    }
		    if (j == CCI_OVERLAP_TEST_POINTS) {
			XEventProxy event(ON_X_EVENT::ccx_overlap);
			if (t_a1 <= t_a2) {
			    event.SetAPoints(pointA1, pointA2);
			    event.SetBPoints(pointB1, pointB2);
			    event.SetAOverlapRange(t_a1, t_a2);
			    event.SetBOverlapRange(t_b1, t_b2);
			} else {
			    event.SetAPoints(pointA2, pointA1);
			    event.SetBPoints(pointB2, pointB1);
			    event.SetAOverlapRange(t_a2, t_a1);
			    event.SetBOverlapRange(t_b2, t_b1);
			}
			tmp_x.Append(event.Event());
			continue;
		    }
		    // If j != CCI_OVERLAP_TEST_POINTS, two ccx_point events should
		    // be generated. Fall through.
		}
		if (distance1 < isect_tol) {
		    XEventProxy event(ON_X_EVENT::ccx_point);
		    event.SetAPoint(pointA1);
		    event.SetBPoint(pointB1);
		    event.SetACurveParameter(t_a1);
		    event.SetBCurveParameter(t_b1);
		    tmp_x.Append(event.Event());
		}
		if (distance2 < isect_tol) {
		    XEventProxy event(ON_X_EVENT::ccx_point);
		    event.SetAPoint(pointA2);
		    event.SetBPoint(pointB2);
		    event.SetACurveParameter(t_a2);
		    event.SetBCurveParameter(t_b2);
		    tmp_x.Append(event.Event());
		}
	    }
	}
    }

    // use an O(n^2) naive approach to eliminate duplications
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
	    if (tmp_x[i].m_A[0].DistanceTo(points[j].m_A[0]) < isect_tol
		&& tmp_x[i].m_A[1].DistanceTo(points[j].m_A[1]) < isect_tol
		&& tmp_x[i].m_B[0].DistanceTo(points[j].m_B[0]) < isect_tol
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < isect_tol) {
		break;
	    }
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // merge the overlap events that are continuous
    ON_X_EVENT pending;
    overlap.QuickSort(compare_by_m_a0);
    if (overlap.Count()) {
	pending = overlap[0];
	for (int i = 1; i < overlap.Count(); i++) {
	    if (pending.m_a[1] < overlap[i].m_a[0] - t1_tol) {
		// pending[j] and overlap[i] are disjoint. pending[j] was
		// already merged, and should be removed from the list.
		// Because the elements in overlap[] are sorted by m_a[0],
		// the remaining elements won't intersect pending[j].
		pending.m_A[0] = curveA->PointAt(pending.m_a[0]);
		pending.m_A[1] = curveA->PointAt(pending.m_a[1]);
		pending.m_B[0] = curveB->PointAt(pending.m_b[0]);
		pending.m_B[1] = curveB->PointAt(pending.m_b[1]);
		x.Append(pending);
		pending = overlap[i];
	    } else if (overlap[i].m_a[0] < pending.m_a[1] + t1_tol) {
		ON_Interval interval_1(overlap[i].m_b[0], overlap[i].m_b[1]);
		ON_Interval interval_2(pending.m_b[0], pending.m_b[1]);
		interval_1.MakeIncreasing();
		interval_1.m_t[0] -= t2_tol;
		interval_1.m_t[1] += t2_tol;
		if (interval_1.Intersection(interval_2)) {
		    // need to merge: pending[j] = union(pending[j], overlap[i])
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

    // the intersection points shouldn't be inside the overlapped parts.
    int overlap_events = x.Count();
    for (int i = 0; i < points.Count(); i++) {
	int j;
	for (j = original_count; j < overlap_events; j++) {
	    if (points[i].m_a[0] > x[j].m_a[0] - t1_tol
		&& points[i].m_a[0] < x[j].m_a[1] + t1_tol) {
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

// We can only test a finite number of points to determine overlap.
// Here we test 2 points uniformly distributed.
#define CSI_OVERLAP_TEST_POINTS 2

HIDDEN void
newton_csi(double &t, double &u, double &v, const ON_Curve *curveA, const ON_Surface *surfB, double isect_tol, Subsurface *tree)
{
    // Equations:
    //   C_x(t) - S_x(u,v) = 0
    //   C_y(t) - S_y(u,v) = 0
    //   C_z(t) - S_z(u,v) = 0
    // We use Newton-Raphson iterations to solve these equations.
    double last_t = DBL_MAX / 3, last_u = DBL_MAX / 3, last_v = DBL_MAX / 3;
    ON_3dPoint pointA = curveA->PointAt(t);
    ON_3dPoint pointB = surfB->PointAt(u, v);
    if (pointA.DistanceTo(pointB) < isect_tol) {
	return;
    }

    // If curveA(t) is already on surfB... Newton iterations may not succeed
    // with this case. Use point-surface intersections.
    ON_ClassArray<ON_PX_EVENT> px_event;
    if (ON_Intersect(pointA, *surfB, px_event, isect_tol, 0, 0, tree)) {
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
	ON_Matrix j(3, 3), f(3, 1);
	for (int i = 0; i < 3; i++) {
	    j[i][0] = derivA[i];
	    j[i][1] = -derivBu[i];
	    j[i][2] = -derivBv[i];
	    f[i][0] = pointA[i] - pointB[i];
	}
	if (!j.Invert(0.0)) {
	    break;
	}
	ON_Matrix delta;
	delta.Multiply(j, f);
	t -= delta[0][0];
	u -= delta[1][0];
	v -= delta[2][0];
    }

    // make sure the solution is inside the domains
    t = std::min(t, curveA->Domain().Max());
    t = std::max(t, curveA->Domain().Min());
    u = std::min(u, surfB->Domain(0).Max());
    u = std::max(u, surfB->Domain(0).Min());
    v = std::min(v, surfB->Domain(1).Max());
    v = std::max(v, surfB->Domain(1).Min());
}

double
tolerance_2d_from_3d(
	double tol_3d,
	Subsurface *root,
	const ON_Surface *surf,
	const ON_Interval *surf_udomain,
	const ON_Interval *surf_vdomain)
{
    double tol_2d = tol_3d;

    const ON_Interval surf_udom = surf_udomain ? *surf_udomain : surf->Domain(0);
    const ON_Interval surf_vdom = surf_vdomain ? *surf_vdomain : surf->Domain(1);
    double surf_ulen = surf_udom.Length();
    double surf_vlen = surf_vdom.Length();
    double uv_diagonal_len = ON_2dVector(surf_ulen, surf_vlen).Length();

    double bbox_diagonal_len = root->m_surf->BoundingBox().Diagonal().Length();
    if (!ON_NearZero(bbox_diagonal_len)) {
	tol_2d = tol_3d / (bbox_diagonal_len * uv_diagonal_len);
    }
    return tol_2d;
}

int
ON_Intersect(const ON_Curve *curveA,
	     const ON_Surface *surfaceB,
	     ON_SimpleArray<ON_X_EVENT> &x,
	     double isect_tol,
	     double overlap_tol,
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

    if (isect_tol <= 0.0) {
	isect_tol = CSI_DEFAULT_TOLERANCE;
    }
    if (overlap_tol < isect_tol) {
	overlap_tol = 2.0 * isect_tol;
    }

    check_domain(curveA_domain, curveA->Domain(), "curveA_domain");
    check_domain(surfaceB_udomain, surfaceB->Domain(0), "surfaceB_udomain");
    check_domain(surfaceB_vdomain, surfaceB->Domain(1), "surfaceB_vdomain");

    // if the curve is degenerated to a point...
    if (curveA->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE) {
	// point-surface intersections
	ON_3dPoint pointA = curveA->PointAtStart();
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (ON_Intersect(pointA, *surfaceB, px_event, isect_tol)) {
	    XEventProxy event(ON_X_EVENT::csx_overlap);
	    event.SetAPoints(pointA, curveA->PointAtEnd());
	    event.SetBPoint(px_event[0].m_B);
	    event.SetAOverlapRange(curveA->Domain());
	    event.SetBSurfaceParameter(px_event[0].m_b[0], px_event[0].m_b[1]);
	    x.Append(event.Event());
	    if (overlap2d) {
		ON_X_EVENT xevent = event.Event();
		overlap2d->Append(new ON_LineCurve(xevent.m_B[0], xevent.m_B[1]));
	    }
	    return 1;
	} else {
	    return 0;
	}
    }

    // generate the roots of the two curve trees if necessary
    Subcurve *rootA = treeA;
    Subsurface *rootB = treeB;
    try {
	if (rootA == NULL) {
	    rootA = get_curve_root(curveA, curveA_domain);
	}
	if (rootB == NULL) {
	    rootB = get_surface_root(surfaceB, surfaceB_udomain,
				     surfaceB_vdomain);
	}
    } catch (std::bad_alloc &e) {
	bu_log("%s", e.what());
	if (treeA == NULL) {
	    delete rootA;
	}
	return 0;
    }

    // adjust the tolerance from 3D scale to respective 2D scales
    double t_tol = isect_tol;
    double u_tol = isect_tol;
    double v_tol = isect_tol;
    double l = rootA->m_curve->BoundingBox().Diagonal().Length();
    double dl = curveA_domain ? curveA_domain->Length() : curveA->Domain().Length();
    if (!ON_NearZero(l)) {
	t_tol = isect_tol / l * dl;
    }
    l = rootB->m_surf->BoundingBox().Diagonal().Length();
    dl = surfaceB_udomain ? surfaceB_udomain->Length() : surfaceB->Domain(0).Length();
    if (!ON_NearZero(l)) {
	u_tol = isect_tol / l * dl;
    }
    dl = surfaceB_vdomain ? surfaceB_vdomain->Length() : surfaceB->Domain(1).Length();
    if (!ON_NearZero(l)) {
	v_tol = isect_tol / l * dl;
    }

    typedef std::vector<std::pair<Subcurve *, Subsurface *> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootB->Intersect(*rootA, isect_tol)) {
	candidates.push_back(std::make_pair(rootA, rootB));
    }

    // use sub-division and bounding box intersections first
    for (int h = 0; h <= MAX_CSI_DEPTH; h++) {
	if (candidates.empty()) {
	    break;
	}
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve *> splittedA;
	    std::vector<Subsurface *> splittedB;
	    if (i->first->m_islinear && i->second->m_isplanar) {
		splittedA.push_back(i->first);
		splittedB.push_back(i->second);
	    } else {
		if (i->first->Split() != 0) {
		    splittedA.push_back(i->first);
		} else {
		    splittedA.push_back(i->first->m_children[0]);
		    splittedA.push_back(i->first->m_children[1]);
		}
		if (i->second->Split() != 0) {
		    splittedB.push_back(i->second);
		} else {
		    for (int j = 0; j < 4; j++) {
			splittedB.push_back(i->second->m_children[j]);
		    }
		}
	    }
	    for (size_t j = 0; j < splittedA.size(); j++)
		for (size_t k = 0; k < splittedB.size(); k++)
		    if (splittedB[k]->Intersect(*splittedA[j], isect_tol)) {
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
		    }
	}
	candidates = next_candidates;
    }

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    // calculate an accurate intersection point for intersected
    // bounding boxes
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	if (i->first->m_islinear && i->second->m_isplanar) {
	    // try optimized approach for line-plane intersections
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
		    // they are parallel (or overlap)

		    if (line.InPlane(plane, isect_tol)) {
			// The line is on the surface's plane. The end-points of
			// the overlap must be the linecurve's end-points or
			// the intersection between the linecurve and the boundary
			// of the surface.

			ON_X_EVENT event;

			// first we check the endpoints of the line segment (PSI)
			ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
			int intersections = 0;
			if (ON_Intersect(line.from, *surfaceB, px_event1, isect_tol, 0, 0, treeB)) {
			    event.m_A[intersections] = line.from;
			    event.m_B[intersections] = px_event1[0].m_B;
			    event.m_a[intersections] = i->first->m_t.Min();
			    event.m_b[2 * intersections] = px_event1[0].m_b[0];
			    event.m_b[2 * intersections + 1] = px_event1[0].m_b[1];
			    intersections++;
			}
			if (ON_Intersect(line.to, *surfaceB, px_event2, isect_tol, 0, 0, treeB)) {
			    event.m_A[intersections] = line.to;
			    event.m_B[intersections] = px_event2[0].m_B;
			    event.m_a[intersections] = i->first->m_t.Max();
			    event.m_b[2 * intersections] = px_event2[0].m_b[0];
			    event.m_b[2 * intersections + 1] = px_event2[0].m_b[1];
			    intersections++;
			}

			// then we check the intersection of the line segment
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
			    // convert from the boxes' domain to the whole surface's domain
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
			    event.m_A[intersections] = line.PointAt(line_t[j]);
			    event.m_B[intersections] = surfaceB->PointAt(surf_u, surf_v);
			    event.m_a[intersections] = i->first->m_t.ParameterAt(line_t[j]);
			    event.m_b[2 * intersections] = surf_u;
			    event.m_b[2 * intersections + 1] = surf_v;
			    intersections++;
			}

			// generate an ON_X_EVENT
			if (intersections == 0) {
			    continue;
			}
			if (intersections == 1) {
			    event.m_type = ON_X_EVENT::csx_point;
			    event.m_A[1] = event.m_A[0];
			    event.m_B[1] = event.m_B[0];
			    event.m_a[1] = event.m_a[0];
			    event.m_b[2] = event.m_b[0];
			    event.m_b[3] = event.m_b[1];
			} else {
			    event.m_type = ON_X_EVENT::csx_overlap;
			    if (event.m_a[0] > event.m_a[1]) {
				std::swap(event.m_A[0], event.m_A[1]);
				std::swap(event.m_B[0], event.m_B[1]);
				std::swap(event.m_a[0], event.m_a[1]);
				std::swap(event.m_b[0], event.m_b[2]);
				std::swap(event.m_b[1], event.m_b[3]);
			    }
			}
			tmp_x.Append(event);
			continue;
		    } else {
			continue;
		    }
		} else {
		    // not parallel (not that this means cos_angle != 0), check intersection point
		    double cos_angle = fabs(ON_DotProduct(plane.Normal(), line.Direction())) / (plane.Normal().Length() * line.Direction().Length());
		    double distance = fabs(plane.DistanceTo(line.from));
		    double line_t = distance / cos_angle / line.Length();
		    if (line_t > 1.0 + t_tol) {
			continue;
		    }
		    ON_3dPoint intersection = line.from + line.Direction() * line_t;
		    ON_ClassArray<ON_PX_EVENT> px_event;
		    if (!ON_Intersect(intersection, *(i->second->m_surf), px_event, isect_tol, 0, 0, treeB)) {
			continue;
		    }

		    XEventProxy event(ON_X_EVENT::csx_point);
		    event.SetAPoint(intersection);
		    event.SetBPoint(px_event[0].m_B);
		    event.SetACurveParameter(i->first->m_t.ParameterAt(line_t));
		    event.SetBSurfaceParameter(px_event[0].m_b.x, px_event[0].m_b.y);
		    tmp_x.Append(event.Event());
		}
	    }
	}

	// They are not both linear or planar, or the above attempt failed.

	// Use two different start points - the two end-points of the interval
	// of the Subcurve's m_t.
	// If they converge to one point, it's considered an intersection
	// point, otherwise it's considered an overlap event.
	// FIXME: Find a better mechanism to check overlapping, because this method
	// may miss some overlap cases. (Overlap events can also converge to one
	// point).

	double u1 = i->second->m_u.Mid(), v1 = i->second->m_v.Mid();
	double t1 = i->first->m_t.Min();
	newton_csi(t1, u1, v1, curveA, surfaceB, isect_tol, treeB);
	double u2 = i->second->m_u.Mid(), v2 = i->second->m_v.Mid();
	double t2 = i->first->m_t.Max();
	newton_csi(t2, u2, v2, curveA, surfaceB, isect_tol, treeB);

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
	if (pointA1.DistanceTo(pointA2) < isect_tol
	    && pointB1.DistanceTo(pointB2) < isect_tol) {
	    // it's considered the same point (both converge)
	    double distance = pointA1.DistanceTo(pointB1);
	    // check the validity of the solution
	    if (distance < isect_tol) {
		XEventProxy event(ON_X_EVENT::csx_point);
		event.SetAPoint(pointA1);
		event.SetBPoint(pointB1);
		event.SetACurveParameter(t1);
		event.SetBSurfaceParameter(u1, v1);
		tmp_x.Append(event.Event());
	    }
	} else {
	    // check overlap
	    // bu_log("Maybe overlap.\n");
	    double distance1 = pointA1.DistanceTo(pointB1);
	    double distance2 = pointA2.DistanceTo(pointB2);

	    // check the validity of the solution
	    if (distance1 < isect_tol && distance2 < isect_tol) {
		XEventProxy event(ON_X_EVENT::csx_overlap);
		// make sure that m_a[0] <= m_a[1]
		if (t1 <= t2) {
		    event.SetAPoints(pointA1, pointA2);
		    event.SetBPoints(pointB1, pointB2);
		    event.SetAOverlapRange(t1, t2);
		    event.SetBSurfaceParameters(u1, v1, u2, v2);
		} else {
		    event.SetAPoints(pointA2, pointA1);
		    event.SetBPoints(pointB2, pointB1);
		    event.SetAOverlapRange(t2, t1);
		    event.SetBSurfaceParameters(u2, v2, u1, v1);
		}
		int j;
		for (j = 1; j < CSI_OVERLAP_TEST_POINTS; j++) {
		    double strike = 1.0 / CSI_OVERLAP_TEST_POINTS;
		    ON_3dPoint test_point = curveA->PointAt(t1 + (t2 - t1) * j * strike);
		    ON_ClassArray<ON_PX_EVENT> psi_x;
		    if (!ON_Intersect(test_point, *surfaceB, psi_x, overlap_tol, 0, 0, treeB)) {
			break;
		    }
		}
		if (j == CSI_OVERLAP_TEST_POINTS) {
		    tmp_x.Append(event.Event());
		    continue;
		}
		// If j != CSI_OVERLAP_TEST_POINTS, two csx_point events may
		// be generated. Fall through.
	    }
	    if (distance1 < isect_tol) {
		XEventProxy event(ON_X_EVENT::csx_point);
		event.SetAPoint(pointA1);
		event.SetBPoint(pointB1);
		event.SetACurveParameter(t1);
		event.SetBSurfaceParameter(u1, v1);
		tmp_x.Append(event.Event());
	    }
	    if (distance2 < isect_tol) {
		XEventProxy event(ON_X_EVENT::csx_point);
		event.SetAPoint(pointA2);
		event.SetBPoint(pointB2);
		event.SetACurveParameter(t2);
		event.SetBSurfaceParameter(u2, v2);
		tmp_x.Append(event.Event());
	    }
	}
    }

    ON_SimpleArray<ON_X_EVENT> points, overlap;
    // use an O(n^2) naive approach to eliminate duplications
    for (int i = 0; i < tmp_x.Count(); i++) {
	int j;
	if (tmp_x[i].m_type == ON_X_EVENT::csx_overlap) {
	    overlap.Append(tmp_x[i]);
	    continue;
	}
	// csx_point
	for (j = 0; j < points.Count(); j++) {
	    if (tmp_x[i].m_A[0].DistanceTo(points[j].m_A[0]) < isect_tol
		&& tmp_x[i].m_A[1].DistanceTo(points[j].m_A[1]) < isect_tol
		&& tmp_x[i].m_B[0].DistanceTo(points[j].m_B[0]) < isect_tol
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < isect_tol) {
		break;
	    }
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // merge the overlap events that are continuous
    ON_X_EVENT pending;
    ON_3dPointArray ptarrayB;
    overlap.QuickSort(compare_by_m_a0);
    if (overlap.Count()) {
	pending = overlap[0];
	ptarrayB.Append(ON_3dPoint(pending.m_b[0], pending.m_b[1], 0.0));
	for (int i = 1; i < overlap.Count(); i++) {
	    if (pending.m_a[1] < overlap[i].m_a[0] - t_tol) {
		// pending[j] and overlap[i] are disjoint. pending[j] was
		// already merged, and should be removed from the list.
		// Because the elements in overlap[] are sorted by m_a[0],
		// the remaining elements won't intersect pending[j].
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
	    } else if (overlap[i].m_a[0] < pending.m_a[1] + t_tol) {
		if (overlap2d) {
		    ptarrayB.Append(ON_3dPoint(overlap[i].m_b[0], overlap[i].m_b[1], 0.0));
		}
		ON_Interval interval_u1(overlap[i].m_b[0], overlap[i].m_b[2]);
		ON_Interval interval_u2(pending.m_b[0], pending.m_b[2]);
		ON_Interval interval_v1(overlap[i].m_b[1], overlap[i].m_b[3]);
		ON_Interval interval_v2(pending.m_b[1], pending.m_b[3]);
		interval_u1.MakeIncreasing();
		interval_u1.m_t[0] -= u_tol;
		interval_u1.m_t[1] += u_tol;
		interval_v1.MakeIncreasing();
		interval_v1.m_t[0] -= v_tol;
		interval_v1.m_t[1] += v_tol;
		if (interval_u1.Intersection(interval_u2) && interval_v1.Intersection(interval_v2)) {
		    // If the uv rectangle of them intersects, it's considered overlap.
		    // need to merge: pending[j] = union(pending[j], overlap[i])
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
	    if (points[i].m_a[0] > x[j].m_a[0] - t_tol
		&& points[i].m_a[0] < x[j].m_a[1] + t_tol) {
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
 *    sub-surfaces' bounding boxes.
 *
 * -- Calculate the intersection of sub-surfaces' bboxes, if they do
 *    intersect, go deeper with splitting surfaces and smaller bboxes,
 *    otherwise trace back.
 *
 * - After getting the intersecting bboxes, approximate the surface
 *   inside the bbox as two triangles, and calculate the intersection
 *   points of the triangles (both in 3d space and two surfaces' UV
 *   space).
 *
 * - If an intersection point is accurate enough (within the tolerance),
 *   we append it to the list. Otherwise, we use Newton-Raphson iterations
 *   to solve an under-determined system to get an more accurate inter-
 *   section point.
 *
 * - Fit the intersection points into polyline curves, and then to NURBS
 *   curves. Points with distance less than max_dist are considered in
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
struct Triangle {
    ON_3dPoint a, b, c;
    ON_2dPoint a_2d, b_2d, c_2d;
    inline void CreateFromPoints(ON_3dPoint &pA, ON_3dPoint &pB, ON_3dPoint &pC)
    {
	a = pA;
	b = pB;
	c = pC;
    }
    ON_3dPoint BarycentricCoordinate(const ON_3dPoint &pt) const
    {
	double x, y, z, pivot_ratio;
	if (ON_Solve2x2(a[0] - c[0], b[0] - c[0], a[1] - c[1], b[1] - c[1], pt[0] - c[0], pt[1] - c[1], &x, &y, &pivot_ratio) == 2) {
	    return ON_3dPoint(x, y, 1 - x - y);
	}
	if (ON_Solve2x2(a[0] - c[0], b[0] - c[0], a[2] - c[2], b[2] - c[2], pt[0] - c[0], pt[2] - c[2], &x, &z, &pivot_ratio) == 2) {
	    return ON_3dPoint(x, 1 - x - z, z);
	}
	if (ON_Solve2x2(a[1] - c[1], b[1] - c[1], a[2] - c[2], b[2] - c[2], pt[1] - c[1], pt[2] - c[2], &y, &z, &pivot_ratio) == 2) {
	    return ON_3dPoint(1 - y - z, y, z);
	}
	return ON_3dPoint::UnsetPoint;
    }
    void GetLineSegments(ON_Line line[3]) const
    {
	line[0] = ON_Line(a, b);
	line[1] = ON_Line(a, c);
	line[2] = ON_Line(b, c);
    }
};


HIDDEN bool
triangle_intersection(const struct Triangle &triA, const struct Triangle &triB, ON_3dPoint &center)
{
    ON_Plane plane_a(triA.a, triA.b, triA.c);
    ON_Plane plane_b(triB.a, triB.b, triB.c);
    ON_Line intersect;
    if (plane_a.Normal().IsParallelTo(plane_b.Normal())) {
	if (!ON_NearZero(plane_a.DistanceTo(plane_b.origin))) {
	    // parallel
	    return false;
	}
	// the two triangles are in the same plane
	ON_3dPoint pt(0.0, 0.0, 0.0);
	int count = 0;
	ON_Line lineA[3], lineB[3];
	triA.GetLineSegments(lineA);
	triB.GetLineSegments(lineB);
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
    double dp1 = ON_DotProduct(triA.a - intersect.from, line_normal);
    double dp2 = ON_DotProduct(triA.b - intersect.from, line_normal);
    double dp3 = ON_DotProduct(triA.c - intersect.from, line_normal);

    int points_on_one_side = (dp1 >= ON_ZERO_TOLERANCE) + (dp2 >= ON_ZERO_TOLERANCE) + (dp3 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3) {
	return false;
    }

    line_normal = ON_CrossProduct(plane_b.Normal(), intersect.Direction());
    double dp4 = ON_DotProduct(triB.a - intersect.from, line_normal);
    double dp5 = ON_DotProduct(triB.b - intersect.from, line_normal);
    double dp6 = ON_DotProduct(triB.c - intersect.from, line_normal);
    points_on_one_side = (dp4 >= ON_ZERO_TOLERANCE) + (dp5 >= ON_ZERO_TOLERANCE) + (dp6 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3) {
	return false;
    }

    double t[4];
    int count = 0;
    double t1, t2;
    // dpi*dpj <= 0 - the corresponding points are on different sides,
    // or at least one of them is on the intersection line
    // - the line segment between them is intersected by the plane-plane
    // intersection line
    if (dp1 * dp2 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(triA.a, &t1);
	intersect.ClosestPointTo(triA.b, &t2);
	double d1 = triA.a.DistanceTo(intersect.PointAt(t1));
	double d2 = triA.b.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp1 * dp3 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(triA.a, &t1);
	intersect.ClosestPointTo(triA.c, &t2);
	double d1 = triA.a.DistanceTo(intersect.PointAt(t1));
	double d2 = triA.c.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp2 * dp3 < ON_ZERO_TOLERANCE && count != 2) {
	intersect.ClosestPointTo(triA.b, &t1);
	intersect.ClosestPointTo(triA.c, &t2);
	double d1 = triA.b.DistanceTo(intersect.PointAt(t1));
	double d2 = triA.c.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (count != 2) {
	return false;
    }
    if (dp4 * dp5 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(triB.a, &t1);
	intersect.ClosestPointTo(triB.b, &t2);
	double d1 = triB.a.DistanceTo(intersect.PointAt(t1));
	double d2 = triB.b.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp4 * dp6 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(triB.a, &t1);
	intersect.ClosestPointTo(triB.c, &t2);
	double d1 = triB.a.DistanceTo(intersect.PointAt(t1));
	double d2 = triB.c.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2)) {
	    t[count++] = t1 + d1 / (d1 + d2) * (t2 - t1);
	}
    }
    if (dp5 * dp6 < ON_ZERO_TOLERANCE && count != 4) {
	intersect.ClosestPointTo(triB.b, &t1);
	intersect.ClosestPointTo(triB.c, &t2);
	double d1 = triB.b.DistanceTo(intersect.PointAt(t1));
	double d2 = triB.c.DistanceTo(intersect.PointAt(t2));
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
    double distance3d, dist_uA, dist_vA, dist_uB, dist_vB;
    double tol;
    bool operator < (const PointPair &pp) const
    {
	if (ON_NearZero(distance3d - pp.distance3d, tol)) {
	    return dist_uA + dist_vA + dist_uB + dist_vB < pp.dist_uA + pp.dist_vA + pp.dist_uB + pp.dist_vB;
	}
	return distance3d < pp.distance3d;
    }
};


HIDDEN void
add_points_to_closed_seams(
	const ON_Surface *surfA,
	const ON_Surface *surfB,
	ON_3dPointArray &curvept,
	ON_2dPointArray &curve_uvA,
	ON_2dPointArray &curve_uvB)
{

    for (int i = 0; i < 4; ++i) {
	int dir = i % 2;
	bool doA = i < 2;

	ON_BOOL32 is_closed = (doA ? surfA : surfB)->IsClosed(dir);
	if (!is_closed) {
	    continue;
	}
	const ON_Interval domain = (doA ? surfA : surfB)->Domain(dir);
	double dmin = domain.Min();
	double dmax = domain.Max();

	for (int j = 0; j < curvept.Count(); ++j) {
	    ON_2dPoint uvA = curve_uvA[j];
	    ON_2dPoint uvB = curve_uvB[j];
	    double &dval = (doA ? uvA : uvB)[dir];

	    // if this uv point is NEAR the closed seam, add a new
	    // point ON the seam with the same 3D coordinates
	    if (ON_NearZero(dval - dmin)) {
		dval = dmax;
	    } else if (ON_NearZero(dval - dmax)) {
		dval = dmin;
	    } else {
		continue;
	    }
	    curvept.Append(curvept[j]);
	    curve_uvA.Append(uvA);
	    curve_uvB.Append(uvB);
	}
    }
}


HIDDEN bool
newton_ssi(double &uA, double &vA, double &uB, double &vB, const ON_Surface *surfA, const ON_Surface *surfB, double isect_tol)
{
    ON_3dPoint pointA = surfA->PointAt(uA, vA);
    ON_3dPoint pointB = surfB->PointAt(uB, vB);
    if (pointA.DistanceTo(pointB) < isect_tol) {
	return true;
    }

    // Equations:
    //   x_a(uA,vA) - x_b(uB,vB) = 0
    //   y_a(uA,vA) - y_b(uB,vB) = 0
    //   z_a(uA,vA) - z_b(uB,vB) = 0
    // It's an under-determined system. We use Moore-Penrose pseudoinverse:
    // pinv(A) = transpose(A) * inv(A * transpose(A)) (A is a 3x4 Jacobian)
    // A * pinv(A) = I_3
    double last_uA = DBL_MAX / 4, last_vA = DBL_MAX / 4, last_uB = DBL_MAX / 4, last_vB = DBL_MAX / 4;

    int iteration = 0;
    while (fabs(last_uA - uA) + fabs(last_vA - vA) + fabs(last_uB - uB) + fabs(last_vB - vB) > ON_ZERO_TOLERANCE
	   && iteration++ < SSI_MAX_ITERATIONS) {
	last_uA = uA, last_vA = vA, last_uB = uB, last_vB = vB;
	ON_3dVector deriv_uA, deriv_vA, deriv_uB, deriv_vB;
	surfA->Ev1Der(uA, vA, pointA, deriv_uA, deriv_vA);
	surfB->Ev1Der(uB, vB, pointB, deriv_uB, deriv_vB);
	ON_Matrix j(3, 4), f(3, 1);
	for (int i = 0; i < 3; i++) {
	    j[i][0] = deriv_uA[i];
	    j[i][1] = deriv_vA[i];
	    j[i][2] = -deriv_uB[i];
	    j[i][3] = -deriv_vB[i];
	    f[i][0] = pointA[i] - pointB[i];
	}
	ON_Matrix jtrans = j;
	jtrans.Transpose();
	ON_Matrix j_jtrans;
	j_jtrans.Multiply(j, jtrans);
	if (!j_jtrans.Invert(0.0)) {
	    break;
	}
	ON_Matrix pinv_j;
	pinv_j.Multiply(jtrans, j_jtrans);
	ON_Matrix delta;
	delta.Multiply(pinv_j, f);
	uA -= delta[0][0];
	vA -= delta[1][0];
	uB -= delta[2][0];
	vB -= delta[3][0];
    }

    // make sure the solution is inside the domains
    uA = std::min(uA, surfA->Domain(0).Max());
    uA = std::max(uA, surfA->Domain(0).Min());
    vA = std::min(vA, surfA->Domain(1).Max());
    vA = std::max(vA, surfA->Domain(1).Min());
    uB = std::min(uB, surfB->Domain(0).Max());
    uB = std::max(uB, surfB->Domain(0).Min());
    vB = std::min(vB, surfB->Domain(1).Max());
    vB = std::max(vB, surfB->Domain(1).Min());

    pointA = surfA->PointAt(uA, vA);
    pointB = surfB->PointAt(uB, vB);
    return (pointA.DistanceTo(pointB) < isect_tol) && !isnan(uA) && !isnan(vA) && !isnan(uB) & !isnan(vB);
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


struct Overlapevent {
    ON_SSX_EVENT *m_event;  // Use pointer to the ON_SSX_EVENT in case that
			    // ~ON_SSX_EVENT() is called.
    ON_BoundingBox m_bboxA; // 2D surfA bounding box of the closed region.
    enum TYPE {
	undefined = 0,
	outer = 1,
	inner = 2
    } m_type;

    std::vector<Overlapevent *> m_inside;

    Overlapevent() {}

    Overlapevent(ON_SSX_EVENT *_e) : m_event(_e)
    {
	m_type = undefined;
	m_bboxA = _e->m_curveA->BoundingBox();
    }

    bool IsPointOnBoundary(const ON_2dPoint &_pt) const
    {
	ON_ClassArray<ON_PX_EVENT> px_event;
	return ON_Intersect(ON_3dPoint(_pt), *(m_event->m_curveA), px_event);
    }

    // Test whether a point is inside the closed region (including boundary).
    // Approach: Use line-curve intersections. If the line between this point
    // and a point outside the b-box has an odd number of intersections with
    // the boundary, it's considered inside, otherwise outside. (Tangent
    // intersections are counted too.)
    bool IsPointIn(const ON_2dPoint &_pt) const
    {
	if (!m_bboxA.IsPointIn(_pt)) {
	    return false;
	}
	if (IsPointOnBoundary(_pt)) {
	    return true;
	}
	ON_ClassArray<ON_PX_EVENT> px_event;
	// this point must be outside the closed region
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

    // return false if the point is on the boundary
    bool IsPointOut(const ON_2dPoint &_pt) const
    {
	return !IsPointIn(_pt);
    }

    // Test whether there are intersections between the box boundaries
    // and the boundary curve.
    // If there are not intersection, there can be 3 cases:
    // 1) The box is completely in.
    // 2) The box is completely out.
    // 3) The box completely contains the closed region bounded by that curve.
    bool IsBoxIntersected(const ON_2dPoint &_min, const ON_2dPoint &_max) const
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

    bool IsBoxCompletelyIn(const ON_2dPoint &_min, const ON_2dPoint &_max) const
    {
	// there should not be intersections between the box boundaries
	// and the boundary curve
	if (IsBoxIntersected(_min, _max)) {
	    return false;
	}
	ON_2dPoint center = (_min + _max) * 0.5;
	return m_bboxA.Includes(ON_BoundingBox(_min, _max)) && IsPointIn(center);
    }

    bool IsBoxCompletelyOut(const ON_2dPoint &_min, const ON_2dPoint &_max) const
    {
	if (IsBoxIntersected(_min, _max)) {
	    return false;
	}
	ON_2dPoint center = (_min + _max) * 0.5;
	return !IsPointIn(center) && !ON_BoundingBox(_min, _max).Includes(m_bboxA);
    }

    bool IsCurveCompletelyIn(const ON_Curve *_curve) const
    {
	ON_SimpleArray<ON_X_EVENT> x_event;
	return !ON_Intersect(m_event->m_curve3d, _curve, x_event) && IsPointIn(_curve->PointAtStart());
    }
};

HIDDEN bool
is_pt_in_surf_overlap(
	ON_2dPoint surf1_pt,
	const ON_Surface *surf1,
	const ON_Surface *surf2,
	Subsurface *surf2_tree)
{
    if (!surf1->Domain(0).Includes(surf1_pt.x) ||
	!surf1->Domain(1).Includes(surf1_pt.y))
    {
	return false;
    }
    bool surf1_pt_intersects_surf2, surfs_parallel_at_pt;
    ON_ClassArray<ON_PX_EVENT> px_event;

    surf1_pt_intersects_surf2 = ON_Intersect(surf1->PointAt(surf1_pt.x, surf1_pt.y),
	    *surf2, px_event, 1.0e-5, 0, 0, surf2_tree);

    if (surf1_pt_intersects_surf2) {
	surfs_parallel_at_pt = surf1->NormalAt(surf1_pt.x, surf1_pt.y).IsParallelTo(
		surf2->NormalAt(px_event[0].m_b[0], px_event[0].m_b[1]));
    }
    return surf1_pt_intersects_surf2 && surfs_parallel_at_pt;
}

HIDDEN bool
is_subsurfaceA_completely_inside_overlap(
	const ON_SimpleArray<Overlapevent> &overlap_events,
	const Subsurface *subA,
	double isect_tolA)
{
    ON_2dPoint bbox_min(subA->m_u.Min(), subA->m_v.Min());
    ON_2dPoint bbox_max(subA->m_u.Max(), subA->m_v.Max());

    // shrink bbox slightly
    ON_2dPoint offset(isect_tolA, isect_tolA);
    bbox_min += offset;
    bbox_max -= offset;

    // the bbox is considered to be inside an overlap if it's inside
    // an Overlapevent's outer loop and outside any of its inner loops
    bool inside_overlap = false;
    for (int i = 0; i < overlap_events.Count() && !inside_overlap; ++i) {
	const Overlapevent *outerloop = &overlap_events[i];
	if (outerloop->m_type == Overlapevent::outer &&
	    outerloop->IsBoxCompletelyIn(bbox_min, bbox_max))
	{
	    inside_overlap = true;
	    for (size_t j = 0; j < outerloop->m_inside.size(); ++j) {
		const Overlapevent *innerloop = outerloop->m_inside[j];
		if (innerloop->m_type == Overlapevent::inner &&
		    !innerloop->IsBoxCompletelyOut(bbox_min, bbox_max))
		{
		    inside_overlap = false;
		    break;
		}
	    }
	}
    }
    return inside_overlap;
}

HIDDEN bool
is_uvA_completely_inside_overlap(
	const ON_SimpleArray<Overlapevent> &overlap_events,
	const ON_2dPoint &pt)
{
    // the pt is considered to be inside an overlap if it's inside
    // an Overlapevent's outer loop and outside any of its inner loops
    bool inside_overlap = false;
    for (int i = 0; i < overlap_events.Count() && !inside_overlap; ++i) {
	const Overlapevent *outerloop = &overlap_events[i];
	if (outerloop->m_type == Overlapevent::outer &&
	    outerloop->IsPointIn(pt))
	{
	    inside_overlap = true;
	    for (size_t j = 0; j < outerloop->m_inside.size(); ++j) {
		const Overlapevent *innerloop = outerloop->m_inside[j];
		if (innerloop->m_type == Overlapevent::inner &&
		    innerloop->IsPointIn(pt) &&
		    !innerloop->IsPointOnBoundary(pt))
		{
		    inside_overlap = false;
		    break;
		}
	    }
	}
    }
    return inside_overlap;
}

enum {
    INSIDE_OVERLAP,
    ON_OVERLAP_BOUNDARY,
    OUTSIDE_OVERLAP
};

HIDDEN int
isocurve_surface_overlap_location(
	const ON_Surface *surf1,
	double iso_t,
	int iso_dir,
	double overlap_start,
	double overlap_end,
	const ON_Surface *surf2,
	Subsurface *surf2_tree)
{
    double test_distance = 0.01;

    // TODO: more sample points
    double u1, v1, u2, v2;
    double midpt = (overlap_start + overlap_end ) * 0.5;
    if (iso_dir == 0) {
	u1 = u2 = midpt;
	v1 = iso_t - test_distance;
	v2 = iso_t + test_distance;
    } else {
	u1 = iso_t - test_distance;
	u2 = iso_t + test_distance;
	v1 = v2 = midpt;
    }

    bool in1, in2;
    ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;

    in1 = is_pt_in_surf_overlap(ON_2dPoint(u1, v1), surf1, surf2, surf2_tree);
    in2 = is_pt_in_surf_overlap(ON_2dPoint(u2, v2), surf1, surf2, surf2_tree);

    if (in1 && in2) {
	return INSIDE_OVERLAP;
    }
    if (!in1 && !in2) {
	return OUTSIDE_OVERLAP;
    }
    return ON_OVERLAP_BOUNDARY;
}

ON_ClassArray<ON_3dPointArray>
get_overlap_intersection_parameters(
	const ON_SimpleArray<OverlapSegment *> &overlaps,
	const ON_Surface *surfA,
	const ON_Surface *surfB,
	Subsurface *treeA,
	Subsurface *treeB,
	double isect_tol,
	double isect_tolA,
	double isect_tolB)
{
    int count_before = overlaps.Count();

    ON_ClassArray<ON_3dPointArray> params(count_before);

    // count must equal capacity for array copy to work as expected
    // when the result of the function is assigned
    params.SetCount(params.Capacity());

    for (int i = 0; i < count_before; i++) {
	// split the curves from the intersection points between them
	int count = params[i].Count();
	for (int j = i + 1; j < count_before; j++) {
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d, x_event, isect_tol);
	    count += x_event.Count();
	    for (int k = 0; k < x_event.Count(); k++) {
		ON_3dPoint param;
		ON_2dPoint uvA, uvB;
		ON_ClassArray<ON_PX_EVENT> psxA, psxB, pcxA, pcxB;

		// ensure that the curve intersection point also
		// intersects both surfaces (i.e. the point is in the
		// overlap)
		if (ON_Intersect(x_event[k].m_A[0], *surfA, psxA, isect_tolA, 0, 0, treeA)
		    && ON_Intersect(x_event[k].m_B[0], *surfB, psxB, isect_tolB, 0, 0, treeB)) {
		    // pull the 3D curve back to the 2D space
		    uvA = psxA[0].m_b;
		    uvB = psxB[0].m_b;

		    // Convert the uv points into the equivalent curve
		    // parameters by intersecting them with the 2D
		    // versions of the curves. The 2D intersection is
		    // done in 3D; note that uvA/uvB are implicitly
		    // converted to ON_3dPoint with z=0.0.
		    if (ON_Intersect(uvA, *(overlaps[i]->m_curveA), pcxA, isect_tolA)
			&& ON_Intersect(uvB, *(overlaps[i]->m_curveB), pcxB, isect_tolB)) {
			param.x = x_event[k].m_a[0];
			param.y = pcxA[0].m_b[0];
			param.z = pcxB[0].m_b[0];
			params[i].Append(param);
		    }
		    pcxA.SetCount(0);
		    pcxB.SetCount(0);
		    if (ON_Intersect(uvA, *(overlaps[j]->m_curveA), pcxA, isect_tolA)
			&& ON_Intersect(uvB, *(overlaps[j]->m_curveB), pcxB, isect_tolB)) {
			// the same routine for overlaps[j]
			param.x = x_event[k].m_b[0];
			param.y = pcxA[0].m_b[0];
			param.z = pcxB[0].m_b[0];
			params[j].Append(param);
		    }
		}
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap) {
		    // for overlap, have to do the same with the intersection end points
		    psxA.SetCount(0);
		    psxB.SetCount(0);
		    if (ON_Intersect(x_event[k].m_A[1], *surfA, psxA, isect_tol, 0, 0, treeA)
			&& ON_Intersect(x_event[k].m_B[1], *surfB, psxB, isect_tol, 0, 0, treeB)) {
			// pull the 3D curve back to the 2D space
			uvA = psxA[0].m_b;
			uvB = psxB[0].m_b;
			pcxA.SetCount(0);
			pcxB.SetCount(0);
			if (ON_Intersect(uvA, *(overlaps[i]->m_curveA), pcxA, isect_tolA)
			    && ON_Intersect(uvB, *(overlaps[i]->m_curveB), pcxB, isect_tolB)) {
			    param.x = x_event[k].m_a[1];
			    param.y = pcxA[0].m_b[0];
			    param.z = pcxB[0].m_b[0];
			    params[i].Append(param);
			}
			pcxA.SetCount(0);
			pcxB.SetCount(0);
			if (ON_Intersect(uvA, *(overlaps[j]->m_curveA), pcxA, isect_tolA)
			    && ON_Intersect(uvB, *(overlaps[j]->m_curveB), pcxB, isect_tolB)) {
			    // the same routine for overlaps[j]
			    param.x = x_event[k].m_b[1];
			    param.y = pcxA[0].m_b[0];
			    param.z = pcxB[0].m_b[0];
			    params[j].Append(param);
			}
		    }
		}
	    }
	}
    }

    return params;
}

HIDDEN bool
is_valid_overlap(const OverlapSegment *overlap)
{
    if (overlap            &&
	overlap->m_curve3d &&
	overlap->m_curveA  &&
	overlap->m_curveB) {
	return true;
    }
    return false;
}

HIDDEN void
split_overlaps_at_intersections(
	ON_SimpleArray<OverlapSegment *> &overlaps,
	const ON_Surface *surfA,
	const ON_Surface *surfB,
	Subsurface *treeA,
	Subsurface *treeB,
	double isect_tol,
	double isect_tolA,
	double isect_tolB)
{
    // Not points, but a bundle of params:
    // params[i] - the separating params on overlaps[i]
    // x - curve3d, y - curveA, z - curveB
    ON_ClassArray<ON_3dPointArray> params = get_overlap_intersection_parameters(
	    overlaps, surfA, surfB, treeA, treeB, isect_tol,
	    isect_tolA, isect_tolB);

    // split overlap curves at intersection parameters into subcurves
    for (int i = 0; i < params.Count(); i++) {
	params[i].QuickSort(ON_CompareIncreasing);
	int start = 0;
	bool splitted = false;
	for (int j = 1; j < params[i].Count(); j++) {
	    if (params[i][j].x - params[i][start].x < isect_tol) {
		continue;
	    }
	    ON_Curve *subcurveA = sub_curve(overlaps[i]->m_curveA, params[i][start].y, params[i][j].y);
	    if (subcurveA == NULL) {
		continue;
	    }
	    bool isvalid = false, isreversed = false;
	    double test_distance = 0.01;
	    // TODO: more sample points
	    ON_2dPoint uv1, uv2;
	    uv1 = uv2 = subcurveA->PointAt(subcurveA->Domain().Mid());
	    ON_3dVector normal = ON_CrossProduct(subcurveA->TangentAt(subcurveA->Domain().Mid()), ON_3dVector::ZAxis);
	    normal.Unitize();
	    uv1 -= normal * test_distance;	// left
	    uv2 += normal * test_distance;	// right
	    bool in1 = is_pt_in_surf_overlap(uv1, surfA, surfB, treeB);
	    bool in2 = is_pt_in_surf_overlap(uv2, surfA, surfB, treeB);
	    if (in1 && !in2) {
		isvalid = true;
	    } else if (!in1 && in2) {
		// the right side is overlapped
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
	    } else {
		delete subcurveA;
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
	    if (!is_valid_overlap(overlaps[i]) || !is_valid_overlap(overlaps[j])) {
		continue;
	    }
	    // eliminate duplications
	    ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
	    if (ON_Intersect(overlaps[i]->m_curveA, overlaps[j]->m_curveA, x_event1, isect_tolA)
		&& ON_Intersect(overlaps[i]->m_curveB, overlaps[j]->m_curveB, x_event2, isect_tolB)) {
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
}

HIDDEN std::vector<Subsurface *>
subdivide_subsurface(Subsurface *sub)
{
    std::vector<Subsurface *> parts;
    if (sub->Split() != 0) {
	parts.push_back(sub);
    } else {
	for (int i = 0; i < 4; ++i) {
	    parts.push_back(sub->m_children[i]);
	}
    }
    return parts;
}

HIDDEN std::pair<Triangle, Triangle>
get_subsurface_triangles(const Subsurface *sub, const ON_Surface *surf)
{
    double min_u = sub->m_u.Min();
    double max_u = sub->m_u.Max();
    double min_v = sub->m_v.Min();
    double max_v = sub->m_v.Max();

    ON_2dPoint corners2d[4] = {
	ON_2dPoint(min_u, min_v),
	ON_2dPoint(min_u, max_v),
	ON_2dPoint(max_u, min_v),
	ON_2dPoint(max_u, max_v)};

    ON_3dPoint corners3d[4];
    for (int i = 0; i < 4; ++i) {
	corners3d[i] = surf->PointAt(corners2d[i].x, corners2d[i].y);
    }

    std::pair<Triangle, Triangle> triangles;
    triangles.first.a_2d = corners2d[0];
    triangles.first.b_2d = corners2d[1];
    triangles.first.c_2d = corners2d[2];
    triangles.first.a = corners3d[0];
    triangles.first.b = corners3d[1];
    triangles.first.c = corners3d[2];
    triangles.second.a_2d = corners2d[1];
    triangles.second.b_2d = corners2d[2];
    triangles.second.c_2d = corners2d[3];
    triangles.second.a = corners3d[1];
    triangles.second.b = corners3d[2];
    triangles.second.c = corners3d[3];

    return triangles;
}

HIDDEN ON_2dPoint
barycentric_to_uv(const ON_3dPoint &bc, const Triangle &tri)
{
    ON_3dVector vertx(tri.a_2d.x, tri.b_2d.x, tri.c_2d.x);
    ON_3dVector verty(tri.a_2d.y, tri.b_2d.y, tri.c_2d.y);
    ON_2dPoint uv(ON_DotProduct(bc, vertx), ON_DotProduct(bc, verty));
    return uv;
}

HIDDEN bool
join_continuous_ppair_to_polyline(ON_SimpleArray<int> *polyline, const PointPair &ppair)
{
    int polyline_first = (*polyline)[0];
    int polyline_last = (*polyline)[polyline->Count() - 1];

    if (polyline_first == ppair.indexA) {
	polyline->Insert(0, ppair.indexB);
	return true;
    }
    if (polyline_first == ppair.indexB){
	polyline->Insert(0, ppair.indexA);
	return true;
    }
    if (polyline_last == ppair.indexA){
	polyline->Append(ppair.indexB);
	return true;
    }
    if (polyline_last == ppair.indexB){
	polyline->Append(ppair.indexA);
	return true;
    }
    return false;
}

// Algorithm Overview
//
// 1) Find overlap intersections (regions where the two surfaces are
//    flush to one another).
//    a) Intersect the isocurves of surfA with surfB and vice versa
//       (any intersection points found are saved for later).
//    b) We save any overlap intersection curves that appear to be
//       boundary curves (curves which contain part of the boundary
//       of the overlap region).
//       i) If the overlap intersection curve is at the edge of the
//          surface, it's automatically a boundary curve.
//      ii) If the surfaces overlap on only one side of the overlap
//          intersection curve, it's a boundary curve (or at least
//          contains a boundary curve).
//    c) Because our boundary test is rather simple, not all of the
//       saved overlap curves actually overlap the surfaces over their
//       whole domain, so we do additional processing to get the real
//       overlaps.
//       i) The overlap curves are intersected with each other.
//      ii) The curves are split into subcurves at the points where
//          they intersect.
//     iii) We retest the subcurves to see if they are truly part of
//          the overlap region boundary, discarding those that aren't.
//      iv) Any overlap curve which is completely contained by another
//          overlap curve is discarded, hopefully leaving a set of
//          genuine and unique overlap curves.
//    d) Overlap curves that share endpoints are merged together.
//    e) Any of the resulting overlap curves which don't form closed
//       loops are discarded.
//    f) The overlap loops are turned into overlap intersection
//       events, with the orientation of outer and inner loops
//       adjusted as needed to ensure the overlapping regions are
//       correctly defined.
// 2) Find non-overlap intersections.
//    a) The two surfaces are repeatedly subdivided into subsurfaces
//       until the MAX_SSI_DEPTH is reached. The subsurface pairs
//       whose bounding boxes intersect are saved.
//    b) Each pair of potentially intersecting subsurfaces is
//       intersected.
//       i) Each subsurface is approximated by two triangles which
//          share an edge.
//      ii) The triangles are intersected.
//     iii) The Newton solver is used to get the center point of the
//          intersection, using the averaged center point of the
//          triangle intersections as the initial guess.
//    c) The center points of the subsurface intersections are
//       combined with the isocurve/surface intersection points found
//       in step 1) above.
//    d) For surface seams that are closed, additional points are
//       generated on the seams (3d points near the seams are copied,
//       and their uv values are adjusted to put them over the seam).
//    e) The list of intersection points is reduced to just the unique
//       intersection points that are not inside any overlap regions.
//    f) Polylines are created from the intersection points.
//       i) Each point starts as its own polyline, and serves as both
//          endpoints of the line.
//      ii) The closest pair of endpoints are joined together, then
//          the next closest, and so on, growing the length of the
//          polylines.
//      iv) Endpoints that are too far apart (distance > max_dist) are
//          not joined, and some intersection points may remain by
//          themselves.
//    g) Intersection curves are generated from the polylines, and are
//       used to make intersection events.
//    h) Any single intersection points found in step 2-f-iv) above
//       that aren't contained by one of the generated intersection
//       curves are used to make additional point intersection
//       events.
int
ON_Intersect(const ON_Surface *surfA,
	     const ON_Surface *surfB,
	     ON_ClassArray<ON_SSX_EVENT> &x,
	     double isect_tol,
	     double overlap_tol,
	     double fitting_tol,
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

    if (isect_tol <= 0.0) {
	isect_tol = SSI_DEFAULT_TOLERANCE;
    }
    if (overlap_tol < isect_tol) {
	overlap_tol = 2.0 * isect_tol;
    }
    if (fitting_tol < isect_tol) {
	fitting_tol = isect_tol;
    }

    check_domain(surfaceA_udomain, surfA->Domain(0), "surfaceA_udomain");
    check_domain(surfaceA_vdomain, surfA->Domain(1), "surfaceA_vdomain");
    check_domain(surfaceB_udomain, surfB->Domain(0), "surfaceB_udomain");
    check_domain(surfaceB_vdomain, surfB->Domain(1), "surfaceB_vdomain");

    /* First step: Initialize the first two Subsurface.
     * It's just like getting the root of the surface tree.
     */
    Subsurface *rootA = treeA;
    Subsurface *rootB = treeB;
    try {
	if (rootA == NULL) {
	    rootA = get_surface_root(surfA, surfaceA_udomain,
				     surfaceA_vdomain);
	}
	if (rootB == NULL) {
	    rootB = get_surface_root(surfB, surfaceB_udomain,
				     surfaceB_vdomain);
	}
    } catch (std::bad_alloc &e) {
	bu_log("%s", e.what());
	if (treeA == NULL) {
	    delete rootA;
	}
	return 0;
    }

    if (!rootA->Intersect(*rootB, isect_tol)) {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return 0;
    }

    double isect_tolA =
	tolerance_2d_from_3d(isect_tol, rootA, surfA,
		surfaceA_udomain, surfaceA_vdomain);

    double fitting_tolA = tolerance_2d_from_3d(fitting_tol,
	    rootA, surfA, surfaceA_udomain, surfaceA_vdomain);

    double isect_tolB =
	tolerance_2d_from_3d(isect_tol, rootB, surfB,
		surfaceB_udomain, surfaceB_vdomain);

    double fitting_tolB = tolerance_2d_from_3d(fitting_tol,
	    rootB, surfB, surfaceB_udomain, surfaceB_vdomain);

    ON_3dPointArray curvept, tmp_curvept;
    ON_2dPointArray curve_uvA, curve_uvB, tmp_curve_uvA, tmp_curve_uvB;

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
	ON_2dPointArray &ptarray1 = i >= 2 ? tmp_curve_uvB : tmp_curve_uvA;
	ON_2dPointArray &ptarray2 = i >= 2 ? tmp_curve_uvA : tmp_curve_uvB;
	int knot_dir = i % 2;
	int surf_dir = 1 - knot_dir;
	int knot_count = surf1->SpanCount(surf_dir) + 1;
	double *knots = new double[knot_count];
	surf1->GetSpanVector(surf_dir, knots);
	// knots that can be boundaries of Bezier patches
	ON_SimpleArray<double> b_knots;
	b_knots.Append(knots[0]);
	for (int j = 1; j < knot_count; j++) {
	    if (knots[j] > *(b_knots.Last())) {
		b_knots.Append(knots[j]);
	    }
	}
	if (surf1->IsClosed(surf_dir)) {
	    b_knots.Remove();
	}

	for (int j = 0; j < b_knots.Count(); j++) {
	    double knot = b_knots[j];
	    ON_Curve *boundary = surf1->IsoCurve(knot_dir, knot);
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_CurveArray overlap2d;
	    ON_Intersect(boundary, surf2, x_event, isect_tol, overlap_tol, 0, 0, 0, &overlap2d);

	    // stash surf1 points and surf2 parameters
	    for (int k = 0; k < x_event.Count(); k++) {
		ON_X_EVENT &event = x_event[k];

		tmp_curvept.Append(event.m_A[0]);
		ptarray2.Append(ON_2dPoint(event.m_b[0], event.m_b[1]));

		if (event.m_type == ON_X_EVENT::csx_overlap) {
		    tmp_curvept.Append(event.m_A[1]);
		    ptarray2.Append(ON_2dPoint(event.m_b[2], event.m_b[3]));
		}
	    }

	    for (int k = 0; k < x_event.Count(); k++) {
		ON_X_EVENT &event = x_event[k];

		ON_2dPoint iso_pt1;
		iso_pt1.x = knot_dir ? knot : event.m_a[0];
		iso_pt1.y = knot_dir ? event.m_a[0] : knot;
		ptarray1.Append(iso_pt1);

		if (event.m_type == ON_X_EVENT::csx_overlap) {
		    ON_2dPoint iso_pt2;
		    iso_pt2.x = knot_dir ? knot : event.m_a[1];
		    iso_pt2.y = knot_dir ? event.m_a[1] : knot;
		    ptarray1.Append(iso_pt2);
		    // get the overlap curve
		    if (event.m_a[0] < event.m_a[1]) {
			bool curve_on_overlap_boundary = false;
			if (j == 0 || (j == b_knots.Count() - 1 && !surf1->IsClosed(surf_dir))) {
			    curve_on_overlap_boundary = true;
			} else {
			    double overlap_start = event.m_a[0];
			    double overlap_end = event.m_a[1];
			    int location = isocurve_surface_overlap_location(
				    surf1, knot, knot_dir, overlap_start, overlap_end, surf2, tree);
			    curve_on_overlap_boundary = (location == ON_OVERLAP_BOUNDARY);
			}
			if (curve_on_overlap_boundary) {
			    // one side of it is shared and the other side is non-shared
			    OverlapSegment *seg = new OverlapSegment;
			    seg->m_curve3d = sub_curve(boundary, event.m_a[0], event.m_a[1]);
			    if (i < 2) {
				seg->m_curveA = new ON_LineCurve(iso_pt1, iso_pt2);
				seg->m_curveB = overlap2d[k];
			    } else {
				seg->m_curveB = new ON_LineCurve(iso_pt1, iso_pt2);
				seg->m_curveA = overlap2d[k];
			    }
			    seg->m_dir = surf_dir;
			    seg->m_fix = knot;
			    overlaps.Append(seg);
			    if (j == 0 && surf1->IsClosed(surf_dir)) {
				// Something like close_domain().
				// If the domain is closed, the iso-curve on the
				// first knot and the last knot is the same, so
				// we don't need to compute the intersections twice.
				seg = new OverlapSegment;
				iso_pt1.x = knot_dir ? knots[knot_count - 1] : event.m_a[0];
				iso_pt1.y = knot_dir ? event.m_a[0] : knots[knot_count - 1];
				iso_pt2.x = knot_dir ? knots[knot_count - 1] : event.m_a[1];
				iso_pt2.y = knot_dir ? event.m_a[1] : knots[knot_count - 1];
				seg->m_curve3d = (*overlaps.Last())->m_curve3d->Duplicate();
				if (i < 2) {
				    seg->m_curveA = new ON_LineCurve(iso_pt1, iso_pt2);
				    seg->m_curveB = overlap2d[k]->Duplicate();
				} else {
				    seg->m_curveB = new ON_LineCurve(iso_pt1, iso_pt2);
				    seg->m_curveA = overlap2d[k]->Duplicate();
				}
				seg->m_dir = surf_dir;
				seg->m_fix = knots[knot_count - 1];
				overlaps.Append(seg);
			    }
			    // We set overlap2d[k] to NULL in case the curve
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

    split_overlaps_at_intersections(overlaps, surfA, surfB, treeA, treeB,
	    isect_tol, isect_tolA, isect_tolB);

    // find the neighbors for every overlap segment
    ON_SimpleArray<bool> start_linked(overlaps.Count()), end_linked(overlaps.Count());
    for (int i = 0; i < overlaps.Count(); i++) {
	start_linked[i] = end_linked[i] = false;
    }
    for (int i = 0; i < overlaps.Count(); i++) {
	if (!is_valid_overlap(overlaps[i])) {
	    continue;
	}

	if (overlaps[i]->m_curve3d->IsClosed() && overlaps[i]->m_curveA->IsClosed() && overlaps[i]->m_curveB->IsClosed()) {
	    start_linked[i] = end_linked[i] = true;
	}

	for (int j = i + 1; j < overlaps.Count(); j++) {
	    if (!is_valid_overlap(overlaps[j])) {
		continue;
	    }

	    // check whether the start and end points are linked to
	    // another curve
#define OVERLAPS_LINKED(from, to) \
    overlaps[i]->m_curve3d->PointAt##from().DistanceTo(overlaps[j]->m_curve3d->PointAt##to()) < isect_tol && \
    overlaps[i]->m_curveA->PointAt##from().DistanceTo(overlaps[j]->m_curveA->PointAt##to()) < isect_tolA && \
    overlaps[i]->m_curveB->PointAt##from().DistanceTo(overlaps[j]->m_curveB->PointAt##to()) < isect_tolB

	    if (OVERLAPS_LINKED(Start, End)) {
		start_linked[i] = end_linked[j] = true;
	    } else if (OVERLAPS_LINKED(End, Start)) {
		start_linked[j] = end_linked[i] = true;
	    } else if (OVERLAPS_LINKED(Start, Start)) {
		start_linked[i] = start_linked[j] = true;
	    } else if (OVERLAPS_LINKED(End, End)) {
		end_linked[i] = end_linked[j] = true;
	    }
	}
    }

    for (int i = 0; i < overlaps.Count(); i++) {
	if (!is_valid_overlap(overlaps[i])) {
	    continue;
	}
	if (!start_linked[i] || !end_linked[i]) {
	    delete overlaps[i];
	    overlaps[i] = NULL;
	}
    }

    for (int i = 0; i < overlaps.Count(); i++) {
	if (!is_valid_overlap(overlaps[i])) {
	    continue;
	}

	for (int j = 0; j <= overlaps.Count(); j++) {
	    if (overlaps[i]->m_curve3d->IsClosed() && overlaps[i]->m_curveA->IsClosed() && overlaps[i]->m_curveB->IsClosed()) {
		// i-th curve is a closed loop, completely bounding an overlap region
		ON_SSX_EVENT event;
		event.m_curve3d = curve_fitting(overlaps[i]->m_curve3d, fitting_tol);
		event.m_curveA = curve_fitting(overlaps[i]->m_curveA, fitting_tolA);
		event.m_curveB = curve_fitting(overlaps[i]->m_curveB, fitting_tolB);
		event.m_type = ON_SSX_EVENT::ssx_overlap;
		// normalize the curves
		event.m_curve3d->SetDomain(ON_Interval(0.0, 1.0));
		event.m_curveA->SetDomain(ON_Interval(0.0, 1.0));
		event.m_curveB->SetDomain(ON_Interval(0.0, 1.0));
		x.Append(event);
		// Set the curves to NULL in case they are deleted by
		// ~ON_SSX_EVENT() or ~ON_CurveArray().
		event.m_curve3d = event.m_curveA = event.m_curveB = NULL;
		overlaps[i]->SetCurvesToNull();
		delete overlaps[i];
		overlaps[i] = NULL;
		break;
	    }

	    if (j == overlaps.Count() || j == i || !is_valid_overlap(overlaps[j])) {
		continue;
	    }

	    // merge the curves that link together
	    if (OVERLAPS_LINKED(Start, End)) {
		overlaps[i]->m_curve3d = link_curves(overlaps[j]->m_curve3d, overlaps[i]->m_curve3d);
		overlaps[i]->m_curveA = link_curves(overlaps[j]->m_curveA, overlaps[i]->m_curveA);
		overlaps[i]->m_curveB = link_curves(overlaps[j]->m_curveB, overlaps[i]->m_curveB);
	    } else if (OVERLAPS_LINKED(End, Start)) {
		overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
	    } else if (OVERLAPS_LINKED(Start, Start)) {
		if (overlaps[i]->m_curve3d->Reverse() && overlaps[i]->m_curveA->Reverse() && overlaps[i]->m_curveB->Reverse()) {
		    overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		    overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		    overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
		}
	    } else if (OVERLAPS_LINKED(End, End)) {
		if (overlaps[j]->m_curve3d->Reverse() && overlaps[j]->m_curveA->Reverse() && overlaps[j]->m_curveB->Reverse()) {
		    overlaps[i]->m_curve3d = link_curves(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d);
		    overlaps[i]->m_curveA = link_curves(overlaps[i]->m_curveA, overlaps[j]->m_curveA);
		    overlaps[i]->m_curveB = link_curves(overlaps[i]->m_curveB, overlaps[j]->m_curveB);
		}
	    }
	    if (!is_valid_overlap(overlaps[j])) {
		delete overlaps[j];
		overlaps[j] = NULL;
	    }
	}
    }

    // generate Overlapevents
    ON_SimpleArray<Overlapevent> overlap_events;
    for (int i = original_count; i < x.Count(); i++) {
	overlap_events.Append(Overlapevent(&x[i]));
    }

    for (int i = original_count; i < x.Count(); i++) {
	// The overlap region should be to the LEFT of that *m_curveA*.
	// (see opennurbs/opennurbs_x.h)

	// find a point with a single tangent
	double start_t = x[i].m_curveA->Domain().Mid();
	if (!x[i].m_curveA->IsContinuous(ON::G1_continuous, start_t)) {
	    // if mid point doesn't work, try a couple other points
	    // TODO: what happens if none of them work?
	    start_t = x[i].m_curveA->Domain().NormalizedParameterAt(1.0 / 3.0);
	    if (!x[i].m_curveA->IsContinuous(ON::G1_continuous, start_t)) {
		start_t = x[i].m_curveA->Domain().NormalizedParameterAt(2.0 / 3.0);
	    }
	}

	ON_3dPoint start_ptA = x[i].m_curveA->PointAt(start_t);
	ON_3dVector normA = ON_CrossProduct(ON_3dVector::ZAxis,
		x[i].m_curveA->TangentAt(start_t));
	double line_len = 1.0 + x[i].m_curveA->BoundingBox().Diagonal().Length();

	// get a line that should be extending left, through the loop
	ON_3dPoint left_ptA = start_ptA + normA * line_len;
	ON_LineCurve inside_lineA(start_ptA, left_ptA);

	// get a line that should be extending right, away from the loop
	ON_3dPoint right_ptA = start_ptA - normA * line_len;
	ON_LineCurve outside_lineA(start_ptA, right_ptA);

	// outside line should intersect at start only
	// inside line should intersect at start and exit
	// otherwise we've got them backwards
	// TODO: seems like this should really be an even/odd test
	ON_SimpleArray<ON_X_EVENT> inside_events, outside_events;
	ON_Intersect(&inside_lineA, x[i].m_curveA, inside_events, isect_tol);
	ON_Intersect(&outside_lineA, x[i].m_curveA, outside_events, isect_tol);
	std::vector<double> line_t;
	if (inside_events.Count() != 2 && outside_events.Count() == 2) {
	    // the direction of the curve should be opposite
	    x[i].m_curve3d->Reverse();
	    x[i].m_curveA->Reverse();
	    x[i].m_curveB->Reverse();
	    inside_lineA = outside_lineA;
	    line_t.push_back(outside_events[0].m_a[0]);
	    line_t.push_back(outside_events[1].m_a[0]);
	} else if (inside_events.Count() == 2 && outside_events.Count() != 2) {
	    line_t.push_back(inside_events[0].m_a[0]);
	    line_t.push_back(inside_events[1].m_a[0]);
	} else {
	    bu_log("error: Cannot determine the correct direction of overlap event %d\n", i);
	    continue;
	}

	// need to reverse inner loops to indicate overlap is outside
	// the closed region
	for (int j = original_count; j < x.Count(); j++) {
	    // any curves that intersect the line crossing through the
	    // loop may be inside the loop
	    // FIXME: What about curves inside the loop that the line
	    //        happens to miss?
	    if (i == j) {
		continue;
	    }
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(&inside_lineA, x[j].m_curveA, x_event, isect_tol);
	    for (int k = 0; k < x_event.Count(); k++) {
		line_t.push_back(x_event[k].m_a[0]);
	    }
	}

	// normalize all params in line_t
	for (size_t j = 0; j < line_t.size(); j++) {
	    line_t[j] = inside_lineA.Domain().NormalizedParameterAt(line_t[j]);
	}

	std::sort(line_t.begin(), line_t.end());
	if (!ON_NearZero(line_t[0])) {
	    bu_log("Error: param 0 is not in line_t!\n");
	    continue;
	}

	// get a test point inside the region
	ON_3dPoint test_pt2d = inside_lineA.PointAt(line_t[1] * 0.5);
	ON_3dPoint test_pt = surfA->PointAt(test_pt2d.x, test_pt2d.y);
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (!ON_Intersect(test_pt, *surfB, px_event, overlap_tol, 0, 0, treeB)) {
	    // the test point is not overlapped
	    x[i].m_curve3d->Reverse();
	    x[i].m_curveA->Reverse();
	    x[i].m_curveB->Reverse();
	    overlap_events[i].m_type = Overlapevent::inner;
	} else {
	    // the test point inside that region is an overlap point
	    overlap_events[i].m_type = Overlapevent::outer;
	}
    }

    // find the inner events inside each outer event
    for (int i = 0; i < overlap_events.Count(); i++) {
	for (int j = 0; j < overlap_events.Count(); j++) {
	    if (i == j || overlap_events[i].m_type != Overlapevent::outer || overlap_events[j].m_type != Overlapevent::inner) {
		continue;
	    }
	    if (overlap_events[i].IsCurveCompletelyIn(overlap_events[j].m_event->m_curve3d)) {
		overlap_events[i].m_inside.push_back(&(overlap_events[j]));
	    }
	}
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d overlap events.\n", overlap_events.Count());
    }

    if (surfA->IsPlanar() && surfB->IsPlanar() && overlap_events.Count()) {
	return x.Count() - original_count;
    }

    /* Second step: calculate the intersection of the bounding boxes.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     */
    typedef std::vector<std::pair<Subsurface *, Subsurface *> > NodePairs;
    NodePairs candidates, next_candidates;
    candidates.push_back(std::make_pair(rootA, rootB));

    for (int h = 0; h <= MAX_SSI_DEPTH && !candidates.empty(); h++) {
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    // If the Subsurface from either surfA or surfB (we're
	    // checking surfA in this case) is completely inside an
	    // overlap region, then we know that there is no
	    // (non-overlap) intersection between this pair of
	    // Subsurfaces, and thus no point in subdividing them
	    // further.
	    if (is_subsurfaceA_completely_inside_overlap(overlap_events, i->first, isect_tolA)) {
		continue;
	    }

	    // subdivide and add the parts whose bounding boxes
	    // intersect to the new list of candidates
	    std::vector<Subsurface *> partsA = subdivide_subsurface(i->first);
	    std::vector<Subsurface *> partsB = subdivide_subsurface(i->second);
	    for (size_t j = 0; j < partsA.size(); ++j) {
		for (size_t k = 0; k < partsB.size(); ++k) {
		    if (partsB[k]->Intersect(*partsA[j], isect_tol)) {
			next_candidates.push_back(std::make_pair(partsA[j], partsB[k]));
		    }
 		}
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
	// Get an estimate of the intersection point lying on the intersection curve.

	// Get the corners of each surface sub-patch inside the bounding-box.
	ON_BoundingBox box_intersect;
	i->first->Intersect(*(i->second), isect_tol, &box_intersect);

	/* We approximate each surface sub-patch inside the bounding-box with two
	 * triangles that share an edge.
	 * The intersection of the surface sub-patches is approximated as the
	 * intersection of triangles.
	 */
	std::pair<Triangle, Triangle> trianglesA = get_subsurface_triangles(i->first, surfA);
	std::pair<Triangle, Triangle> trianglesB = get_subsurface_triangles(i->second, surfB);

	// calculate the intersection centers' uv coordinates
	int num_intersects = 0;
	ON_3dPoint average(0.0, 0.0, 0.0);
	ON_2dPoint avg_uvA(0.0, 0.0), avg_uvB(0.0, 0.0);
	for (int j = 0; j < 2; ++j) {
	    const Triangle &triA = j ? trianglesA.second : trianglesA.first;

	    for (int k = 0; k < 2; ++k) {
		const Triangle &triB = k ? trianglesB.second : trianglesB.first;

		ON_3dPoint intersection_center;
		if (triangle_intersection(triA, triB, intersection_center)) {
		    ON_3dPoint bcA = triA.BarycentricCoordinate(intersection_center);
		    ON_3dPoint bcB = triB.BarycentricCoordinate(intersection_center);
		    ON_2dPoint uvA = barycentric_to_uv(bcA, triA);
		    ON_2dPoint uvB = barycentric_to_uv(bcB, triB);
		    average += intersection_center;
		    avg_uvA += uvA;
		    avg_uvB += uvB;
		    ++num_intersects;
		}
	    }
	}

	// The centroid of these intersection centers is the
	// intersection point we want.
	if (num_intersects) {
	    average /= num_intersects;
	    avg_uvA /= num_intersects;
	    avg_uvB /= num_intersects;
	    if (box_intersect.IsPointIn(average)) {
		// use Newton iterations to get an accurate intersection
		if (newton_ssi(avg_uvA.x, avg_uvA.y, avg_uvB.x, avg_uvB.y, surfA, surfB, isect_tol)) {
		    average = surfA->PointAt(avg_uvA.x, avg_uvA.y);
		    tmp_curvept.Append(average);
		    tmp_curve_uvA.Append(avg_uvA);
		    tmp_curve_uvB.Append(avg_uvB);
		}
	    }
	}
    }

    add_points_to_closed_seams(surfA, surfB, tmp_curvept, tmp_curve_uvA, tmp_curve_uvB);

    // get just the unique, non-overlap intersection points
    for (int i = 0; i < tmp_curvept.Count(); i++) {
	bool unique_pt = true;
	for (int j = 0; j < curvept.Count(); j++) {
	    if (tmp_curvept[i].DistanceTo(curvept[j]) < isect_tol &&
		tmp_curve_uvA[i].DistanceTo(curve_uvA[j]) < isect_tolA &&
		tmp_curve_uvB[i].DistanceTo(curve_uvB[j]) < isect_tolB)
	    {
		unique_pt = false;
		break;
	    }
	}
	if (unique_pt) {
	    if (!is_uvA_completely_inside_overlap(overlap_events, tmp_curve_uvA[i])) {
		curvept.Append(tmp_curvept[i]);
		curve_uvA.Append(tmp_curve_uvA[i]);
		curve_uvB.Append(tmp_curve_uvB[i]);
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

	// should not return 0 as there might be overlap events
	return x.Count() - original_count;
    }

    // Fourth step: Fit the points in curvept into NURBS curves.
    // Here we use polyline approximation.
    // TODO: Find a better fitting algorithm unless this is good enough.
    // Time complexity: O(n^2*log(n)), really slow when n is large.
    // (n is the number of points generated above)

    // need to automatically generate a threshold
    // TODO: need more tests to find a better threshold
    double max_dist = std::min(rootA->m_surf->BoundingBox().Diagonal().Length(), rootB->m_surf->BoundingBox().Diagonal().Length()) * 0.1;

    double max_dist_uA = surfA->Domain(0).Length() * 0.05;
    double max_dist_vA = surfA->Domain(1).Length() * 0.05;
    double max_dist_uB = surfB->Domain(0).Length() * 0.05;
    double max_dist_vB = surfB->Domain(1).Length() * 0.05;
    if (DEBUG_BREP_INTERSECT) {
	bu_log("max_dist: %f\n", max_dist);
	bu_log("max_dist_uA: %f\n", max_dist_uA);
	bu_log("max_dist_vA: %f\n", max_dist_vA);
	bu_log("max_dist_uB: %f\n", max_dist_uB);
	bu_log("max_dist_vB: %f\n", max_dist_vB);
    }

    // identify the neighbors of each point
    std::vector<PointPair> ptpairs;
    for (int i = 0; i < curvept.Count(); i++) {
	for (int j = i + 1; j < curvept.Count(); j++) {
	    PointPair ppair;
	    ppair.distance3d = curvept[i].DistanceTo(curvept[j]);
	    ppair.dist_uA = fabs(curve_uvA[i].x - curve_uvA[j].x);
	    ppair.dist_vA = fabs(curve_uvA[i].y - curve_uvA[j].y);
	    ppair.dist_uB = fabs(curve_uvB[i].x - curve_uvB[j].x);
	    ppair.dist_vB = fabs(curve_uvB[i].y - curve_uvB[j].y);
	    ppair.tol = isect_tol;
	    if (ppair.dist_uA < max_dist_uA && ppair.dist_vA < max_dist_vA && ppair.dist_uB < max_dist_uB && ppair.dist_vB < max_dist_vB && ppair.distance3d < max_dist) {
		ppair.indexA = i;
		ppair.indexB = j;
		ptpairs.push_back(ppair);
	    }
	}
    }
    std::sort(ptpairs.begin(), ptpairs.end());

    std::vector<ON_SimpleArray<int>*> polylines(curvept.Count());

    // polyline_of_terminal[i] = j means curvept[i] is a startpoint/endpoint of polylines[j]
    int *polyline_of_terminal = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");

    // index of startpoints and endpoints of polylines[i]
    int *startpt = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");
    int *endpt = (int *)bu_malloc(curvept.Count() * sizeof(int), "int");

    // initialize each polyline with only one point
    for (int i = 0; i < curvept.Count(); i++) {
	ON_SimpleArray<int> *single = new ON_SimpleArray<int>();
	single->Append(i);
	polylines[i] = single;
	polyline_of_terminal[i] = i;
	startpt[i] = i;
	endpt[i] = i;
    }

    // merge polylines with distance less than max_dist
    for (size_t i = 0; i < ptpairs.size(); i++) {
	int terminal1 = ptpairs[i].indexA;
	int terminal2 = ptpairs[i].indexB;
	int poly1 = polyline_of_terminal[terminal1];
	int poly2 = polyline_of_terminal[terminal2];
	if (poly1 == -1 || poly2 == -1) {
	    continue;
	}
	// to merge poly2 into poly1, first start by making start/end points belong to poly1
	polyline_of_terminal[startpt[poly1]] = polyline_of_terminal[endpt[poly1]] = poly1;
	polyline_of_terminal[startpt[poly2]] = polyline_of_terminal[endpt[poly2]] = poly1;

	ON_SimpleArray<int> *line1 = polylines[poly1];
	ON_SimpleArray<int> *line2 = polylines[poly2];
	if (line1 != NULL && line2 != NULL && line1 != line2) {
	    // Join the two polylines so that terminal1 and terminal2
	    // are adjacent in the new unionline, reflecting the
	    // spatial adjacency of the corresponding curve points.
	    ON_SimpleArray<int> *unionline = new ON_SimpleArray<int>();
	    bool start1 = (*line1)[0] == terminal1;
	    bool start2 = (*line2)[0] == terminal2;
	    if (start1) {
		if (start2) {
		    // line1: [terminal1, ..., end1]
		    // line2: [terminal2, ..., end2]
		    // unionline: [end1, ..., terminal1, terminal2, ..., end2]
		    line1->Reverse();
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[poly1] = endpt[poly1];
		    endpt[poly1] = endpt[poly2];
		} else {
		    // line1: [terminal1, ..., end1]
		    // line2: [start2, ..., terminal2]
		    // unionline: [start2, ..., terminal2, terminal1, ..., end1]
		    unionline->Append(line2->Count(), line2->Array());
		    unionline->Append(line1->Count(), line1->Array());
		    startpt[poly1] = startpt[poly2];
		    endpt[poly1] = endpt[poly1];
		}
	    } else {
		if (start2) {
		    // line1: [start1, ..., terminal1]
		    // line2: [terminal2, ..., end2]
		    // unionline: [start1, ..., terminal1, terminal2, ..., end2]
		    unionline->Append(line1->Count(), line1->Array());
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[poly1] = startpt[poly1];
		    endpt[poly1] = endpt[poly2];
		} else {
		    // line1: [start1, ..., terminal1]
		    // line2: [start2, ..., terminal2]
		    // unionline: [start1, ..., terminal1, terminal2, ..., start2]
		    unionline->Append(line1->Count(), line1->Array());
		    line2->Reverse();
		    unionline->Append(line2->Count(), line2->Array());
		    startpt[poly1] = startpt[poly1];
		    endpt[poly1] = startpt[poly2];
		}
	    }
	    polylines[poly1] = unionline;
	    polylines[poly2] = NULL;

	    // If lineN has more than one point, then joining to
	    // terminalN has made it an interior point and it must be
	    // invalidated as a terminal.
	    if (line1->Count() >= 2) {
		polyline_of_terminal[terminal1] = -1;
	    }
	    if (line2->Count() >= 2) {
		polyline_of_terminal[terminal2] = -1;
	    }
	    delete line1;
	    delete line2;
	}
    }

    // In some cases, the intersection curves will intersect with each
    // other. But with our merging mechanism, a point can only belong
    // to one curve. If curves need to share an intersection point, we
    // need some "seaming" segments to handle it.
    size_t num_curves = polylines.size();
    for (size_t i = 0; i < num_curves; i++) {
	if (!polylines[i]) {
	    continue;
	}
	for (size_t j = i + 1; j < num_curves; j++) {
	    if (!polylines[j]) {
		continue;
	    }
	    ON_SimpleArray<int> &polyline1 = *polylines[i];
	    ON_SimpleArray<int> &polyline2 = *polylines[j];

	    // find the closest pair of adjacent points between the two polylines
	    int point_count1 = polyline1.Count();
	    int point_count2 = polyline2.Count();
	    PointPair pair;
	    pair.distance3d = DBL_MAX;
	    for (int k = 0; k < point_count1; k++) {
		for (int m = 0; m < point_count2; m++) {
		    PointPair newpair;
		    int start = polyline1[k], end = polyline2[m];
		    newpair.distance3d = curvept[start].DistanceTo(curvept[end]);
		    newpair.dist_uA = fabs(curve_uvA[start].x - curve_uvA[end].x);
		    newpair.dist_vA = fabs(curve_uvA[start].y - curve_uvA[end].y);
		    newpair.dist_uB = fabs(curve_uvB[start].x - curve_uvB[end].x);
		    newpair.dist_vB = fabs(curve_uvB[start].y - curve_uvB[end].y);
		    newpair.tol = isect_tol;
		    if (newpair.dist_uA < max_dist_uA && newpair.dist_vA < max_dist_vA && newpair.dist_uB < max_dist_uB && newpair.dist_vB < max_dist_vB) {
			if (newpair < pair) {
			    newpair.indexA = start;
			    newpair.indexB = end;
			    pair = newpair;
			}
		    }
		}
	    }
	    if (pair.distance3d < max_dist) {
		// Generate a seaming curve if there are currently no
		// intersections.
		// TODO: These curve-curve intersections are
		//       expensive. Is this really necessary?
		ON_3dPointArray uvA1, uvA2, uvB1, uvB2;
		for (int k = 0; k < polyline1.Count(); k++) {
		    uvA1.Append(curve_uvA[polyline1[k]]);
		    uvB1.Append(curve_uvB[polyline1[k]]);
		}
		for (int k = 0; k < polyline2.Count(); k++) {
		    uvA2.Append(curve_uvA[polyline2[k]]);
		    uvB2.Append(curve_uvB[polyline2[k]]);
		}
		ON_PolylineCurve curveA1(uvA1), curveA2(uvA2), curveB1(uvB1), curveB2(uvB2);
		ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
		if (ON_Intersect(&curveA1, &curveA2, x_event1, isect_tol) &&
		    ON_Intersect(&curveB1, &curveB2, x_event2, isect_tol)) {
		    continue;
		}

		// If the seaming curve is continuous to polylines[i]
		// or polylines[j], we can just merge the curve with
		// the polyline, otherwise we generate a new segment.
		if (!join_continuous_ppair_to_polyline(polylines[i], pair) &&
		    !join_continuous_ppair_to_polyline(polylines[j], pair))
		{
		    ON_SimpleArray<int> *seam = new ON_SimpleArray<int>;
		    seam->Append(pair.indexA);
		    seam->Append(pair.indexB);
		    polylines.push_back(seam);
		}
	    }
	}
    }

    // generate ON_Curves from the polylines
    ON_SimpleArray<ON_Curve *> intersect3d, intersect_uvA, intersect_uvB;
    ON_SimpleArray<int> single_pts;
    for (size_t i = 0; i < polylines.size(); i++) {
	if (polylines[i] == NULL) {
	    continue;
	}
	bool is_seam = (i >= num_curves);

	ON_SimpleArray<int> &polyline = *polylines[i];
	int startpoint = polyline[0];
	int endpoint = polyline[polyline.Count() - 1];

	if (polyline.Count() == 1) {
	    single_pts.Append(startpoint);
	    delete polylines[i];
	    continue;
	}

	// curve in 3D space
	ON_3dPointArray ptarray;
	for (int j = 0; j < polyline.Count(); j++) {
	    ptarray.Append(curvept[polyline[j]]);
	}
	// close curve if it forms a loop
	if (!is_seam && curvept[startpoint].DistanceTo(curvept[endpoint]) < max_dist) {
	    ptarray.Append(curvept[startpoint]);
	}
	ON_PolylineCurve *curve = new ON_PolylineCurve(ptarray);
	intersect3d.Append(curve);

	// curve in UV space (surfA)
	ptarray.Empty();
	for (int j = 0; j < polyline.Count(); j++) {
	    ON_2dPoint &pt2d = curve_uvA[polyline[j]];
	    ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	}
	// close curve if it forms a loop (happens rarely compared to 3D)
	if (!is_seam &&
	    fabs(curve_uvA[startpoint].x - curve_uvA[endpoint].x) < max_dist_uA &&
	    fabs(curve_uvA[startpoint].y - curve_uvA[endpoint].y) < max_dist_vA)
	{
	    ON_2dPoint &pt2d = curve_uvA[startpoint];
	    ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	}
	curve = new ON_PolylineCurve(ptarray);
	curve->ChangeDimension(2);
	intersect_uvA.Append(curve_fitting(curve, fitting_tolA));

	// curve in UV space (surfB)
	ptarray.Empty();
	for (int j = 0; j < polyline.Count(); j++) {
	    ON_2dPoint &pt2d = curve_uvB[polyline[j]];
	    ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	}
	// close curve if it forms a loop (happens rarely compared to 3D)
	if (!is_seam &&
	    fabs(curve_uvB[startpoint].x - curve_uvB[endpoint].x) < max_dist_uB &&
	    fabs(curve_uvB[startpoint].y - curve_uvB[endpoint].y) < max_dist_vB)
	{
	    ON_2dPoint &pt2d = curve_uvB[startpoint];
	    ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	}
	curve = new ON_PolylineCurve(ptarray);
	curve->ChangeDimension(2);
	intersect_uvB.Append(curve_fitting(curve, fitting_tolB));

	delete polylines[i];
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d curve segments and %d single points.\n", intersect3d.Count(), single_pts.Count());
    }
    bu_free(polyline_of_terminal, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");

    // generate ON_SSX_EVENTs
    if (intersect3d.Count()) {
	for (int i = 0; i < intersect3d.Count(); i++) {
	    ON_SSX_EVENT event;
	    event.m_curve3d = intersect3d[i];
	    event.m_curveA = intersect_uvA[i];
	    event.m_curveB = intersect_uvB[i];
	    // Normalize the curves, so that their domains are the same,
	    // which is required by ON_SSX_EVENT::IsValid().
	    event.m_curve3d->SetDomain(ON_Interval(0.0, 1.0));
	    event.m_curveA->SetDomain(ON_Interval(0.0, 1.0));
	    event.m_curveB->SetDomain(ON_Interval(0.0, 1.0));
	    // If the surfA and surfB normals of all points are
	    // parallel, the intersection is considered tangent.
	    bool is_tangent = true;
	    int count = std::min(event.m_curveA->SpanCount(), event.m_curveB->SpanCount());
	    for (int j = 0; j <= count; ++j) {
		ON_3dVector normalA, normalB;
		ON_3dPoint pointA = event.m_curveA->PointAt((double)j / count);
		ON_3dPoint pointB = event.m_curveB->PointAt((double)j / count);
		if (!(surfA->EvNormal(pointA.x, pointA.y, normalA) &&
		      surfB->EvNormal(pointB.x, pointB.y, normalB) &&
		      normalA.IsParallelTo(normalB)))
		{
		    is_tangent = false;
		    break;
		}
	    }
	    if (is_tangent) {
		// For ssx_tangent events, the 3d curve direction may
		// not agree with SurfaceNormalA x SurfaceNormalB
		// (See opennurbs/opennurbs_x.h).
		ON_3dVector direction = event.m_curve3d->TangentAt(0);
		ON_3dVector SurfaceNormalA = surfA->NormalAt(
			event.m_curveA->PointAtStart().x,
			event.m_curveA->PointAtStart().y);
		ON_3dVector SurfaceNormalB = surfB->NormalAt(
			event.m_curveB->PointAtStart().x,
			event.m_curveB->PointAtStart().y);
		if (ON_DotProduct(direction, ON_CrossProduct(SurfaceNormalB, SurfaceNormalA)) < 0) {
		    if (!(event.m_curve3d->Reverse() &&
			  event.m_curveA->Reverse() &&
			  event.m_curveB->Reverse()))
		    {
			bu_log("warning: reverse failed. The direction of %d might be wrong.\n",
			       x.Count() - original_count);
		    }
		}
		event.m_type = ON_SSX_EVENT::ssx_tangent;
	    } else {
		event.m_type = ON_SSX_EVENT::ssx_transverse;
	    }
	    // ssx_overlap is handled above
	    x.Append(event);
	    // set the curves to NULL in case they are deleted by ~ON_SSX_EVENT()
	    event.m_curve3d = event.m_curveA = event.m_curveB = NULL;
	}
    }

    for (int i = 0; i < single_pts.Count(); i++) {
	bool unique_pt = true;
	for (int j = 0; j < intersect3d.Count(); ++j) {
	    ON_ClassArray<ON_PX_EVENT> px_event;
	    if (ON_Intersect(curvept[single_pts[i]], *intersect3d[j], px_event)) {
		unique_pt = false;
		break;
	    }
	}
	if (unique_pt) {
	    ON_SSX_EVENT event;
	    event.m_point3d = curvept[single_pts[i]];
	    event.m_pointA = curve_uvA[single_pts[i]];
	    event.m_pointB = curve_uvB[single_pts[i]];
	    // If the surfA and surfB normals are parallel, the
	    // intersection is considered tangent.
	    ON_3dVector normalA, normalB;
	    if (surfA->EvNormal(event.m_pointA.x, event.m_pointA.y, normalA) &&
		surfB->EvNormal(event.m_pointB.x, event.m_pointB.y, normalB) &&
		normalA.IsParallelTo(normalB))
	    {
		event.m_type = ON_SSX_EVENT::ssx_tangent_point;
	    } else {
		event.m_type = ON_SSX_EVENT::ssx_transverse_point;
	    }
	    x.Append(event);
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
