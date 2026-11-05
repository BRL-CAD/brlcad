/*                  I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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
 * Additional documentation can be found in the "NURBS Boolean Evaluation
 * Development Guide" docbook article (bool_eval_development.html).
 */

#include "common.h"

#include <atomic>
#include <assert.h>
#include <vector>
#include <algorithm> // for std::sort
#include <cmath>
#include <limits>
#include <map>

#include "vmath.h"

#include "bio.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "brep/defines.h"
#include "brep/intersect.h"
#include "brep/pullback.h"
#include "brep/util.h"
#include "debug_plot.h"
#include "brep_except.h"
#include "brep_defines.h"
#include "affine_surface.h"

//extern DebugPlot *dplot;

// Whether to output the debug messages about b-rep intersections.
static int DEBUG_BREP_INTERSECT = 0;

class brlcad::IntersectionWorkBudget {
public:
    explicit IntersectionWorkBudget(size_t maximum_candidate_pairs)
	: m_maximum_candidate_pairs(maximum_candidate_pairs),
	  m_candidate_pair_count(0),
	  m_exhausted(false)
    {
    }

    bool ConsumeCandidatePair()
    {
	if (m_exhausted.load()) {
	    return false;
	}

	size_t candidate_pair_count = m_candidate_pair_count.load();
	while (true) {
	    if (candidate_pair_count >= m_maximum_candidate_pairs) {
		m_exhausted.store(true);
		return false;
	    }
	    if (m_candidate_pair_count.compare_exchange_weak(candidate_pair_count,
		    candidate_pair_count + 1)) {
		return true;
	    }
	    if (m_exhausted.load()) {
		return false;
	    }
	}
    }

    void MarkExhausted()
    {
	m_exhausted.store(true);
    }

    bool IsExceeded() const
    {
	return m_exhausted.load();
    }

private:
    const size_t m_maximum_candidate_pairs;
    std::atomic<size_t> m_candidate_pair_count;
    std::atomic<bool> m_exhausted;
};


static thread_local brlcad::IntersectionWorkBudgetHandle intersection_work_budget;
static thread_local unsigned int intersection_call_depth = 0;
static thread_local bool intersection_budget_exhausted = false;


brlcad::IntersectionWorkBudgetHandle
brlcad::CreateIntersectionWorkBudget(size_t maximum_candidate_pairs)
{
    return maximum_candidate_pairs ? IntersectionWorkBudgetHandle(
	new IntersectionWorkBudget(maximum_candidate_pairs)) :
	IntersectionWorkBudgetHandle();
}


brlcad::IntersectionWorkBudgetHandle
brlcad::CurrentIntersectionWorkBudget()
{
    return intersection_work_budget;
}


void
brlcad::SetIntersectionWorkBudget(
    const IntersectionWorkBudgetHandle &work_budget)
{
    intersection_work_budget = work_budget;
    intersection_budget_exhausted = intersection_work_budget &&
	intersection_work_budget->IsExceeded();
}


void
brlcad::ClearIntersectionWorkBudget()
{
    intersection_work_budget.reset();
    intersection_budget_exhausted = false;
}


bool
brlcad::IntersectionWorkBudgetExceeded()
{
    return intersection_work_budget && intersection_work_budget->IsExceeded();
}

class IntersectionBudgetScope {
public:
    IntersectionBudgetScope()
	: m_outermost(intersection_call_depth++ == 0)
    {
	if (m_outermost) {
	    intersection_budget_exhausted = intersection_work_budget &&
		intersection_work_budget->IsExceeded();
	}
    }

    ~IntersectionBudgetScope()
    {
	--intersection_call_depth;
    }

    bool IsBudgetExceeded() const
    {
	return intersection_budget_exhausted ||
	    (intersection_work_budget && intersection_work_budget->IsExceeded());
    }

    bool ConsumeCandidatePair()
    {
	if (IsBudgetExceeded()) {
	    intersection_budget_exhausted = true;
	    return false;
	}
	if (intersection_work_budget &&
	    !intersection_work_budget->ConsumeCandidatePair()) {
	    intersection_budget_exhausted = true;
	    return false;
	}
	return true;
    }

    int BudgetExceededResult()
    {
	intersection_budget_exhausted = true;
	if (intersection_work_budget) {
	    intersection_work_budget->MarkExhausted();
	}
	return m_outermost ? BREP_INTERSECTION_BUDGET_EXCEEDED : 0;
    }

private:
    bool m_outermost;
};

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
    ON_Interval dom = in->Domain();

    if (b < a) {
	std::swap(a, b);
    }

    ON_Curve *min_to_a = NULL, *a_to_max = NULL;
    ON_Curve *a_to_b = NULL, *b_to_max = NULL;

    if (a < dom.m_t[0] || b > dom.m_t[1]) {
	throw InvalidInterval("sub_curve() interval outside curve domain\n");
    }

    // Note that ON_SQRT_EPSILON is the tolerance used inside
    // ON_PolylineCurve::Split() to determine if the split parameter
    // should snap to an end of the domain.
    if (ON_NearZero(a - dom.m_t[0], ON_SQRT_EPSILON)) {
	if (ON_NearZero(dom.m_t[1] - b, ON_SQRT_EPSILON)) {
	    // a == dom.m_t[0] && b == dom.m_t[1]
	    a_to_b = in->Duplicate();
	} else {
	    // a == dom.m_t[0] && b < dom.m_t[1]
	    in->Split(b, a_to_b, b_to_max);
	    delete b_to_max;
	}
	if (!a_to_b) {
	    // dom.m_t[0] == a == b
	    throw InvalidInterval("sub_curve() degenerate interval\n");
	}
	return a_to_b;
    }

    if (ON_NearZero(dom.m_t[1] - b, ON_SQRT_EPSILON)) {
	// a > dom.m_t[0] && b == dom.m_t[1]
	in->Split(a, min_to_a, a_to_b);
	delete min_to_a;

	if (!a_to_b) {
	    // a == b == dom.m_t[1]
	    throw InvalidInterval("sub_curve() degenerate interval\n");
	}
	return a_to_b;
    }

    // a > dom.m_t[0] && b < dom.m_t[1]
    in->Split(a, min_to_a, a_to_max);
    delete min_to_a;

    if (!a_to_max) {
	// a == b == dom.m_t[1]
	throw InvalidInterval("sub_curve() interval is degenerate\n");
    }
    a_to_max->Split(b, a_to_b, b_to_max);
    delete a_to_max;
    delete b_to_max;

    if (!a_to_b) {
	// a == b
	throw InvalidInterval("sub_curve() interval is degenerate\n");
    }
    return a_to_b;
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


static const ON_Interval
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


static bool
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
	try {
	    root.m_curve = sub_curve(curve, domain->Min(), domain->Max());
	} catch (InvalidInterval &) {
	    root.m_curve = NULL;
	}
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


static Subcurve *
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


static bool
build_surface_root(const ON_Surface *surf, const ON_Interval *u_domain, const ON_Interval *v_domain, Subsurface &root)
{
    // Build the surface tree root within a given domain

    if (surf == NULL) {
	return false;
    }

    ON_Surface *u_splitted_surf = NULL;
    const ON_Surface *root_surface = surf;

    // First, do u split
    if (u_domain == NULL || *u_domain == surf->Domain(0)) {
	root.m_u = surf->Domain(0);
    } else {
	// Call sub_surface() to get the sub-surface inside the input U domain.
	u_splitted_surf = sub_surface(surf, 0, u_domain->Min(), u_domain->Max());
	root.m_u = *u_domain;
	root_surface = u_splitted_surf;
    }

    if (root_surface == NULL) {
	return false;
    }

    // Then v split
    if (v_domain == NULL || *v_domain == surf->Domain(1)) {
	root.m_surf = root_surface->Duplicate();
	root.m_v = surf->Domain(1);
    } else {
	// Call sub_surface() to get the sub-surface inside the input V domain.
	root.m_surf = sub_surface(root_surface, 1, v_domain->Min(), v_domain->Max());
	root.m_v = *v_domain;
    }
    delete u_splitted_surf;

    if (root.m_surf) {
	ON_BoundingBox surf_bbox;
	int ret = root.m_surf->GetBoundingBox(surf_bbox.m_min,
					      surf_bbox.m_max);
	if (!ret) {
	    vect_t corners_min = VINIT_ZERO;
	    vect_t corners_max = VINIT_ZERO;
	    for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
		    ON_3dPoint corner = root.m_surf->PointAt(root.m_u.m_t[i],
							 root.m_v.m_t[j]);
		    VMINMAX(corners_min, corners_max, corner);
		}
	    }
	    surf_bbox.m_min = ON_3dPoint(corners_min);
	    surf_bbox.m_max = ON_3dPoint(corners_max);
	}
	root.SetBBox(surf_bbox);
	root.m_isplanar = root.m_surf->IsPlanar();
	return true;
    } else {
	return false;
    }
}


static Subsurface *
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


static ON_Curve *
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
static bool
newton_pci(double &t, const ON_3dPoint &pointA, const ON_Curve &curveB, double tol)
{
    ON_3dPoint closest_point = curveB.PointAt(t);
    double dist;

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
    return dist <= tol && !std::isnan(t);
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
	double line_t;
	line.ClosestPointTo(pointA, &line_t);

	// make sure line_t belongs to [0, 1]
	CLAMP(line_t, 0.0, 1.0);
	double closest_point_t = candidates[i]->m_t.ParameterAt(line_t);

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
static bool
newton_psi(double &u, double &v, const ON_3dPoint &pointA, const ON_Surface &surfB, double tol)
{
    ON_3dPoint closest_point = surfB.PointAt(u, v);
    double dist;

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
    return dist <= tol && !std::isnan(u) && !std::isnan(v);
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

static void
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

    double approx_len_3d = root->m_curve->BoundingBox().Diagonal().Length();
    if (!ON_NearZero(approx_len_3d)) {
	const ON_Interval dom = curve_domain ? *curve_domain : curve->Domain();
	double approx_len_2d = dom.Length();

	tol_2d = tol_3d * (approx_len_2d / approx_len_3d);
    }
    return tol_2d;
}


static double
minimum_distance(const ON_Line &lineA, const ON_Line &lineB, double isect_tol)
{
    double min_dist = lineA.MinimumDistanceTo(lineB);

    if (min_dist >= isect_tol) {
	// min_dist may not be accurate if endpoints are collinear
	double d1 = lineA.from.DistanceTo(lineB.from);
	double d2 = lineA.from.DistanceTo(lineB.to);
	double d3 = lineA.to.DistanceTo(lineB.from);
	double d4 = lineA.to.DistanceTo(lineB.to);

	min_dist = std::min(min_dist, std::min(d1, std::min(d2,
							    std::min(d3, d4))));
    }
    return min_dist;
}


static bool
bounding_box_is_within_chord_tolerance(const ON_BoundingBox &bbox,
				       const ON_Line &chord,
				       double tolerance)
{
    if (!bbox.IsValid() || tolerance < 0.0) {
	return false;
    }

    const ON_3dVector direction = chord.Direction();
    const double direction_length_squared = direction.LengthSquared();
    if (direction_length_squared <= ON_ZERO_TOLERANCE * ON_ZERO_TOLERANCE) {
	return false;
    }

    const double parameter_tolerance = tolerance /
	std::sqrt(direction_length_squared);
    for (int x_index = 0; x_index < 2; ++x_index) {
	for (int y_index = 0; y_index < 2; ++y_index) {
	    for (int z_index = 0; z_index < 2; ++z_index) {
		const ON_3dPoint corner(x_index ? bbox.m_max.x : bbox.m_min.x,
		    y_index ? bbox.m_max.y : bbox.m_min.y,
		    z_index ? bbox.m_max.z : bbox.m_min.z);
		const ON_3dVector offset = corner - chord.from;
		const double parameter = ON_DotProduct(offset, direction) /
		    direction_length_squared;
		if (parameter < -parameter_tolerance ||
		    parameter > 1.0 + parameter_tolerance) {
		    return false;
		}
		const ON_3dPoint closest_point = chord.PointAt(
		    std::max(0.0, std::min(1.0, parameter)));
		if (corner.DistanceTo(closest_point) > tolerance) {
		    return false;
		}
	    }
	}
    }

    return true;
}


static bool
curve_segment_is_within_chord_tolerance(const ON_Curve *curve,
					double first_parameter,
					double second_parameter,
					const ON_Line &chord,
					double tolerance)
{
    ON_Curve *segment = NULL;
    try {
	segment = sub_curve(curve, first_parameter, second_parameter);
    } catch (InvalidInterval &) {
	return false;
    }

    if (!segment) {
	return false;
    }

    ON_BoundingBox bbox;
    if (!segment->GetTightBoundingBox(bbox)) {
	bbox = segment->BoundingBox();
    }
    if (!bbox.IsValid()) {
	delete segment;
	return false;
    }
    delete segment;
    return bounding_box_is_within_chord_tolerance(bbox, chord, tolerance);
}


static bool
overlap_candidate_fits_common_chord(const ON_Curve *curveA,
				    double t_a1,
				    double t_a2,
				    const ON_Curve *curveB,
				    double t_b1,
				    double t_b2,
				    const ON_3dPoint &pointA1,
				    const ON_3dPoint &pointA2,
				    double overlap_tol)
{
    // A common chord tube checks complete segment bounds rather than
    // promoting a finite set of point hits to a continuous overlap.
    const ON_Line chord(pointA1, pointA2);
    const double chord_tolerance = overlap_tol * 0.5;
    return curve_segment_is_within_chord_tolerance(curveA, t_a1, t_a2,
	chord, chord_tolerance) &&
	curve_segment_is_within_chord_tolerance(curveB, t_b1, t_b2,
	chord, chord_tolerance);
}


static bool
nurbs_forms_match(const ON_NurbsCurve &curveA, const ON_NurbsCurve &curveB)
{
    // Equal homogeneous control vertices and normalized knots prove the same
    // curve under an affine parameter-domain transformation.
    if (curveA.Degree() != curveB.Degree() ||
	curveA.CVCount() != curveB.CVCount() ||
	curveA.KnotCount() != curveB.KnotCount()) {
	return false;
    }

    const ON_Interval domainA = curveA.Domain();
    const ON_Interval domainB = curveB.Domain();
    if (!domainA.IsIncreasing() || !domainB.IsIncreasing()) {
	return false;
    }

    for (int cv_index = 0; cv_index < curveA.CVCount(); ++cv_index) {
	ON_4dPoint cvA, cvB;
	if (!curveA.GetCV(cv_index, cvA) || !curveB.GetCV(cv_index, cvB) ||
	    cvA != cvB) {
	    return false;
	}
    }

    for (int knot_index = 0; knot_index < curveA.KnotCount();
	 ++knot_index) {
	const double knotA = (curveA.m_knot[knot_index] - domainA.Min()) /
	    domainA.Length();
	const double knotB = (curveB.m_knot[knot_index] - domainB.Min()) /
	    domainB.Length();
	if (!ON_NearZero(knotA - knotB)) {
	    return false;
	}
    }

    return true;
}


static bool
curves_have_matching_nurbs_forms(const ON_Curve *curveA,
				 const ON_Curve *curveB,
				 bool reverse_second)
{
    if (curveA == NULL || curveB == NULL) {
	return false;
    }

    ON_Curve *oriented_curveB = curveB->Duplicate();
    if (oriented_curveB == NULL) {
	return false;
    }
    if (reverse_second && !oriented_curveB->Reverse()) {
	delete oriented_curveB;
	return false;
    }

    ON_NurbsCurve nurbsA;
    ON_NurbsCurve nurbsB;
    const bool matches = curveA->GetNurbForm(nurbsA) > 0 &&
	oriented_curveB->GetNurbForm(nurbsB) > 0 &&
	nurbs_forms_match(nurbsA, nurbsB);
    delete oriented_curveB;
    return matches;
}


static const int CURVE_EQUIVALENCE_MAXIMUM_SUBDIVISION_DEPTH = MAX_CCI_DEPTH;
static const double CURVE_EQUIVALENCE_CERTIFICATE_TOLERANCE_FRACTION = 0.25;
static const double CURVE_EQUIVALENCE_PARAMETER_TOLERANCE_SCALE = 8.0;


class CurveNurbsForm {
public:
    explicit CurveNurbsForm(const ON_Curve *curve)
	: m_curve(curve),
	  m_nurbs_form(0)
    {
    }

    bool Initialize()
    {
	if (m_curve == NULL) {
	    return false;
	}

	m_nurbs_form = m_curve->GetNurbForm(m_nurbs);
	return (m_nurbs_form == 1 || m_nurbs_form == 2) &&
	    m_nurbs.Domain().IsIncreasing();
    }

    const ON_Curve *Curve() const
    {
	return m_curve;
    }

    bool ClosestCurveParameter(const ON_3dPoint &point,
			       double maximum_distance,
			       const ON_Interval &curve_domain,
			       double &curve_parameter) const
    {
	if (m_curve == NULL || !point.IsValid() || !(maximum_distance > 0.0) ||
	    !curve_domain.IsIncreasing())
	{
	    return false;
	}

	double nurbs_start = 0.0;
	double nurbs_end = 0.0;
	if (!NurbsParameter(curve_domain.Min(), nurbs_start) ||
	    !NurbsParameter(curve_domain.Max(), nurbs_end))
	{
	    return false;
	}

	ON_Interval nurbs_domain(nurbs_start, nurbs_end);
	nurbs_domain.MakeIncreasing();
	if (!nurbs_domain.IsIncreasing() ||
	    !m_nurbs.Domain().Includes(nurbs_domain.Min()) ||
	    !m_nurbs.Domain().Includes(nurbs_domain.Max()))
	{
	    return false;
	}

	double nurbs_parameter = nurbs_domain.Mid();
	if (!ON_NurbsCurve_GetClosestPoint(&nurbs_parameter, &m_nurbs, point,
		maximum_distance, &nurbs_domain) ||
	    !CurveParameter(nurbs_parameter, curve_parameter) ||
	    !curve_domain.Includes(curve_parameter))
	{
	    return false;
	}

	const ON_3dPoint closest_point = m_curve->PointAt(curve_parameter);
	return closest_point.IsValid() &&
	    closest_point.DistanceTo(point) <= maximum_distance;
    }

private:
    bool NurbsParameter(double curve_parameter, double &nurbs_parameter) const
    {
	nurbs_parameter = curve_parameter;
	return m_nurbs_form != 2 ||
	    m_curve->GetNurbFormParameterFromCurveParameter(curve_parameter,
		&nurbs_parameter);
    }

    bool CurveParameter(double nurbs_parameter, double &curve_parameter) const
    {
	curve_parameter = nurbs_parameter;
	return m_nurbs_form != 2 ||
	    m_curve->GetCurveParameterFromNurbFormParameter(nurbs_parameter,
		&curve_parameter);
    }

    const ON_Curve *m_curve;
    ON_NurbsCurve m_nurbs;
    int m_nurbs_form;
};


static double
curve_equivalence_parameter_tolerance(const ON_Interval &domain)
{
    return CURVE_EQUIVALENCE_PARAMETER_TOLERANCE_SCALE *
	ON_ZERO_TOLERANCE * std::max(1.0, fabs(domain.Length()));
}


static bool
curve_equivalence_interval_has_interior(const ON_Interval &interval,
					double parameter,
					double parameter_tolerance)
{
    return interval.IsIncreasing() &&
	parameter > interval.Min() + parameter_tolerance &&
	parameter < interval.Max() - parameter_tolerance;
}


static bool
curve_segment_follows_chord_monotonically(const ON_Curve *curve,
					  double first_parameter,
					  double second_parameter,
					  const ON_Line &chord,
					  double tolerance)
{
    ON_Curve *segment = NULL;
    try {
	segment = sub_curve(curve, first_parameter, second_parameter);
    } catch (InvalidInterval &) {
	return false;
    }

    if (segment == NULL) {
	return false;
    }

    ON_NurbsCurve nurbs;
    const bool has_nurbs_form = segment->GetNurbForm(nurbs) > 0;
    delete segment;
    if (!has_nurbs_form || nurbs.IsRational()) {
	return false;
    }

    const ON_3dVector chord_direction = chord.Direction();
    const double chord_length = chord_direction.Length();
    if (chord_length <= tolerance) {
	return false;
    }

    double previous_projection = 0.0;
    for (int control_vertex_index = 0;
	 control_vertex_index < nurbs.CVCount(); ++control_vertex_index)
    {
	ON_4dPoint control_vertex;
	if (!nurbs.GetCV(control_vertex_index, control_vertex) ||
	    !std::isfinite(control_vertex.w) ||
	    fabs(control_vertex.w) <= ON_ZERO_TOLERANCE)
	{
	    return false;
	}

	const ON_3dPoint control_point(control_vertex);
	if (!control_point.IsValid()) {
	    return false;
	}

	const double projection = ON_DotProduct(control_point - chord.from,
		chord_direction) / chord_length;
	if (control_vertex_index > 0 &&
	    projection + tolerance < previous_projection)
	{
	    return false;
	}
	previous_projection = projection;
    }

    return true;
}


static bool
curve_interval_pair_has_equivalence_certificate(
	const ON_Curve *curveA,
	const ON_Interval &intervalA,
	const ON_Curve *curveB,
	const ON_Interval &intervalB,
	double certificate_tolerance)
{
    const ON_3dPoint startA = curveA->PointAt(intervalA.Min());
    const ON_3dPoint endA = curveA->PointAt(intervalA.Max());
    const ON_3dPoint startB = curveB->PointAt(intervalB.Min());
    const ON_3dPoint endB = curveB->PointAt(intervalB.Max());
    if (!startA.IsValid() || !endA.IsValid() || !startB.IsValid() ||
	!endB.IsValid() ||
	startA.DistanceTo(startB) > certificate_tolerance ||
	endA.DistanceTo(endB) > certificate_tolerance)
    {
	return false;
    }

    const ON_Line chord(startA, endA);
    return curve_segment_is_within_chord_tolerance(curveA,
	intervalA.Min(), intervalA.Max(), chord, certificate_tolerance) &&
	curve_segment_is_within_chord_tolerance(curveB,
	intervalB.Min(), intervalB.Max(), chord, certificate_tolerance) &&
	curve_segment_follows_chord_monotonically(curveA,
	intervalA.Min(), intervalA.Max(), chord, certificate_tolerance) &&
	curve_segment_follows_chord_monotonically(curveB,
	intervalB.Min(), intervalB.Max(), chord, certificate_tolerance);
}


static bool
certify_curve_equivalence(const CurveNurbsForm &curveA_form,
			  const ON_Interval &intervalA,
			  const CurveNurbsForm &curveB_form,
			  const ON_Interval &intervalB,
			  double certificate_tolerance,
			  int depth)
{
    const ON_Curve *curveA = curveA_form.Curve();
    const ON_Curve *curveB = curveB_form.Curve();
    if (curveA == NULL || curveB == NULL || !intervalA.IsIncreasing() ||
	!intervalB.IsIncreasing())
    {
	return false;
    }

    if (curve_interval_pair_has_equivalence_certificate(curveA, intervalA,
	    curveB, intervalB, certificate_tolerance))
    {
	return true;
    }
    if (depth >= CURVE_EQUIVALENCE_MAXIMUM_SUBDIVISION_DEPTH) {
	return false;
    }

    const double midpointA = intervalA.Mid();
    const ON_3dPoint pointA = curveA->PointAt(midpointA);
    if (!pointA.IsValid()) {
	return false;
    }

    double midpointB = 0.0;
    if (!curveB_form.ClosestCurveParameter(pointA, certificate_tolerance,
	intervalB, midpointB) ||
	!curve_equivalence_interval_has_interior(intervalB, midpointB,
	    curve_equivalence_parameter_tolerance(intervalB)))
    {
	return false;
    }

    const double unpaired_midpointB = intervalB.Mid();
    const ON_3dPoint unpaired_pointB = curveB->PointAt(unpaired_midpointB);
    double paired_midpointA = 0.0;
    if (!unpaired_pointB.IsValid() ||
	!curveA_form.ClosestCurveParameter(unpaired_pointB,
	    certificate_tolerance, intervalA, paired_midpointA) ||
	!intervalA.Includes(paired_midpointA))
    {
	return false;
    }

    return certify_curve_equivalence(curveA_form,
	ON_Interval(intervalA.Min(), midpointA), curveB_form,
	ON_Interval(intervalB.Min(), midpointB), certificate_tolerance,
	depth + 1) &&
	certify_curve_equivalence(curveA_form,
	ON_Interval(midpointA, intervalA.Max()), curveB_form,
	ON_Interval(midpointB, intervalB.Max()), certificate_tolerance,
	depth + 1);
}


bool
ON_CurvesAreEquivalent(const ON_Curve *curveA,
		       const ON_Curve *curveB,
		       double tolerance,
		       bool reverse_second)
{
    if (curveA == NULL || curveB == NULL || !(tolerance > 0.0) ||
	!std::isfinite(tolerance))
    {
	return false;
    }

    if (curves_have_matching_nurbs_forms(curveA, curveB, reverse_second)) {
	return true;
    }

    ON_Curve *oriented_curveB = curveB->Duplicate();
    if (oriented_curveB == NULL) {
	return false;
    }
    if (reverse_second && !oriented_curveB->Reverse()) {
	delete oriented_curveB;
	return false;
    }

    const ON_Interval domainA = curveA->Domain();
    const ON_Interval domainB = oriented_curveB->Domain();
    const double certificate_tolerance = tolerance *
	CURVE_EQUIVALENCE_CERTIFICATE_TOLERANCE_FRACTION;
    const ON_3dPoint startA = curveA->PointAt(domainA.Min());
    const ON_3dPoint endA = curveA->PointAt(domainA.Max());
    const ON_3dPoint startB = oriented_curveB->PointAt(domainB.Min());
    const ON_3dPoint endB = oriented_curveB->PointAt(domainB.Max());
    if (!domainA.IsIncreasing() || !domainB.IsIncreasing() ||
	!(certificate_tolerance > 0.0) || !startA.IsValid() || !endA.IsValid() ||
	!startB.IsValid() || !endB.IsValid() ||
	startA.DistanceTo(startB) > certificate_tolerance ||
	endA.DistanceTo(endB) > certificate_tolerance)
    {
	delete oriented_curveB;
	return false;
    }

    CurveNurbsForm curveA_form(curveA);
    CurveNurbsForm curveB_form(oriented_curveB);
    const bool matches = curveA_form.Initialize() && curveB_form.Initialize() &&
	certify_curve_equivalence(curveA_form, domainA, curveB_form, domainB,
	    certificate_tolerance, 0);
    delete oriented_curveB;
    return matches;
}


static bool
curve_segments_have_matching_nurbs_forms(const ON_Curve *curveA,
					  double t_a1,
					  double t_a2,
					  const ON_Curve *curveB,
					  double t_b1,
					  double t_b2)
{
    ON_Curve *segmentA = NULL;
    ON_Curve *segmentB = NULL;
    try {
	segmentA = sub_curve(curveA, t_a1, t_a2);
	segmentB = sub_curve(curveB, t_b1, t_b2);
    } catch (InvalidInterval &) {
	delete segmentA;
	delete segmentB;
	return false;
    }

    if (!segmentA || !segmentB) {
	delete segmentA;
	delete segmentB;
	return false;
    }

	const bool reverse_second = (t_a2 - t_a1) * (t_b2 - t_b1) < 0.0;
	const bool matches = curves_have_matching_nurbs_forms(segmentA, segmentB,
	reverse_second);
	delete segmentA;
	delete segmentB;
	return matches;
}


static int
curve_intervals_equivalence_orientation(
    const ON_Curve *curveA,
    const ON_Interval &domainA,
    const ON_Curve *curveB,
    const ON_Interval &domainB,
    double tolerance)
{
    std::unique_ptr<ON_Curve> segmentA;
    std::unique_ptr<ON_Curve> segmentB;
    try {
	segmentA.reset(sub_curve(curveA, domainA.Min(), domainA.Max()));
	segmentB.reset(sub_curve(curveB, domainB.Min(), domainB.Max()));
    } catch (InvalidInterval &) {
	return 0;
    }
    if (!segmentA || !segmentB) {
	return 0;
    }

    if (ON_CurvesAreEquivalent(segmentA.get(), segmentB.get(), tolerance,
	false))
    {
	return 1;
    }
    return ON_CurvesAreEquivalent(segmentA.get(), segmentB.get(), tolerance,
	true) ? -1 : 0;
}


// For two lines known to be parallel and within distance tolerance,
// find the point/overlap intersection parameters.
//
// Returns 1 for point intersection, 2 for overlap intersection.
// For overlaps, parameters are set to be increasing from a1 to a2 and
// b1 to b2.
static int
intersect_overlapping_lines(
    double &t_a1,
    double &t_a2,
    double &t_b1,
    double &t_b2,
    const ON_Line &lineA,
    const ON_Line &lineB,
    double tol_2d)
{
    // curves lie on the same line, may be single
    // point intersection or overlap
    lineA.ClosestPointTo(lineB.from, &t_a1);
    lineA.ClosestPointTo(lineB.to, &t_a2);
    lineB.ClosestPointTo(lineA.from, &t_b1);
    lineB.ClosestPointTo(lineA.to, &t_b2);

    // Since ClosestPointTo treats the lines as
    // infinite, t values aren't necessarily in [0.0, 1.0].
    CLAMP(t_a1, 0.0, 1.0);
    CLAMP(t_a2, 0.0, 1.0);
    CLAMP(t_b1, 0.0, 1.0);
    CLAMP(t_b2, 0.0, 1.0);

    // t values may not be increasing if lines have
    // opposite directions.
    if (t_a1 > t_a2) {
	std::swap(t_a1, t_a2);
    }
    if (t_b1 > t_b2) {
	std::swap(t_b1, t_b2);
    }

    if (ON_NearZero(t_a2 - t_a1, tol_2d)) {
	// point intersection
	return 1;
    }
    return 2;
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
    IntersectionBudgetScope budget_scope;
    if (budget_scope.IsBudgetExceeded()) {
	return budget_scope.BudgetExceededResult();
    }

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

    const ON_Interval equivalence_domainA = curveA_domain ?
	*curveA_domain : curveA->Domain();
    const ON_Interval equivalence_domainB = curveB_domain ?
	*curveB_domain : curveB->Domain();
    // Equivalence certification replaces subdivision of the initial curve
    // pair, but it is still intersection work and must participate in an
    // operation-wide budget.  Reuse this charge if subdivision is needed.
    if (!budget_scope.ConsumeCandidatePair()) {
	return budget_scope.BudgetExceededResult();
    }
    const int equivalence_orientation =
	curve_intervals_equivalence_orientation(curveA, equivalence_domainA,
	    curveB, equivalence_domainB, overlap_tol);
    if (equivalence_orientation != 0) {
	XEventProxy event(ON_X_EVENT::ccx_overlap);
	event.SetAPoints(curveA->PointAt(equivalence_domainA.Min()),
	    curveA->PointAt(equivalence_domainA.Max()));
	if (equivalence_orientation > 0) {
	    event.SetBPoints(curveB->PointAt(equivalence_domainB.Min()),
		curveB->PointAt(equivalence_domainB.Max()));
	} else {
	    event.SetBPoints(curveB->PointAt(equivalence_domainB.Max()),
		curveB->PointAt(equivalence_domainB.Min()));
	}
	event.SetAOverlapRange(equivalence_domainA);
	event.SetBOverlapRange(equivalence_domainB);
	x.Append(event.Event());
	return 1;
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
    size_t candidate_pair_count = candidates.size();

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
			if (candidate_pair_count >= MAX_CCI_CANDIDATE_PAIRS ||
			    !budget_scope.ConsumeCandidatePair()) {
			    x.SetCount(original_count);
			    if (treeA == NULL) {
				delete rootA;
			    }
			    if (treeB == NULL) {
				delete rootB;
			    }
			    return budget_scope.BudgetExceededResult();
			}
			++candidate_pair_count;
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
		double min_dist = minimum_distance(lineA, lineB, isect_tol);

		if (min_dist < isect_tol) {
		    double t_a1, t_a2, t_b1, t_b2;
		    int ret = intersect_overlapping_lines(
			t_a1, t_a2, t_b1, t_b2, lineA, lineB, t1_tol);

		    if (ret == 1) {
			// point intersection
			XEventProxy event(ON_X_EVENT::ccx_point);
			event.SetAPoint(lineA.PointAt(t_a1));
			event.SetBPoint(lineB.PointAt(t_b1));
			event.SetACurveParameter(i->first->m_t.ParameterAt(t_a1));
			event.SetBCurveParameter(i->second->m_t.ParameterAt(t_b1));
			tmp_x.Append(event.Event());
		    } else if (ret == 2) {
			// overlap intersection
			XEventProxy event(ON_X_EVENT::ccx_overlap);
			event.SetAPoints(lineA.PointAt(t_a1), lineA.PointAt(t_a2));
			event.SetBPoints(lineB.PointAt(t_b1), lineB.PointAt(t_b2));
			event.SetAOverlapRange(i->first->m_t.ParameterAt(t_a1),
					       i->first->m_t.ParameterAt(t_a2));
			event.SetBOverlapRange(i->second->m_t.ParameterAt(t_b1),
					       i->second->m_t.ParameterAt(t_b2));
			tmp_x.Append(event.Event());
		    }
		}
	    } else {
		// not parallel, check intersection point
		double t_lineA, t_lineB;
		double t_a, t_b;
		if (ON_IntersectLineLine(lineA, lineB, &t_lineA, &t_lineB, isect_tol, true)) {
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
	    if (std::isnan(t_a1) || std::isnan(t_b1)) {
		// the first iteration result is not sufficient
		std::swap(t_a1, t_a2);
		std::swap(t_b1, t_b2);
	    }
	    if (std::isnan(t_a1) || std::isnan(t_b1)) {
		continue;
	    }

	    ON_3dPoint pointA1 = curveA->PointAt(t_a1);
	    ON_3dPoint pointB1 = curveB->PointAt(t_b1);
	    ON_3dPoint pointA2 = curveA->PointAt(t_a2);
	    ON_3dPoint pointB2 = curveB->PointAt(t_b2);
	    if ((pointA1.DistanceTo(pointA2) < isect_tol && pointB1.DistanceTo(pointB2) < isect_tol)
		|| (std::isnan(t_a2) || std::isnan(t_b2))) {
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
		    if (curve_segments_have_matching_nurbs_forms(curveA, t_a1,
			    t_a2, curveB, t_b1, t_b2) ||
			overlap_candidate_fits_common_chord(curveA, t_a1, t_a2,
			    curveB, t_b1, t_b2, pointA1, pointA2, overlap_tol)) {
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
		    // A non-flat candidate may contain only endpoint intersections.
		    // Fall through to report those points instead of an overlap.
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

static void
newton_csi(double &t, double &u, double &v, const ON_Curve *curveA, const ON_Surface *surfB, double isect_tol, Subsurface *tree)
{
    // Equations:
    //   C_x(t) - S_x(u, v) = 0
    //   C_y(t) - S_y(u, v) = 0
    //   C_z(t) - S_z(u, v) = 0
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
	// F[0] = C_x(t) - S_x(u, v)
	// F[1] = C_y(t) - S_y(u, v)
	// F[2] = C_z(t) - S_z(u, v)
	// J[i][0] = dF[i]/dt = C'(t)
	// J[i][1] = dF[i]/du = -dS(u, v)/du
	// J[i][2] = dF[i]/dv = -dS(u, v)/dv
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

    double diagonal_3d = root->m_surf->BoundingBox().Diagonal().Length();
    if (!ON_NearZero(diagonal_3d)) {
	double surf_ulen = surf_udomain ? surf_udomain->Length() : surf->Domain(0).Length();
	double surf_vlen = surf_vdomain ? surf_vdomain->Length() : surf->Domain(1).Length();
	double diagonal_2d = ON_2dVector(surf_ulen, surf_vlen).Length();

	tol_2d = (tol_3d * diagonal_2d) / diagonal_3d;
    }
    return tol_2d;
}


static void
boundary_t_to_uv(
    double &surf_u,
    double &surf_v,
    const Subsurface *surf,
    int boundary_index,
    double boundary_t)
{
    switch (boundary_index) {
	case 0:
	    surf_u = surf->m_u.Min();
	    surf_v = surf->m_v.ParameterAt(boundary_t);
	    break;
	case 1:
	    surf_u = surf->m_u.ParameterAt(boundary_t);
	    surf_v = surf->m_v.Max();
	    break;
	case 2:
	    surf_u = surf->m_u.Max();
	    surf_v = surf->m_v.ParameterAt(boundary_t);
	    break;
	case 3:
	    surf_u = surf->m_u.ParameterAt(boundary_t);
	    surf_v = surf->m_v.Min();
	    break;
    }
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
    IntersectionBudgetScope budget_scope;
    if (budget_scope.IsBudgetExceeded()) {
	return budget_scope.BudgetExceededResult();
    }

    std::vector<ON_Curve *> ovlp2d;

    if (curveA == NULL || surfaceB == NULL) {
	return 0;
    }

    int original_count = x.Count();
    if (overlap2d) {
	while (overlap2d->Count() < original_count)
	    overlap2d->Append(NULL);
    }

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
		const ON_2dPoint surface_parameter(px_event[0].m_b[0],
		    px_event[0].m_b[1]);
		overlap2d->Append(new ON_LineCurve(surface_parameter,
		    surface_parameter));
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
    double t_tol = tolerance_2d_from_3d(isect_tol, rootA, curveA,
					curveA_domain);

    double u_tol = isect_tol;
    double diagonal_3d = rootB->m_surf->BoundingBox().Diagonal().Length();
    if (!ON_NearZero(diagonal_3d)) {
	double diagonal_2d =
	    surfaceB_udomain ? surfaceB_udomain->Length() : surfaceB->Domain(0).Length();
	u_tol = isect_tol * (diagonal_2d / diagonal_3d);
    }
    double v_tol = isect_tol;
    if (!ON_NearZero(diagonal_3d)) {
	double diagonal_2d =
	    surfaceB_vdomain ? surfaceB_vdomain->Length() : surfaceB->Domain(1).Length();
	v_tol = isect_tol * (diagonal_2d / diagonal_3d);
    }

    typedef std::vector<std::pair<Subcurve *, Subsurface *> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootB->Intersect(*rootA, isect_tol)) {
	if (!budget_scope.ConsumeCandidatePair()) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    if (treeB == NULL) {
		delete rootB;
	    }
	    return budget_scope.BudgetExceededResult();
	}
	candidates.push_back(std::make_pair(rootA, rootB));
    }
    size_t candidate_pair_count = candidates.size();

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
			if (candidate_pair_count >= MAX_CSI_CANDIDATE_PAIRS ||
			    !budget_scope.ConsumeCandidatePair()) {
			    x.SetCount(original_count);
			    if (treeA == NULL) {
				delete rootA;
			    }
			    if (treeB == NULL) {
				delete rootB;
			    }
			    return budget_scope.BudgetExceededResult();
			}
			++candidate_pair_count;
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

		    if (plane.DistanceTo(line.from) < isect_tol) {
			// The line is on the surface's plane. The end-points of
			// the overlap must be the linecurve's end-points or
			// the intersection between the linecurve and the boundary
			// of the surface.

			ON_X_EVENT event;

			ON_Line boundary[4];
			ON_SimpleArray<double> line_t, boundary_t;
			ON_SimpleArray<int> boundary_index;
			boundary[0] = ON_Line(point1, point2);
			boundary[1] = ON_Line(point2, point3);
			boundary[2] = ON_Line(point4, point3);
			boundary[3] = ON_Line(point1, point4);

			int intersections = 0;

			// first, if the line overlaps any of the boundaries
			// then that is the one and only intersection
			for (int j = 0; j < 4; j++) {
			    if (line.Direction().IsParallelTo(boundary[j].Direction())) {
				double min_dist = minimum_distance(line, boundary[j], isect_tol);

				if (min_dist < isect_tol) {
				    double t_a1, t_a2, t_b1, t_b2;
				    int ret = intersect_overlapping_lines(
					t_a1, t_a2, t_b1, t_b2, line,
					boundary[j], t_tol);

				    event.m_type = ON_X_EVENT::csx_point;
				    event.m_A[0] = line.PointAt(t_a1);
				    event.m_B[0] = boundary[j].PointAt(t_b1);

				    event.m_a[0] = i->first->m_t.ParameterAt(t_a1);
				    boundary_t_to_uv(event.m_b[0],
						     event.m_b[1],
						     i->second, j, t_b1);

				    if (ret == 2) {
					event.m_type = ON_X_EVENT::csx_overlap;
					event.m_A[1] = line.PointAt(t_a2);
					event.m_B[1] = boundary[j].PointAt(t_b2);

					event.m_a[1] = i->first->m_t.ParameterAt(t_a2);
					boundary_t_to_uv(event.m_b[2],
							 event.m_b[3],
							 i->second, j, t_b2);
				    }
				    tmp_x.Append(event);
				    ++intersections;
				    break;
				}
			    }
			}

			if (intersections > 0) {
			    continue;
			}

			// having ruled out a boundary overlap, there
			// must be one intersection point inside the
			// boundary of the surface, and another either
			// inside or on the boundary
			//
			// first we check the endpoints of the line segment (PSI)
			ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
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
			for (int j = 0; j < 4; j++) {
			    double t1, t2;
			    if (ON_IntersectLineLine(line, boundary[j], &t1, &t2, isect_tol, true)) {
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

			for (int j = 0; j < count && intersections < 2; j++) {
			    ON_3dPoint line_pt = line.PointAt(line_t[j]);

			    if (intersections > 0 &&
				event.m_A[0].DistanceTo(line_pt) < isect_tol)
			    {
				// skip duplicate intersection point
				continue;
			    }

			    // convert from the boxes' domain to the whole surface's domain
			    double surf_u = 0.0, surf_v = 0.0;
			    boundary_t_to_uv(surf_u, surf_v, i->second,
					     boundary_index[j], boundary_t[j]);

			    event.m_A[intersections] = line_pt;
			    event.m_B[intersections] = surfaceB->PointAt(surf_u, surf_v);
			    event.m_a[intersections] = i->first->m_t.ParameterAt(line_t[j]);
			    event.m_b[2 * intersections] = surf_u;
			    event.m_b[2 * intersections + 1] = surf_v;
			    ++intersections;
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
	newton_csi(t1, u1, v1, curveA, surfaceB, isect_tol, rootB);
	double u2 = i->second->m_u.Mid(), v2 = i->second->m_v.Mid();
	double t2 = i->first->m_t.Max();
	newton_csi(t2, u2, v2, curveA, surfaceB, isect_tol, rootB);

	if (std::isnan(u1) || std::isnan(v1) || std::isnan(t1)) {
	    u1 = u2;
	    v1 = v2;
	    t1 = t2;
	}

	if (std::isnan(u1) || std::isnan(v1) || std::isnan(t1)) {
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
		    ovlp2d.push_back(curve_fitting(polyline));
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
	    ovlp2d.push_back(curve_fitting(polyline));
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
		ovlp2d.push_back(NULL);
	    }
	}
    }

    if (overlap2d) {
	for (size_t i = 0; i < ovlp2d.size(); i++) {
	    overlap2d->Append(ovlp2d[i]);
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


static bool
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
    int indexA = -1;
    int indexB = -1;
    double distance3d = DBL_MAX;
    double dist_uA = DBL_MAX;
    double dist_vA = DBL_MAX;
    double dist_uB = DBL_MAX;
    double dist_vB = DBL_MAX;
    double tol = DBL_MAX;
    bool operator < (const PointPair &pp) const
    {
	if (ON_NearZero(distance3d - pp.distance3d, tol)) {
	    return dist_uA + dist_vA + dist_uB + dist_vB < pp.dist_uA + pp.dist_vA + pp.dist_uB + pp.dist_vB;
	}
	return distance3d < pp.distance3d;
    }
};


static void
add_points_to_closed_seams(
    const ON_Surface *surfA,
    const ON_Surface *surfB,
    ON_3dPointArray &curvept,
    ON_2dPointArray &curve_uvA,
    ON_2dPointArray &curve_uvB)
{

    if (!curve_uvA.Count() || !curve_uvB.Count()) {
	return;
    }

    for (int i = 0; i < 4; ++i) {
	int dir = i % 2;
	bool doA = i < 2;

	bool is_closed = (doA ? surfA : surfB)->IsClosed(dir);
	if (!is_closed) {
	    continue;
	}
	const ON_Interval domain = (doA ? surfA : surfB)->Domain(dir);
	double dmin = domain.Min();
	double dmax = domain.Max();

	int orig_count = curvept.Count();
	for (int j = 0; j < orig_count; ++j) {
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


static bool
newton_ssi(double &uA, double &vA, double &uB, double &vB, const ON_Surface *surfA, const ON_Surface *surfB, double isect_tol)
{
    ON_3dPoint pointA = surfA->PointAt(uA, vA);
    ON_3dPoint pointB = surfB->PointAt(uB, vB);
    if (pointA.DistanceTo(pointB) < isect_tol) {
	return true;
    }

    // Equations:
    //   x_a(uA, vA) - x_b(uB, vB) = 0
    //   y_a(uA, vA) - y_b(uB, vB) = 0
    //   z_a(uA, vA) - z_b(uB, vB) = 0
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
    return (pointA.DistanceTo(pointB) < isect_tol) && !std::isnan(uA) && !std::isnan(vA) && !std::isnan(uB) && !std::isnan(vB);
}

// if curve end is greater than tol distance from pt, append a linear
// segment to the curve so it extends to the pt
static void
extend_curve_end_to_pt(ON_Curve *&curve, ON_3dPoint pt, double tol)
{
    ON_NurbsCurve *nc = curve->NurbsCurve();
    if (nc->PointAtEnd().DistanceTo(pt) > tol) {
	ON_LineCurve line_bridge(nc->PointAtEnd(), pt);

	ON_NurbsCurve bridge;
	if (line_bridge.GetNurbForm(bridge)) {
	    nc->Append(bridge);
	}
	delete curve;
	curve = nc;
    }
}

static ON_Curve *
link_curves(ON_Curve *&c1, ON_Curve *&c2)
{
    extend_curve_end_to_pt(c1, c2->PointAtStart(), ON_ZERO_TOLERANCE);

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

// if curve start and end are within tolerance, append a linear
// segment to the curve to close it completely so it passes
// IsClosed() tests
static void
fill_gap_if_closed(ON_Curve *&curve, double tol)
{
    if (curve->PointAtStart().DistanceTo(curve->PointAtEnd()) <= tol) {
	extend_curve_end_to_pt(curve, curve->PointAtStart(), ON_ZERO_TOLERANCE);
    }
}

struct OverlapSegment {
    ON_Curve *m_curve3d, *m_curveA, *m_curveB;

    int m_from;	    // 0: iso-curve from A, 1: from B
    int m_dir;	    // 0: u iso-curve, 1: v iso-curve
    double m_fix;   // the param fixed in the iso-curve
    // m_dir==0: m_fix==u; m_dir==1: m_fix==v
    double m_min;   // minimum of the variable param in the iso-curve
    double m_max;   // maximum of the variable param in the iso-curve

    OverlapSegment(void)
    {
	m_curve3d = m_curveA = m_curveB = NULL;
    }

    OverlapSegment(const OverlapSegment *other)
	: m_from(other->m_from), m_dir(other->m_dir), m_fix(other->m_fix),
	  m_min(other->m_min), m_max(other->m_max)
    {
	m_curve3d = other->m_curve3d->Duplicate();
	m_curveA = other->m_curveA->Duplicate();
	m_curveB = other->m_curveB->Duplicate();
    }

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
    // Resolve events by array index because appending can relocate them.
    ON_ClassArray<ON_SSX_EVENT> *m_events;
    int m_event_index;
    ON_BoundingBox m_bboxA; // 2D surfA bounding box of the closed region.
    enum TYPE {
	undefined = 0,
	outer = 1,
	inner = 2
    } m_type;

    std::vector<int> m_inside;

    Overlapevent() : m_events(NULL), m_event_index(-1), m_type(undefined) {}

    Overlapevent(ON_ClassArray<ON_SSX_EVENT> *events, int event_index) :
	m_events(events), m_event_index(event_index), m_type(undefined)
    {
	assert(m_events != NULL && m_event_index >= 0 &&
	       m_event_index < m_events->Count());
	m_bboxA = Event().m_curveA->BoundingBox();
    }

    ON_SSX_EVENT &Event()
    {
	assert(m_events != NULL && m_event_index >= 0 &&
	       m_event_index < m_events->Count());
	return (*m_events)[m_event_index];
    }

    const ON_SSX_EVENT &Event() const
    {
	assert(m_events != NULL && m_event_index >= 0 &&
	       m_event_index < m_events->Count());
	return (*m_events)[m_event_index];
    }

    bool IsPointOnBoundary(const ON_2dPoint &_pt) const
    {
	ON_ClassArray<ON_PX_EVENT> px_event;
	return ON_Intersect(ON_3dPoint(_pt), *(Event().m_curveA), px_event);
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
	ON_Intersect(&linecurve, Event().m_curveA, x_event);
	int count = x_event.Count();
	for (int i = 0; i < x_event.Count(); i++) {
	    // Find tangent intersections.
	    // What should we do if it's ccx_overlap?
	    if (Event().m_curveA->TangentAt(x_event[i].m_a[0]).IsParallelTo(linecurve.m_line.Direction())) {
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
	    if (ON_Intersect(&linecurve[i], Event().m_curveA, x_event[i])) {
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
	return !ON_Intersect(Event().m_curve3d, _curve, x_event) && IsPointIn(_curve->PointAtStart());
    }
};


static std::vector<double>
get_bezier_knots(const ON_Surface *surf, int dir);


enum {
    INSIDE_OVERLAP,
    ON_OVERLAP_BOUNDARY,
    OUTSIDE_OVERLAP
};


enum OverlapSideLocation {
    OVERLAP_SIDES_UNDEFINED,
    OVERLAP_SIDES_INSIDE,
    OVERLAP_SIDES_OUTSIDE,
    OVERLAP_SIDES_LEFT,
    OVERLAP_SIDES_RIGHT
};


static bool
is_pt_in_surf_overlap(
    ON_2dPoint surf1_pt,
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    Subsurface *surf2_tree,
    double isect_tol)
{
    if (!surf1->Domain(0).Includes(surf1_pt.x) ||
	!surf1->Domain(1).Includes(surf1_pt.y))
    {
	return false;
    }
    bool surf1_pt_intersects_surf2, surfs_parallel_at_pt = false;
    ON_ClassArray<ON_PX_EVENT> px_event;

    surf1_pt_intersects_surf2 = ON_Intersect(surf1->PointAt(surf1_pt.x, surf1_pt.y),
					     *surf2, px_event, isect_tol, 0, 0, surf2_tree);

    if (surf1_pt_intersects_surf2) {
	surfs_parallel_at_pt = surf1->NormalAt(surf1_pt.x, surf1_pt.y).IsParallelTo(
	    surf2->NormalAt(px_event[0].m_b[0], px_event[0].m_b[1])) != 0;
    }
    return surf1_pt_intersects_surf2 && surfs_parallel_at_pt;
}


static void
append_overlap_parameter(
    std::vector<double> &parameters,
    double parameter,
    const ON_Interval &domain,
    double tolerance)
{
    if (parameter > domain.Min() + tolerance &&
	parameter < domain.Max() - tolerance) {
	parameters.push_back(parameter);
    }
}


static bool
append_curve_span_parameters(
    std::vector<double> &parameters,
    const ON_Curve *curve,
    const ON_Interval &domain,
    double tolerance)
{
    const int span_count = curve->SpanCount();
    if (span_count < 1) {
	return false;
    }

    std::vector<double> spans(static_cast<size_t>(span_count) + 1);
    if (!curve->GetSpanVector(spans.data())) {
	return false;
    }

    for (size_t span_index = 0; span_index < spans.size(); ++span_index) {
	append_overlap_parameter(parameters, spans[span_index], domain,
	    tolerance);
    }
    return true;
}


static bool
normalize_overlap_parameters(
    std::vector<double> &parameters,
    const ON_Interval &domain,
    double tolerance)
{
    parameters.push_back(domain.Min());
    parameters.push_back(domain.Max());
    std::sort(parameters.begin(), parameters.end());

    std::vector<double> unique_parameters;
    for (size_t parameter_index = 0;
	 parameter_index < parameters.size(); ++parameter_index) {
	if (unique_parameters.empty() || parameters[parameter_index] -
	    unique_parameters.back() > tolerance) {
	    unique_parameters.push_back(parameters[parameter_index]);
	}
    }
    if (unique_parameters.size() < 2) {
	return false;
    }

    unique_parameters.front() = domain.Min();
    unique_parameters.back() = domain.Max();
    parameters.swap(unique_parameters);
    return true;
}


static bool
append_curve_intersection_parameters(
    std::vector<double> &parameters,
    const ON_Curve *curve,
    const ON_Curve *boundary_curve,
    const ON_Interval &domain,
    double isect_tol,
    double parameter_tolerance)
{
    ON_SimpleArray<ON_X_EVENT> events;
    const int result = ON_Intersect(curve, boundary_curve, events, isect_tol);
    if (result < 0) {
	return false;
    }

    for (int event_index = 0; event_index < events.Count(); ++event_index) {
	append_overlap_parameter(parameters, events[event_index].m_a[0], domain,
	    parameter_tolerance);
	append_overlap_parameter(parameters, events[event_index].m_a[1], domain,
	    parameter_tolerance);
    }
    return true;
}


static bool
append_surface_knot_parameters(
    std::vector<double> &parameters,
    const ON_Curve *curve,
    const ON_Surface *surface,
    const ON_Interval &domain,
    double isect_tol,
    double parameter_tolerance)
{
    for (int direction = 0; direction < 2; ++direction) {
	const std::vector<double> knots = get_bezier_knots(surface, direction);
	for (size_t knot_index = 0; knot_index < knots.size(); ++knot_index) {
	    ON_Curve *knot_curve = surface->IsoCurve(direction,
		knots[knot_index]);
	    if (knot_curve == NULL) {
		return false;
	    }

	    const bool appended = append_curve_intersection_parameters(
		parameters, curve, knot_curve, domain, isect_tol,
		parameter_tolerance);
	    delete knot_curve;
	    if (!appended) {
		return false;
	    }
	}
    }
    return true;
}


static bool
append_surface_parameter_knot_parameters(
    std::vector<double> &parameters,
    const ON_Curve *curve,
    const ON_Surface *surface,
    const ON_Interval &domain,
    double isect_tol,
    double parameter_tolerance)
{
    const ON_Interval u_domain = surface->Domain(0);
    const ON_Interval v_domain = surface->Domain(1);
    for (int direction = 0; direction < 2; ++direction) {
	const std::vector<double> knots = get_bezier_knots(surface, direction);
	for (size_t knot_index = 0; knot_index < knots.size(); ++knot_index) {
	    ON_LineCurve knot_curve(direction == 0 ?
		ON_2dPoint(knots[knot_index], v_domain.Min()) :
		ON_2dPoint(u_domain.Min(), knots[knot_index]),
		direction == 0 ?
		ON_2dPoint(knots[knot_index], v_domain.Max()) :
		ON_2dPoint(u_domain.Max(), knots[knot_index]));
	    if (!append_curve_intersection_parameters(parameters, curve,
		&knot_curve, domain, isect_tol, parameter_tolerance)) {
		return false;
	    }
	}
    }
    return true;
}


static OverlapSideLocation
overlap_side_location(
    const ON_2dPoint &left_point,
    const ON_2dPoint &right_point,
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    Subsurface *surf2_tree,
    double isect_tol)
{
    const bool left_inside = is_pt_in_surf_overlap(left_point, surf1, surf2,
	surf2_tree, isect_tol);
    const bool right_inside = is_pt_in_surf_overlap(right_point, surf1, surf2,
	surf2_tree, isect_tol);
    if (left_inside && right_inside) {
	return OVERLAP_SIDES_INSIDE;
    }
    if (!left_inside && !right_inside) {
	return OVERLAP_SIDES_OUTSIDE;
    }
    return left_inside ? OVERLAP_SIDES_LEFT : OVERLAP_SIDES_RIGHT;
}


static OverlapSideLocation
overlap_curve_side_location(
    const ON_Curve *curve,
    const ON_Surface *surfA,
    const ON_Surface *surfB,
    Subsurface *treeB,
    double isect_tol,
    double isect_tolA)
{
    const ON_Interval domain = curve->Domain();
    const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE,
	isect_tolA);
    std::vector<double> parameters;
    if (!append_curve_span_parameters(parameters, curve, domain,
	parameter_tolerance) ||
	!append_surface_parameter_knot_parameters(parameters, curve, surfA,
	    domain, parameter_tolerance, parameter_tolerance) ||
	!normalize_overlap_parameters(parameters, domain, parameter_tolerance)) {
	return OVERLAP_SIDES_UNDEFINED;
    }

    const double test_distance = 2.0 * parameter_tolerance;
    OverlapSideLocation boundary_location = OVERLAP_SIDES_UNDEFINED;
    bool found_nonboundary = false;
    for (size_t parameter_index = 0;
	 parameter_index + 1 < parameters.size(); ++parameter_index) {
	const double start = parameters[parameter_index];
	const double end = parameters[parameter_index + 1];
	if (end - start <= parameter_tolerance) {
	    continue;
	}

	const double sample = (start + end) * 0.5;
	const ON_3dPoint curve_point = curve->PointAt(sample);
	ON_3dVector normal = ON_CrossProduct(curve->TangentAt(sample),
	    ON_3dVector::ZAxis);
	if (!normal.Unitize()) {
	    return OVERLAP_SIDES_UNDEFINED;
	}
	const ON_3dVector offset = normal * test_distance;
	const ON_2dPoint left_point(curve_point.x - offset.x,
	    curve_point.y - offset.y);
	const ON_2dPoint right_point(curve_point.x + offset.x,
	    curve_point.y + offset.y);
	const OverlapSideLocation sample_location = overlap_side_location(
	    left_point, right_point, surfA, surfB, treeB, isect_tol);
	if (sample_location == OVERLAP_SIDES_INSIDE ||
	    sample_location == OVERLAP_SIDES_OUTSIDE) {
	    if (boundary_location != OVERLAP_SIDES_UNDEFINED) {
		return OVERLAP_SIDES_UNDEFINED;
	    }
	    found_nonboundary = true;
	    continue;
	}
	if (found_nonboundary ||
	    (boundary_location != OVERLAP_SIDES_UNDEFINED &&
	     boundary_location != sample_location)) {
	    return OVERLAP_SIDES_UNDEFINED;
	}
	boundary_location = sample_location;
    }

    return boundary_location;
}


static bool
is_subsurfaceA_completely_inside_overlap(
    const ON_ClassArray<Overlapevent> &overlap_events,
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
		const int inner_index = outerloop->m_inside[j];
		if (inner_index < 0 || inner_index >= overlap_events.Count()) {
		    return false;
		}
		const Overlapevent *innerloop = &overlap_events[inner_index];
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


static bool
is_uvA_completely_inside_overlap(
    const ON_ClassArray<Overlapevent> &overlap_events,
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
		const int inner_index = outerloop->m_inside[j];
		if (inner_index < 0 || inner_index >= overlap_events.Count()) {
		    return false;
		}
		const Overlapevent *innerloop = &overlap_events[inner_index];
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


static ON_2dPoint
point_xy_or_yx(double x, double y, bool swap)
{
    ON_2dPoint pt(x, y);
    if (swap) {
	std::swap(pt.x, pt.y);
    }
    return pt;
}

struct SurfaceKnot {
    double c;
    int dir;
    std::vector<double> v;
    int idx;

    SurfaceKnot(int k_dir, std::vector<double> k_v, int k_idx)
	: dir(k_dir), v(k_v), idx(k_idx)
    {
	c = v[idx];
    }

    bool
    IsFirst() {
	return idx == 0;
    }

    bool
    IsLast() {
	return idx == ((int)v.size() - 1);
    }
};


struct KnotIsocurveSource {
    bool is_surfA;
    const ON_Surface *surf;
    SurfaceKnot knot;

    KnotIsocurveSource(bool iso_surfA, const ON_Surface *iso_surf,
		       SurfaceKnot iso_knot)
	: is_surfA(iso_surfA), surf(iso_surf), knot(iso_knot)
    {
    }
};


struct OverlapIsocurve {
    KnotIsocurveSource src;
    const ON_Curve *curve;
    double overlap_t[2];
    ON_2dPoint overlap_uv[2];

    OverlapIsocurve(KnotIsocurveSource iso_src, const ON_Curve *iso_curve,
		    double ol_start, double ol_end)
	: src(iso_src), curve(iso_curve)
    {
	overlap_t[0] = ol_start;
	overlap_t[1] = ol_end;

	bool swap_xy = src.knot.dir != 0;
	overlap_uv[0] = point_xy_or_yx(overlap_t[0], src.knot.c, swap_xy);
	overlap_uv[1] = point_xy_or_yx(overlap_t[1], src.knot.c, swap_xy);
    }
};


static int
isocurve_surface_overlap_location(
    OverlapIsocurve iso,
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    Subsurface *surf2_tree,
    double isect_tol,
    double isect_tol1)
{
    ON_Interval interval(iso.overlap_t[0], iso.overlap_t[1]);
    interval.MakeIncreasing();
    const double parameter_tolerance = std::max(ON_ZERO_TOLERANCE,
	isect_tol1);
    std::vector<double> parameters;
    if (!append_curve_span_parameters(parameters, iso.curve, interval,
	parameter_tolerance) ||
	!append_surface_knot_parameters(parameters, iso.curve, surf2, interval,
	    isect_tol, parameter_tolerance) ||
	!normalize_overlap_parameters(parameters, interval, parameter_tolerance)) {
	return ON_OVERLAP_BOUNDARY;
    }

    const double test_distance = 2.0 * parameter_tolerance;
    const double knot = iso.src.knot.c;
    const bool swap_xy = iso.src.knot.dir == 1;
    bool found_inside = false;
    bool found_outside = false;
    bool found_boundary = false;
    for (size_t parameter_index = 0;
	 parameter_index + 1 < parameters.size(); ++parameter_index) {
	const double start = parameters[parameter_index];
	const double end = parameters[parameter_index + 1];
	if (end - start <= parameter_tolerance) {
	    continue;
	}

	const double sample = (start + end) * 0.5;
	const ON_2dPoint left_point = point_xy_or_yx(sample,
	    knot - test_distance, swap_xy);
	const ON_2dPoint right_point = point_xy_or_yx(sample,
	    knot + test_distance, swap_xy);
	switch (overlap_side_location(left_point, right_point, surf1, surf2,
	    surf2_tree, isect_tol)) {
	    case OVERLAP_SIDES_INSIDE:
		found_inside = true;
		break;
	    case OVERLAP_SIDES_OUTSIDE:
		found_outside = true;
		break;
	    case OVERLAP_SIDES_LEFT:
	    case OVERLAP_SIDES_RIGHT:
		found_boundary = true;
		break;
	    default:
		return ON_OVERLAP_BOUNDARY;
	}
    }

    if (found_boundary || (found_inside && found_outside)) {
	return ON_OVERLAP_BOUNDARY;
    }
    if (found_inside) {
	return INSIDE_OVERLAP;
    }
    if (found_outside) {
	return OUTSIDE_OVERLAP;
    }
    return ON_OVERLAP_BOUNDARY;
}


static bool
is_curve_on_overlap_boundary(
    OverlapIsocurve iso,
    const ON_Surface *surf1,
    const ON_Surface *surf2,
    Subsurface *surf2_tree,
    double isect_tol,
    double isect_tol1)
{
    bool ret = false;

    if (iso.overlap_t[0] < iso.overlap_t[1]) {
	int location = isocurve_surface_overlap_location(iso, surf1, surf2,
		surf2_tree, isect_tol, isect_tol1);

	ret = (location == ON_OVERLAP_BOUNDARY);
    }
    return ret;
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
	for (int j = i + 1; j < count_before; j++) {

	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(overlaps[i]->m_curve3d, overlaps[j]->m_curve3d, x_event, isect_tol);

	    for (int k = 0; k < x_event.Count(); k++) {
		ON_3dPoint param;
		ON_2dPoint uvA, uvB;
		ON_ClassArray<ON_PX_EVENT> psxA, psxB, pcxA, pcxB;

		ON_3dPoint start_i = x_event[k].m_A[0];
		ON_3dPoint start_j = x_event[k].m_B[0];
		ON_3dPoint end_i = x_event[k].m_A[1];
		ON_3dPoint end_j = x_event[k].m_B[1];
		double start_ti = x_event[k].m_a[0];
		double start_tj = x_event[k].m_b[0];
		double end_ti = x_event[k].m_a[1];
		double end_tj = x_event[k].m_b[1];

		// the curves may have opposite directions, so make
		// sure the ends are consistent
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap &&
		    start_i.DistanceTo(start_j) > start_i.DistanceTo(end_j)) {
		    std::swap(start_j, end_j);
		    std::swap(start_tj, end_tj);
		}
		// ensure that the curve intersection point also
		// intersects both surfaces (i.e. the point is in the
		// overlap)
		if (ON_Intersect(start_i, *surfA, psxA, isect_tolA, 0, 0, treeA)
		    && ON_Intersect(start_j, *surfB, psxB, isect_tolB, 0, 0, treeB)) {
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
			param.x = start_ti;
			param.y = pcxA[0].m_b[0];
			param.z = pcxB[0].m_b[0];
			params[i].Append(param);
		    }
		    pcxA.SetCount(0);
		    pcxB.SetCount(0);
		    if (ON_Intersect(uvA, *(overlaps[j]->m_curveA), pcxA, isect_tolA)
			&& ON_Intersect(uvB, *(overlaps[j]->m_curveB), pcxB, isect_tolB)) {
			// the same routine for overlaps[j]
			param.x = start_tj;
			param.y = pcxA[0].m_b[0];
			param.z = pcxB[0].m_b[0];
			params[j].Append(param);
		    }
		}
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap) {
		    // for overlap, have to do the same with the intersection end points
		    psxA.SetCount(0);
		    psxB.SetCount(0);
		    if (ON_Intersect(end_i, *surfA, psxA, isect_tol, 0, 0, treeA)
			&& ON_Intersect(end_j, *surfB, psxB, isect_tol, 0, 0, treeB)) {
			// pull the 3D curve back to the 2D space
			uvA = psxA[0].m_b;
			uvB = psxB[0].m_b;
			pcxA.SetCount(0);
			pcxB.SetCount(0);
			if (ON_Intersect(uvA, *(overlaps[i]->m_curveA), pcxA, isect_tolA)
			    && ON_Intersect(uvB, *(overlaps[i]->m_curveB), pcxB, isect_tolB)) {
			    param.x = end_ti;
			    param.y = pcxA[0].m_b[0];
			    param.z = pcxB[0].m_b[0];
			    params[i].Append(param);
			}
			pcxA.SetCount(0);
			pcxB.SetCount(0);
			if (ON_Intersect(uvA, *(overlaps[j]->m_curveA), pcxA, isect_tolA)
			    && ON_Intersect(uvB, *(overlaps[j]->m_curveB), pcxB, isect_tolB)) {
			    // the same routine for overlaps[j]
			    param.x = end_tj;
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


static bool
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


static void
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
	    ON_Curve *subcurveA = NULL;
	    try {
		subcurveA = sub_curve(overlaps[i]->m_curveA,
				      params[i][start].y, params[i][j].y);
	    } catch (InvalidInterval &) {
		continue;
	    }
	    bool isvalid = false, isreversed = false;
	    const OverlapSideLocation side_location = overlap_curve_side_location(
		subcurveA, surfA, surfB, treeB, isect_tol, isect_tolA);
	    if (side_location == OVERLAP_SIDES_LEFT) {
		isvalid = true;
	    } else if (side_location == OVERLAP_SIDES_RIGHT) {
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


static std::vector<Subsurface *>
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


static std::vector<Subsurface *>
subdivide_subsurface_direction(Subsurface *sub, int direction,
	std::vector<std::unique_ptr<Subsurface> > &owned_nodes,
	std::map<std::pair<Subsurface *, int>,
	    std::pair<Subsurface *, Subsurface *> > &split_cache)
{
    std::vector<Subsurface *> parts;
    if (sub == NULL || sub->m_surf == NULL ||
	(direction != 0 && direction != 1))
    {
	parts.push_back(sub);
	return parts;
    }

    const std::pair<Subsurface *, int> cache_key(sub, direction);
    const auto cached = split_cache.find(cache_key);
    if (cached != split_cache.end()) {
	if (cached->second.second == NULL) {
	    parts.push_back(sub);
	} else {
	    parts.push_back(cached->second.first);
	    parts.push_back(cached->second.second);
	}
	return parts;
    }

    ON_Surface *first_surface = NULL;
    ON_Surface *second_surface = NULL;
    const ON_Interval domain = sub->m_surf->Domain(direction);
    double split_parameter = domain.Mid();
    const ON_RevSurface *revolution = ON_RevSurface::Cast(sub->m_surf);
    const int profile_direction = revolution && revolution->m_bTransposed ?
	0 : 1;
    if (revolution != NULL && revolution->m_curve != NULL &&
	direction == profile_direction && domain.IsIncreasing())
    {
	ON_3dVector axis_direction = revolution->m_axis.Direction();
	const ON_3dPoint first_point =
	    revolution->m_curve->PointAt(domain.Min());
	const ON_3dPoint second_point =
	    revolution->m_curve->PointAt(domain.Max());
	if (axis_direction.Unitize() && first_point.IsValid() &&
	    second_point.IsValid())
	{
	    const double first_axis = ON_DotProduct(
		first_point - revolution->m_axis.from, axis_direction);
	    const double second_axis = ON_DotProduct(
		second_point - revolution->m_axis.from, axis_direction);
	    if (std::isfinite(first_axis) && std::isfinite(second_axis) &&
		first_axis != second_axis)
	    {
		const double target_axis = 0.5 * (first_axis + second_axis);
		double lower_parameter = domain.Min();
		double upper_parameter = domain.Max();
		double lower_value = first_axis - target_axis;
		for (unsigned int iteration = 0; iteration < 48; ++iteration) {
		    const double middle_parameter =
			0.5 * (lower_parameter + upper_parameter);
		    const ON_3dPoint middle_point =
			revolution->m_curve->PointAt(middle_parameter);
		    if (!middle_point.IsValid())
			break;
		    const double middle_value = ON_DotProduct(
			middle_point - revolution->m_axis.from,
			axis_direction) - target_axis;
		    if (!std::isfinite(middle_value))
			break;
		    split_parameter = middle_parameter;
		    if (middle_value == 0.0)
			break;
		    if ((lower_value < 0.0 && middle_value > 0.0) ||
			(lower_value > 0.0 && middle_value < 0.0))
		    {
			upper_parameter = middle_parameter;
		    } else {
			lower_parameter = middle_parameter;
			lower_value = middle_value;
		    }
		}
	    }
	}
    }
    if (!domain.IsIncreasing() || !domain.Includes(split_parameter, true) ||
	!sub->m_surf->Split(direction, split_parameter,
	    first_surface, second_surface) ||
	!first_surface || !second_surface)
    {
	delete first_surface;
	delete second_surface;
	split_cache[cache_key] = std::make_pair(sub,
	    static_cast<Subsurface *>(NULL));
	parts.push_back(sub);
	return parts;
    }

    std::unique_ptr<Subsurface> first(new Subsurface(first_surface));
    std::unique_ptr<Subsurface> second(new Subsurface(second_surface));
    first->m_u = second->m_u = sub->m_u;
    first->m_v = second->m_v = sub->m_v;
    if (direction == 0) {
	first->m_u = ON_Interval(sub->m_u.Min(), split_parameter);
	second->m_u = ON_Interval(split_parameter, sub->m_u.Max());
    } else {
	first->m_v = ON_Interval(sub->m_v.Min(), split_parameter);
	second->m_v = ON_Interval(split_parameter, sub->m_v.Max());
    }

    Subsurface *first_part = first.get();
    Subsurface *second_part = second.get();
    parts.push_back(first_part);
    parts.push_back(second_part);
    owned_nodes.push_back(std::move(first));
    owned_nodes.push_back(std::move(second));
    split_cache[cache_key] = std::make_pair(first_part, second_part);
    return parts;
}


static int
nearly_linear_revolution_profile_direction(const ON_Surface *surface,
	double tolerance)
{
    const ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    if (revolution == NULL || revolution->m_curve == NULL ||
	!(tolerance > 0.0))
    {
	return -1;
    }

    const double angle = std::fabs(revolution->m_angle.Length());
    ON_3dVector axis_direction = revolution->m_axis.Direction();
    const ON_BoundingBox profile_bounds =
	revolution->m_curve->BoundingBox();
    if (!(angle > 0.0) || angle >= ON_PI ||
	!axis_direction.Unitize() || !profile_bounds.IsValid())
    {
	return -1;
    }

    double maximum_radius = 0.0;
    for (int x = 0; x < 2; ++x) {
	for (int y = 0; y < 2; ++y) {
	    for (int z = 0; z < 2; ++z) {
		const ON_3dPoint corner(
		    x ? profile_bounds.m_max.x : profile_bounds.m_min.x,
		    y ? profile_bounds.m_max.y : profile_bounds.m_min.y,
		    z ? profile_bounds.m_max.z : profile_bounds.m_min.z);
		const ON_3dVector offset = corner - revolution->m_axis.from;
		const ON_3dVector radial = offset - axis_direction *
		    ON_DotProduct(offset, axis_direction);
		maximum_radius = std::max(maximum_radius, radial.Length());
	    }
	}
    }

    /* A chord in the sweep direction differs from the exact revolution by
     * at most this sag.  When that error is already below the requested SSI
     * tolerance, refine only the profile direction; Newton refinement still
     * evaluates the original exact surface. */
    const double maximum_sag = maximum_radius *
	(1.0 - std::cos(0.5 * angle));
    if (!std::isfinite(maximum_sag) || maximum_sag > tolerance)
	return -1;
    return revolution->m_bTransposed ? 0 : 1;
}


struct RevolutionRefinementGeometry {
    ON_Line axis;
    int profile_direction;
};


static bool
revolution_refinement_geometry(const ON_Surface *surface, double tolerance,
	RevolutionRefinementGeometry &geometry)
{
    if (surface == NULL)
	return false;

    const ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    if (revolution != NULL && revolution->m_curve != NULL) {
	ON_3dVector direction = revolution->m_axis.Direction();
	if (!direction.Unitize())
	    return false;
	geometry.axis = ON_Line(revolution->m_axis.from,
	    revolution->m_axis.from + direction);
	geometry.profile_direction = revolution->m_bTransposed ? 0 : 1;
	return true;
    }

    ON_Cylinder cylinder;
    if (!surface->IsCylinder(&cylinder,
	    std::max(tolerance, ON_ZERO_TOLERANCE)) || !cylinder.IsValid())
    {
	return false;
    }
    ON_3dVector axis_direction = cylinder.Axis();
    if (!axis_direction.Unitize())
	return false;

    const ON_Interval u_domain = surface->Domain(0);
    const ON_Interval v_domain = surface->Domain(1);
    ON_3dPoint point;
    ON_3dVector derivative_u;
    ON_3dVector derivative_v;
    if (!u_domain.IsIncreasing() || !v_domain.IsIncreasing() ||
	!surface->Ev1Der(u_domain.Mid(), v_domain.Mid(), point,
	    derivative_u, derivative_v) ||
	!derivative_u.Unitize() || !derivative_v.Unitize())
    {
	return false;
    }
    const double axial_u = std::fabs(ON_DotProduct(derivative_u,
	axis_direction));
    const double axial_v = std::fabs(ON_DotProduct(derivative_v,
	axis_direction));
    if (std::max(axial_u, axial_v) < 0.9 ||
	std::fabs(axial_u - axial_v) < 0.5)
    {
	return false;
    }

    geometry.axis = ON_Line(cylinder.Center(),
	cylinder.Center() + axis_direction);
    geometry.profile_direction = axial_u > axial_v ? 0 : 1;
    return true;
}


static bool
parallel_revolution_refinement_directions(const ON_Surface *surface_a,
	const ON_Surface *surface_b, double tolerance,
	int &profile_direction_a, int &profile_direction_b,
	ON_Line &axis_a, ON_Line &axis_b)
{
    RevolutionRefinementGeometry geometry_a;
    RevolutionRefinementGeometry geometry_b;
    if (!revolution_refinement_geometry(surface_a, tolerance, geometry_a) ||
	!revolution_refinement_geometry(surface_b, tolerance, geometry_b))
    {
	return false;
    }

    ON_3dVector direction_a = geometry_a.axis.Direction();
    ON_3dVector direction_b = geometry_b.axis.Direction();
    if (!direction_a.Unitize() || !direction_b.Unitize() ||
	ON_CrossProduct(direction_a, direction_b).Length() >
	    std::max(ON_ZERO_TOLERANCE, 256.0 * ON_EPSILON))
    {
	return false;
    }

    profile_direction_a = geometry_a.profile_direction;
    profile_direction_b = geometry_b.profile_direction;
    axis_a = geometry_a.axis;
    axis_b = geometry_b.axis;
    return true;
}


static bool
coaxial_revolution_refinement_directions(const ON_Surface *surface_a,
	const ON_Surface *surface_b, double tolerance,
	int &profile_direction_a, int &profile_direction_b,
	ON_Line &common_axis)
{
    RevolutionRefinementGeometry geometry_a;
    RevolutionRefinementGeometry geometry_b;
    if (!revolution_refinement_geometry(surface_a, tolerance, geometry_a) ||
	!revolution_refinement_geometry(surface_b, tolerance, geometry_b))
    {
	return false;
    }

    ON_3dVector direction_a = geometry_a.axis.Direction();
    ON_3dVector direction_b = geometry_b.axis.Direction();
    if (!direction_a.Unitize() || !direction_b.Unitize() ||
	ON_CrossProduct(direction_a, direction_b).Length() >
	    std::max(ON_ZERO_TOLERANCE, 256.0 * ON_EPSILON))
    {
	return false;
    }

    const ON_3dVector axis_offset =
	geometry_b.axis.from - geometry_a.axis.from;
    double coordinate_scale = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(geometry_a.axis.from[axis]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(geometry_b.axis.from[axis]));
    }
    const double axis_tolerance = std::max(tolerance,
	1024.0 * ON_EPSILON * coordinate_scale);
    if (ON_CrossProduct(axis_offset, direction_a).Length() > axis_tolerance)
	return false;

    profile_direction_a = geometry_a.profile_direction;
    profile_direction_b = geometry_b.profile_direction;
    common_axis = geometry_a.axis;
    return true;
}


static bool
project_revolution_profile_to_radius_axis(
    const ON_RevSurface &revolution,
    const ON_3dPoint &axis_origin,
    const ON_3dVector &axis_direction,
    double tolerance,
    ON_NurbsCurve &projected)
{
    if (revolution.m_curve == NULL || !(tolerance > 0.0) ||
	!axis_origin.IsValid() || !axis_direction.IsUnitVector() ||
	revolution.m_curve->GetNurbForm(projected) <= 0 ||
	!projected.IsValid() || projected.Dimension() != 3)
    {
	return false;
    }

    std::vector<ON_3dPoint> controls;
    std::vector<double> weights;
    controls.reserve(projected.CVCount());
    weights.reserve(projected.CVCount());
    ON_3dVector radial_direction;
    double maximum_radial_length = 0.0;
    double coordinate_scale = 1.0;
    for (int control_index = 0; control_index < projected.CVCount();
	 ++control_index)
    {
	ON_3dPoint control;
	double weight = 1.0;
	if (projected.IsRational()) {
	    ON_4dPoint homogeneous;
	    if (!projected.GetCV(control_index, homogeneous) ||
		!std::isfinite(homogeneous.w) ||
		homogeneous.w <= ON_ZERO_TOLERANCE)
	    {
		return false;
	    }
	    weight = homogeneous.w;
	    control = ON_3dPoint(homogeneous.x / weight,
		homogeneous.y / weight, homogeneous.z / weight);
	} else if (!projected.GetCV(control_index, control)) {
	    return false;
	}
	if (!control.IsValid())
	    return false;

	controls.push_back(control);
	weights.push_back(weight);
	const ON_3dVector offset = control - axis_origin;
	const ON_3dVector radial = offset - axis_direction *
	    ON_DotProduct(offset, axis_direction);
	if (radial.Length() > maximum_radial_length) {
	    maximum_radial_length = radial.Length();
	    radial_direction = radial;
	}
	for (int axis_index = 0; axis_index < 3; ++axis_index)
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(control[axis_index]));
    }
    if (!radial_direction.Unitize())
	return false;

    const double representation_tolerance = std::max(tolerance,
	1024.0 * ON_EPSILON * coordinate_scale);
    const ON_3dVector binormal = ON_CrossProduct(axis_direction,
	radial_direction);
    double minimum_radius = std::numeric_limits<double>::infinity();
    double maximum_radius = -std::numeric_limits<double>::infinity();
    double maximum_off_plane = 0.0;
    for (const ON_3dPoint &control : controls) {
	const ON_3dVector offset = control - axis_origin;
	const double radius = ON_DotProduct(offset, radial_direction);
	minimum_radius = std::min(minimum_radius, radius);
	maximum_radius = std::max(maximum_radius, radius);
	maximum_off_plane = std::max(maximum_off_plane,
	    std::fabs(ON_DotProduct(offset, binormal)));
    }
    if (std::fabs(minimum_radius) > std::fabs(maximum_radius)) {
	radial_direction = -radial_direction;
	minimum_radius = -maximum_radius;
    }
    if (minimum_radius < -representation_tolerance ||
	maximum_off_plane > representation_tolerance)
    {
	return false;
    }

    for (int control_index = 0; control_index < projected.CVCount();
	 ++control_index)
    {
	const ON_3dVector offset = controls[control_index] - axis_origin;
	double radius = ON_DotProduct(offset, radial_direction);
	if (radius < 0.0 && radius >= -representation_tolerance)
	    radius = 0.0;
	const double axis_parameter = ON_DotProduct(offset, axis_direction);
	const double weight = weights[control_index];
	const bool set = projected.IsRational() ?
	    projected.SetCV(control_index, ON_4dPoint(radius * weight,
		axis_parameter * weight, 0.0, weight)) :
	    projected.SetCV(control_index,
		ON_3dPoint(radius, axis_parameter, 0.0));
	if (!set)
	    return false;
    }

    return projected.IsValid();
}


static bool
coaxial_revolution_profiles_are_disjoint(
    const ON_Surface *surface_a,
    const ON_Surface *surface_b,
    const ON_Line &axis,
    double tolerance,
    bool &disjoint)
{
    disjoint = false;
    const ON_RevSurface *revolution_a = ON_RevSurface::Cast(surface_a);
    const ON_RevSurface *revolution_b = ON_RevSurface::Cast(surface_b);
    ON_3dVector axis_direction = axis.Direction();
    if (revolution_a == NULL || revolution_b == NULL ||
	!axis_direction.Unitize() || !(tolerance > 0.0))
    {
	return false;
    }

    ON_NurbsCurve profile_a;
    ON_NurbsCurve profile_b;
    if (!project_revolution_profile_to_radius_axis(*revolution_a,
	    axis.from, axis_direction, tolerance, profile_a) ||
	!project_revolution_profile_to_radius_axis(*revolution_b,
	    axis.from, axis_direction, tolerance, profile_b))
    {
	return false;
    }

    ON_SimpleArray<ON_X_EVENT> profile_events;
    const int intersection_count = ON_Intersect(&profile_a, &profile_b,
	profile_events, tolerance, 2.0 * tolerance);
    if (intersection_count < 0)
	return false;

    disjoint = intersection_count == 0 && profile_events.Count() == 0;
    return true;
}


static bool
subsurface_control_hull_projection(const Subsurface *subsurface,
	const ON_3dVector &direction, double &minimum_projection,
	double &maximum_projection);


struct CoaxialProfileBounds {
    double minimum_radius;
    double maximum_radius;
    double minimum_axis;
    double maximum_axis;
};


static bool
revolution_profile_bounds(const ON_RevSurface &revolution,
	const ON_3dPoint &axis_origin, const ON_3dVector &axis_direction,
	double tolerance, CoaxialProfileBounds &bounds)
{
    if (revolution.m_curve == NULL)
	return false;
    ON_NurbsCurve profile;
    if (revolution.m_curve->GetNurbForm(profile) <= 0 || !profile.IsValid())
	return false;

    std::vector<ON_3dPoint> controls;
    controls.reserve(profile.CVCount());
    ON_3dVector radial_direction;
    double maximum_radial_length = 0.0;
    for (int control_index = 0; control_index < profile.CVCount();
	 ++control_index)
    {
	ON_3dPoint control;
	if (profile.IsRational()) {
	    ON_4dPoint homogeneous;
	    if (!profile.GetCV(control_index, homogeneous) ||
		!std::isfinite(homogeneous.w) ||
		homogeneous.w <= ON_ZERO_TOLERANCE)
	    {
		return false;
	    }
	    control = ON_3dPoint(homogeneous.x / homogeneous.w,
		homogeneous.y / homogeneous.w,
		homogeneous.z / homogeneous.w);
	} else if (!profile.GetCV(control_index, control)) {
	    return false;
	}
	if (!control.IsValid())
	    return false;
	controls.push_back(control);
	const ON_3dVector offset = control - axis_origin;
	const ON_3dVector radial = offset - axis_direction *
	    ON_DotProduct(offset, axis_direction);
	if (radial.Length() > maximum_radial_length) {
	    maximum_radial_length = radial.Length();
	    radial_direction = radial;
	}
    }
    if (!radial_direction.Unitize())
	return false;

    const ON_3dVector binormal = ON_CrossProduct(axis_direction,
	radial_direction);
    double minimum_signed_radius = std::numeric_limits<double>::infinity();
    double maximum_signed_radius = -std::numeric_limits<double>::infinity();
    bounds.minimum_axis = std::numeric_limits<double>::infinity();
    bounds.maximum_axis = -std::numeric_limits<double>::infinity();
    double coordinate_scale = 1.0;
    double maximum_off_plane = 0.0;
    for (const ON_3dPoint &control : controls) {
	const ON_3dVector offset = control - axis_origin;
	const double signed_radius = ON_DotProduct(offset, radial_direction);
	const double axis_parameter = ON_DotProduct(offset, axis_direction);
	minimum_signed_radius = std::min(minimum_signed_radius, signed_radius);
	maximum_signed_radius = std::max(maximum_signed_radius, signed_radius);
	bounds.minimum_axis = std::min(bounds.minimum_axis, axis_parameter);
	bounds.maximum_axis = std::max(bounds.maximum_axis, axis_parameter);
	maximum_off_plane = std::max(maximum_off_plane,
	    std::fabs(ON_DotProduct(offset, binormal)));
	for (int axis = 0; axis < 3; ++axis)
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(control[axis]));
    }
    const double plane_tolerance = std::max(tolerance,
	1024.0 * ON_EPSILON * coordinate_scale);
    if (maximum_off_plane > plane_tolerance)
	return false;

    bounds.maximum_radius = std::max(std::fabs(minimum_signed_radius),
	std::fabs(maximum_signed_radius));
    bounds.minimum_radius = minimum_signed_radius <= 0.0 &&
	maximum_signed_radius >= 0.0 ? 0.0 :
	std::min(std::fabs(minimum_signed_radius),
	    std::fabs(maximum_signed_radius));
    return std::isfinite(bounds.minimum_radius) &&
	std::isfinite(bounds.maximum_radius) &&
	std::isfinite(bounds.minimum_axis) &&
	std::isfinite(bounds.maximum_axis);
}


static bool
coaxial_subsurface_profile_bounds(const Subsurface *subsurface,
	const ON_3dPoint &axis_origin, const ON_3dVector &axis_direction,
	double tolerance, CoaxialProfileBounds &bounds)
{
    if (subsurface == NULL || subsurface->m_surf == NULL)
	return false;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(subsurface->m_surf);
    if (revolution != NULL)
	return revolution_profile_bounds(*revolution, axis_origin,
	    axis_direction, tolerance, bounds);

    ON_Cylinder cylinder;
    if (!subsurface->m_surf->IsCylinder(&cylinder,
	    std::max(tolerance, ON_ZERO_TOLERANCE)) || !cylinder.IsValid())
    {
	return false;
    }
    const double radius = std::fabs(cylinder.circle.Radius());
    if (!std::isfinite(radius) ||
	!subsurface_control_hull_projection(subsurface, axis_direction,
	    bounds.minimum_axis, bounds.maximum_axis))
    {
	return false;
    }
    const double origin_parameter = axis_origin.x * axis_direction.x +
	axis_origin.y * axis_direction.y + axis_origin.z * axis_direction.z;
    bounds.minimum_axis -= origin_parameter;
    bounds.maximum_axis -= origin_parameter;
    bounds.minimum_radius = radius;
    bounds.maximum_radius = radius;
    return true;
}


static bool
coaxial_subsurface_profiles_may_intersect(const Subsurface *surface_a,
	const Subsurface *surface_b, const ON_Line &axis, double tolerance)
{
    ON_3dVector axis_direction = axis.Direction();
    if (!axis_direction.Unitize())
	return true;
    CoaxialProfileBounds bounds_a;
    CoaxialProfileBounds bounds_b;
    if (!coaxial_subsurface_profile_bounds(surface_a, axis.from,
	    axis_direction, tolerance, bounds_a) ||
	!coaxial_subsurface_profile_bounds(surface_b, axis.from,
	    axis_direction, tolerance, bounds_b))
    {
	return true;
    }
    return !(bounds_a.maximum_radius < bounds_b.minimum_radius - tolerance ||
	bounds_b.maximum_radius < bounds_a.minimum_radius - tolerance ||
	bounds_a.maximum_axis < bounds_b.minimum_axis - tolerance ||
	bounds_b.maximum_axis < bounds_a.minimum_axis - tolerance);
}


static bool
parallel_revolution_subsurface_profiles_may_intersect(
    const Subsurface *surface_a, const Subsurface *surface_b,
    const ON_Line &axis_a, const ON_Line &axis_b, double tolerance)
{
    ON_3dVector axis_direction_a = axis_a.Direction();
    ON_3dVector axis_direction_b = axis_b.Direction();
    if (!axis_direction_a.Unitize() || !axis_direction_b.Unitize() ||
	ON_CrossProduct(axis_direction_a, axis_direction_b).Length() >
	    std::max(ON_ZERO_TOLERANCE, 256.0 * ON_EPSILON))
    {
	return true;
    }

    CoaxialProfileBounds bounds_a;
    CoaxialProfileBounds bounds_b;
    if (!coaxial_subsurface_profile_bounds(surface_a, axis_a.from,
	    axis_direction_a, tolerance, bounds_a) ||
	!coaxial_subsurface_profile_bounds(surface_b, axis_b.from,
	    axis_direction_a, tolerance, bounds_b))
    {
	return true;
    }

    const ON_3dVector axis_offset = axis_b.from - axis_a.from;
    const double axial_offset = ON_DotProduct(axis_offset,
	axis_direction_a);
    bounds_b.minimum_axis += axial_offset;
    bounds_b.maximum_axis += axial_offset;
    if (bounds_a.maximum_axis < bounds_b.minimum_axis - tolerance ||
	bounds_b.maximum_axis < bounds_a.minimum_axis - tolerance)
    {
	return false;
    }

    const ON_3dVector transverse_offset = axis_offset -
	axis_direction_a * axial_offset;
    const double axis_separation = transverse_offset.Length();
    if (!std::isfinite(axis_separation))
	return true;

    /* At any common axial coordinate the two revolution surfaces lie on
	 * circles about their respective axes.  Circle intersection requires the
	 * center separation to lie between the sum and absolute difference of
	 * their radii.  Interval bounds give a conservative rejection test for
	 * whole profile bands without constraining either angular sector. */
    if (axis_separation > bounds_a.maximum_radius +
	bounds_b.maximum_radius + tolerance)
    {
	return false;
    }
    double minimum_radius_difference = 0.0;
    if (bounds_a.minimum_radius > bounds_b.maximum_radius) {
	minimum_radius_difference = bounds_a.minimum_radius -
	    bounds_b.maximum_radius;
    } else if (bounds_b.minimum_radius > bounds_a.maximum_radius) {
	minimum_radius_difference = bounds_b.minimum_radius -
	    bounds_a.maximum_radius;
    }
    return minimum_radius_difference <= axis_separation + tolerance;
}


struct SSIRefinementCandidate {
    SSIRefinementCandidate(Subsurface *surface_a, Subsurface *surface_b,
	unsigned int depth_a = 0, unsigned int depth_b = 0)
	: surfaceA(surface_a), surfaceB(surface_b), depthA(depth_a),
	  depthB(depth_b)
    {
    }

    Subsurface *surfaceA;
    Subsurface *surfaceB;
    unsigned int depthA;
    unsigned int depthB;
};


static double
subsurface_physical_span(Subsurface *subsurface)
{
    if (subsurface == NULL)
	return 0.0;

    ON_3dPoint minimum;
    ON_3dPoint maximum;
    subsurface->GetBBox(minimum, maximum);
    const double span = minimum.DistanceTo(maximum);
    return std::isfinite(span) ? span : 0.0;
}


static bool
refine_first_ssi_surface(const SSIRefinementCandidate &candidate,
	bool can_refine_A, bool can_refine_B)
{
    if (!can_refine_B)
	return true;
    if (!can_refine_A)
	return false;

    const double spanA = subsurface_physical_span(candidate.surfaceA);
    const double spanB = subsurface_physical_span(candidate.surfaceB);
    if (spanA != spanB)
	return spanA > spanB;

    /* Keep equal-sized patches at comparable refinement depths. */
    return candidate.depthA <= candidate.depthB;
}


static bool
subsurface_control_hull_projection(
    const Subsurface *subsurface,
    const ON_3dVector &direction,
    double &minimum_projection,
    double &maximum_projection)
{
    if (subsurface == NULL || subsurface->m_surf == NULL) {
	return false;
    }

    ON_NurbsSurface nurbs_form;
    const ON_NurbsSurface *nurbs = ON_NurbsSurface::Cast(subsurface->m_surf);
    if (nurbs == NULL) {
	/* Directional refinement deliberately preserves analytic surface
	 * classes such as ON_RevSurface.  Their boxes can remain much looser
	 * than the exact rational control hull, especially while one parameter
	 * direction is still unsplit.  Use an exact NURBS form for the separating
	 * projection test without replacing the surface used by Newton solving. */
	if (subsurface->m_surf->GetNurbForm(nurbs_form, 0.0) <= 0 ||
	    !nurbs_form.IsValid())
	{
	    return false;
	}
	nurbs = &nurbs_form;
    }

    bool have_projection = false;
    for (int u_index = 0; u_index < nurbs->CVCount(0); ++u_index) {
	for (int v_index = 0; v_index < nurbs->CVCount(1); ++v_index) {
	    ON_3dPoint control_point;
	    if (nurbs->IsRational()) {
		ON_4dPoint control_vertex;
		if (!nurbs->GetCV(u_index, v_index, control_vertex) ||
		    !std::isfinite(control_vertex.x) ||
		    !std::isfinite(control_vertex.y) ||
		    !std::isfinite(control_vertex.z) ||
		    !std::isfinite(control_vertex.w) ||
		    control_vertex.w <= ON_ZERO_TOLERANCE)
		{
		    return false;
		}
		control_point = ON_3dPoint(control_vertex.x / control_vertex.w,
		    control_vertex.y / control_vertex.w,
		    control_vertex.z / control_vertex.w);
	    } else if (!nurbs->GetCV(u_index, v_index, control_point)) {
		return false;
	    }

	    if (!control_point.IsValid()) {
		return false;
	    }
	    const double projection = control_point.x * direction.x +
		control_point.y * direction.y + control_point.z * direction.z;
	    if (!std::isfinite(projection)) {
		return false;
	    }
	    if (!have_projection) {
		minimum_projection = projection;
		maximum_projection = projection;
		have_projection = true;
	    } else {
		minimum_projection = std::min(minimum_projection, projection);
		maximum_projection = std::max(maximum_projection, projection);
	    }
	}
    }

    return have_projection;
}


static bool
subsurface_control_hulls_may_intersect(
    const Subsurface *subsurface_a,
    const Subsurface *subsurface_b,
    double tolerance)
{
    const Subsurface *subsurfaces[2] = {subsurface_a, subsurface_b};
    const double separation_tolerance = std::max(0.0, tolerance);
    for (int subsurface_index = 0; subsurface_index < 2;
	 ++subsurface_index)
    {
	const Subsurface *subsurface = subsurfaces[subsurface_index];
	if (subsurface == NULL || subsurface->m_surf == NULL) {
	    continue;
	}

	ON_Interval u_domain = subsurface->m_surf->Domain(0);
	ON_Interval v_domain = subsurface->m_surf->Domain(1);
	u_domain.MakeIncreasing();
	v_domain.MakeIncreasing();
	if (!u_domain.IsIncreasing() || !v_domain.IsIncreasing()) {
	    continue;
	}

	ON_3dVector normal = subsurface->m_surf->NormalAt(u_domain.Mid(),
	    v_domain.Mid());
	if (!normal.Unitize()) {
	    continue;
	}

	double minimum_a = 0.0;
	double maximum_a = 0.0;
	double minimum_b = 0.0;
	double maximum_b = 0.0;
	if (!subsurface_control_hull_projection(subsurface_a, normal, minimum_a,
		maximum_a) ||
	    !subsurface_control_hull_projection(subsurface_b, normal, minimum_b,
		maximum_b))
	{
	    continue;
	}

	if (maximum_a < minimum_b - separation_tolerance ||
	    maximum_b < minimum_a - separation_tolerance)
	{
	    return false;
	}
    }

    return true;
}


static std::pair<Triangle, Triangle>
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


static ON_2dPoint
barycentric_to_uv(const ON_3dPoint &bc, const Triangle &tri)
{
    ON_3dVector vertx(tri.a_2d.x, tri.b_2d.x, tri.c_2d.x);
    ON_3dVector verty(tri.a_2d.y, tri.b_2d.y, tri.c_2d.y);
    ON_2dPoint uv(ON_DotProduct(bc, vertx), ON_DotProduct(bc, verty));
    return uv;
}


static bool
join_continuous_ppair_to_polyline(ON_SimpleArray<int> *polyline, const PointPair &ppair)
{
    int polyline_first = (*polyline)[0];
    int polyline_last = (*polyline)[polyline->Count() - 1];

    if (polyline_first == ppair.indexA) {
	polyline->Insert(0, ppair.indexB);
	return true;
    }
    if (polyline_first == ppair.indexB) {
	polyline->Insert(0, ppair.indexA);
	return true;
    }
    if (polyline_last == ppair.indexA) {
	polyline->Append(ppair.indexB);
	return true;
    }
    if (polyline_last == ppair.indexB) {
	polyline->Append(ppair.indexA);
	return true;
    }
    return false;
}


static std::vector<double>
get_knots(const ON_Surface *surf, int dir)
{
    int knot_count = surf->SpanCount(dir) + 1;

    double *surf_knots = new double[knot_count];
    surf->GetSpanVector(dir, surf_knots);

    std::vector<double> out;
    for (int i = 0; i < knot_count; ++i) {
	out.push_back(surf_knots[i]);
    }
    delete[] surf_knots;

    return out;
}


// get surface knots in given direction that can be boundaries of
// Bezier patches
static std::vector<double>
get_bezier_knots(
    const ON_Surface *surf,
    int dir)
{
    std::vector<double> bezier_knots, knots = get_knots(surf, dir);

    bezier_knots.push_back(knots[0]);
    for (size_t i = 1; i < knots.size(); ++i) {
	if (knots[i] > bezier_knots.back()) {
	    bezier_knots.push_back(knots[i]);
	}
    }
    if (surf->IsClosed(dir)) {
	bezier_knots.pop_back();
    }
    return bezier_knots;
}


static void
append_overlap_segments(
    ON_SimpleArray<OverlapSegment *> &overlaps,
    OverlapIsocurve iso,
    const ON_Curve *overlap2d,
    const ON_Surface *surf1)
{
    std::vector<OverlapSegment *> ovlps;
    OverlapSegment *seg = new OverlapSegment;
    try {
	seg->m_curve3d = sub_curve(iso.curve, iso.overlap_t[0],
				   iso.overlap_t[1]);
    } catch (InvalidInterval &e) {
	bu_log("%s", e.what());
	delete seg;
	return;
    }
    seg->m_curveA = overlap2d->Duplicate();
    seg->m_curveB = new ON_LineCurve(iso.overlap_uv[0], iso.overlap_uv[1]);
    if (iso.src.is_surfA) {
	std::swap(seg->m_curveA, seg->m_curveB);
    }
    int surf_dir = 1 - iso.src.knot.dir;
    seg->m_dir = surf_dir;
    seg->m_fix = iso.src.knot.c;

    ovlps.push_back(seg);

    if (iso.src.knot.IsFirst() && surf1->IsClosed(surf_dir)) {
	// Something like close_domain().
	// If the domain is closed, the iso-curve
	// on the first knot and the last knot is
	// the same, so we don't need to compute
	// the intersections twice.
	seg = new OverlapSegment(seg);
	seg->m_fix = get_knots(surf1, surf_dir).back();
	bool swap_xy = (1 - surf_dir) != 0;
	ON_2dPoint iso_pt1 = point_xy_or_yx(iso.overlap_t[0], seg->m_fix, swap_xy);
	ON_2dPoint iso_pt2 = point_xy_or_yx(iso.overlap_t[1], seg->m_fix, swap_xy);
	ON_Curve *&surf1_curve = iso.src.is_surfA ? seg->m_curveA :
	    seg->m_curveB;
	delete surf1_curve;
	surf1_curve = new ON_LineCurve(iso_pt1, iso_pt2);

	ovlps.push_back(seg);
    }

    for (size_t i = 0; i < ovlps.size(); i++) {
	overlaps.Append(ovlps[i]);
    }
}


static void
append_csx_event_points(
    ON_3dPointArray &curveA_3d,
    ON_2dPointArray &surfB_2d,
    const ON_SimpleArray<ON_X_EVENT> &events)
{
    for (int i = 0; i < events.Count(); ++i) {
	const ON_X_EVENT &event = events[i];

	curveA_3d.Append(event.m_A[0]);
	surfB_2d.Append(ON_2dPoint(event.m_b[0], event.m_b[1]));

	if (event.m_type == ON_X_EVENT::csx_overlap) {
	    curveA_3d.Append(event.m_A[1]);
	    surfB_2d.Append(ON_2dPoint(event.m_b[2], event.m_b[3]));
	}
    }
}


// Make a transverse or tangent ssx curve event from matching 3d and surface
// A/B curves.
static int
set_ssx_event_from_curves(
    ON_SSX_EVENT &event,
    ON_Curve *c3d,
    ON_Curve *uvA,
    ON_Curve *uvB,
    const ON_Surface *surfA,
    const ON_Surface *surfB)
{
    int status = 0;

    event.m_curve3d = c3d;
    event.m_curveA = uvA;
    event.m_curveB = uvB;

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
		status = 1;
	    }
	}
	event.m_type = ON_SSX_EVENT::ssx_tangent;
    } else {
	event.m_type = ON_SSX_EVENT::ssx_transverse;
    }

    return status;
}


static bool
affine_planar_surface_line_interval(
    const AffinePlanarSurface &surface,
    const ON_Line &line,
    double parameter_tolerance,
    bool &has_interval,
    double &minimum,
    double &maximum)
{
    has_interval = false;
    ON_2dPoint line_start, line_end;
    if (!surface.ParametersAt(line.from, line_start) ||
	!surface.ParametersAt(line.to, line_end))
    {
	return false;
    }

    minimum = -DBL_MAX;
    maximum = DBL_MAX;
    const ON_2dVector line_direction = line_end - line_start;
    const ON_Interval domains[2] = {surface.m_u_domain, surface.m_v_domain};
    const double coordinates[2] = {line_start.x, line_start.y};
    const double directions[2] = {line_direction.x, line_direction.y};
    for (int coordinate_index = 0; coordinate_index < 2;
	 ++coordinate_index)
    {
	const double lower = domains[coordinate_index].Min() -
	    parameter_tolerance;
	const double upper = domains[coordinate_index].Max() +
	    parameter_tolerance;
	if (!std::isfinite(coordinates[coordinate_index]) ||
	    !std::isfinite(directions[coordinate_index]) ||
	    !std::isfinite(lower) || !std::isfinite(upper))
	{
	    return false;
	}
	if (ON_NearZero(directions[coordinate_index])) {
	    if (coordinates[coordinate_index] < lower ||
		coordinates[coordinate_index] > upper)
	    {
		return true;
	    }
	    continue;
	}

	double first = (lower - coordinates[coordinate_index]) /
	    directions[coordinate_index];
	double second = (upper - coordinates[coordinate_index]) /
	    directions[coordinate_index];
	if (first > second) {
	    std::swap(first, second);
	}
	minimum = std::max(minimum, first);
	maximum = std::min(maximum, second);
	if (minimum > maximum) {
	    return true;
	}
    }

    has_interval = true;
    return true;
}


static bool
append_affine_planar_ssi_event(
    const ON_Surface *surfA,
    const ON_Surface *surfB,
    ON_ClassArray<ON_SSX_EVENT> &events,
    double isect_tol,
    double isect_tolA,
    double isect_tolB)
{
    AffinePlanarSurface affineA, affineB;
    ON_Plane planeA, planeB;
    if (!affine_planar_surface_mapping(surfA, affineA) ||
	!affine_planar_surface_mapping(surfB, affineB) ||
	!surfA->IsPlanar(&planeA) || !surfB->IsPlanar(&planeB))
    {
	return false;
    }

    ON_Line line;
    if (!ON_Intersect(planeA, planeB, line)) {
	return false;
    }

    bool has_intervalA = false;
    bool has_intervalB = false;
    double minimumA = 0.0;
    double maximumA = 0.0;
    double minimumB = 0.0;
    double maximumB = 0.0;
    if (!affine_planar_surface_line_interval(affineA, line, isect_tolA,
	has_intervalA, minimumA, maximumA) ||
	!affine_planar_surface_line_interval(affineB, line, isect_tolB,
	has_intervalB, minimumB, maximumB))
    {
	return false;
    }
    if (!has_intervalA || !has_intervalB) {
	return true;
    }

    const double line_start_parameter = std::max(minimumA, minimumB);
    const double line_end_parameter = std::min(maximumA, maximumB);
    if (line_start_parameter > line_end_parameter) {
	return true;
    }

    const ON_3dPoint line_start = line.PointAt(line_start_parameter);
    const ON_3dPoint line_end = line.PointAt(line_end_parameter);
    if (!line_start.IsValid() || !line_end.IsValid() ||
	line_start.DistanceTo(line_end) <= isect_tol)
    {
	return false;
    }

    ON_2dPoint line_startA, line_endA, line_startB, line_endB;
    if (!affineA.ParametersAt(line_start, line_startA) ||
	!affineA.ParametersAt(line_end, line_endA) ||
	!affineB.ParametersAt(line_start, line_startB) ||
	!affineB.ParametersAt(line_end, line_endB))
    {
	return false;
    }
    if (surfA->PointAt(line_startA.x, line_startA.y).DistanceTo(line_start) >
	isect_tol ||
	surfA->PointAt(line_endA.x, line_endA.y).DistanceTo(line_end) >
	isect_tol ||
	surfB->PointAt(line_startB.x, line_startB.y).DistanceTo(line_start) >
	isect_tol ||
	surfB->PointAt(line_endB.x, line_endB.y).DistanceTo(line_end) >
	isect_tol)
    {
	return false;
    }

    ON_LineCurve *curve3d = new ON_LineCurve(line_start, line_end);
    ON_LineCurve *curveA = new ON_LineCurve(line_startA, line_endA);
    ON_LineCurve *curveB = new ON_LineCurve(line_startB, line_endB);
    if (!curveA->ChangeDimension(2) || !curveB->ChangeDimension(2)) {
	delete curve3d;
	delete curveA;
	delete curveB;
	return false;
    }

    ON_SSX_EVENT event;
    set_ssx_event_from_curves(event, curve3d, curveA, curveB, surfA, surfB);
    events.Append(event);
    event.m_curve3d = NULL;
    event.m_curveA = NULL;
    event.m_curveB = NULL;
    return true;
}


static bool
surface_control_vertex(const ON_NurbsSurface &surface, int u_index,
	int v_index, ON_4dPoint &control_vertex)
{
    if (surface.IsRational()) {
	if (!surface.GetCV(u_index, v_index, control_vertex))
	    return false;
    } else {
	ON_3dPoint control_point;
	if (!surface.GetCV(u_index, v_index, control_point))
	    return false;
	control_vertex = ON_4dPoint(control_point.x, control_point.y,
	    control_point.z, 1.0);
    }

    return std::isfinite(control_vertex.x) &&
	std::isfinite(control_vertex.y) &&
	std::isfinite(control_vertex.z) &&
	std::isfinite(control_vertex.w) &&
	control_vertex.w > ON_ZERO_TOLERANCE;
}


static double
homogeneous_plane_value(const ON_Plane &plane,
	const ON_4dPoint &control_vertex)
{
    const ON_3dPoint control_point(control_vertex.x / control_vertex.w,
	control_vertex.y / control_vertex.w,
	control_vertex.z / control_vertex.w);
    return control_vertex.w * plane.DistanceTo(control_point);
}


/* A surface produced by CreateRuledSurface has one degree-one direction with
 * two control vertices.  If one common parameter in that direction makes
 * every homogeneous control vertex satisfy the plane equation, the
 * corresponding isocurve is the complete plane/surface intersection. */
static bool
plane_ruled_isocurve_parameter(const ON_Plane &plane,
	const ON_NurbsSurface &surface, int ruling_direction,
	double identity_tolerance, double &ruling_parameter)
{
    if (ruling_direction < 0 || ruling_direction > 1 ||
	surface.Degree(ruling_direction) != 1 ||
	surface.CVCount(ruling_direction) != 2)
    {
	return false;
    }

    const int curve_direction = 1 - ruling_direction;
    if (!surface.IsClosed(curve_direction))
	return false;

    bool have_candidate = false;
    double candidate_fraction = 0.0;
    double largest_distance_change = 0.0;
    for (int curve_index = 0;
	 curve_index < surface.CVCount(curve_direction); ++curve_index)
    {
	const int first_u = ruling_direction == 0 ? 0 : curve_index;
	const int first_v = ruling_direction == 0 ? curve_index : 0;
	const int second_u = ruling_direction == 0 ? 1 : curve_index;
	const int second_v = ruling_direction == 0 ? curve_index : 1;
	ON_4dPoint first_control, second_control;
	if (!surface_control_vertex(surface, first_u, first_v,
		first_control) ||
	    !surface_control_vertex(surface, second_u, second_v,
		second_control))
	{
	    return false;
	}

	const double first_value = homogeneous_plane_value(plane,
	    first_control);
	const double second_value = homogeneous_plane_value(plane,
	    second_control);
	const double denominator = second_value - first_value;
	const double distance_change = std::fabs(
	    second_value / second_control.w -
	    first_value / first_control.w);
	if (std::isfinite(denominator) && std::isfinite(distance_change) &&
	    std::fabs(denominator) > ON_ZERO_TOLERANCE &&
	    distance_change > largest_distance_change)
	{
	    const double fraction = -first_value / denominator;
	    if (!std::isfinite(fraction))
		return false;
	    candidate_fraction = fraction;
	    largest_distance_change = distance_change;
	    have_candidate = true;
	}
    }

    if (!have_candidate || largest_distance_change <= identity_tolerance)
	return false;

    const double fraction_tolerance = std::max(64.0 * ON_EPSILON,
	identity_tolerance / largest_distance_change);
    if (candidate_fraction < -fraction_tolerance ||
	candidate_fraction > 1.0 + fraction_tolerance)
    {
	return false;
    }
    candidate_fraction = std::max(0.0,
	std::min(1.0, candidate_fraction));

    /* This is the certificate which distinguishes a genuine constant-
     * parameter intersection from a merely similar-looking sampled curve. */
    for (int curve_index = 0;
	 curve_index < surface.CVCount(curve_direction); ++curve_index)
    {
	const int first_u = ruling_direction == 0 ? 0 : curve_index;
	const int first_v = ruling_direction == 0 ? curve_index : 0;
	const int second_u = ruling_direction == 0 ? 1 : curve_index;
	const int second_v = ruling_direction == 0 ? curve_index : 1;
	ON_4dPoint first_control, second_control;
	if (!surface_control_vertex(surface, first_u, first_v,
		first_control) ||
	    !surface_control_vertex(surface, second_u, second_v,
		second_control))
	{
	    return false;
	}
	const double weight = (1.0 - candidate_fraction) * first_control.w +
	    candidate_fraction * second_control.w;
	const double plane_value = (1.0 - candidate_fraction) *
	    homogeneous_plane_value(plane, first_control) +
	    candidate_fraction * homogeneous_plane_value(plane,
		second_control);
	if (!std::isfinite(weight) || !std::isfinite(plane_value) ||
	    weight <= ON_ZERO_TOLERANCE ||
	    std::fabs(plane_value / weight) > identity_tolerance)
	{
	    return false;
	}
    }

    const ON_Interval ruling_domain = surface.Domain(ruling_direction);
    ruling_parameter = ruling_domain.ParameterAt(candidate_fraction);
    return std::isfinite(ruling_parameter);
}


static bool
parameter_in_domain(double parameter, const ON_Interval &domain,
	double tolerance)
{
    return std::isfinite(parameter) &&
	parameter >= domain.Min() - tolerance &&
	parameter <= domain.Max() + tolerance;
}


static ON_Curve *
affine_planar_parameter_curve(const ON_Curve &model_curve,
	const ON_Plane &plane, const AffinePlanarSurface &mapping,
	double identity_tolerance, bool require_closed = true)
{
    const double u_dot_u = ON_DotProduct(mapping.m_u_direction,
	mapping.m_u_direction);
    const double u_dot_v = ON_DotProduct(mapping.m_u_direction,
	mapping.m_v_direction);
    const double v_dot_v = ON_DotProduct(mapping.m_v_direction,
	mapping.m_v_direction);
    const double determinant = u_dot_u * v_dot_v - u_dot_v * u_dot_v;
    if (std::isfinite(determinant) &&
	determinant > ON_EPSILON * u_dot_u * v_dot_v)
    {
	const ON_3dVector u_dual = (v_dot_v * mapping.m_u_direction -
	    u_dot_v * mapping.m_v_direction) / determinant;
	const ON_3dVector v_dual = (u_dot_u * mapping.m_v_direction -
	    u_dot_v * mapping.m_u_direction) / determinant;
	const ON_3dVector u_coeff = mapping.m_u_domain.Length() * u_dual;
	const ON_3dVector v_coeff = mapping.m_v_domain.Length() * v_dual;
	ON_3dVector normal = plane.Normal();
	if (normal.Unitize()) {
	    ON_Xform parameter_transform = ON_Xform::IdentityTransformation;
	    for (int coordinate = 0; coordinate < 3; ++coordinate) {
		parameter_transform.m_xform[0][coordinate] = u_coeff[coordinate];
		parameter_transform.m_xform[1][coordinate] = v_coeff[coordinate];
		parameter_transform.m_xform[2][coordinate] = normal[coordinate];
	    }
	    parameter_transform.m_xform[0][3] = mapping.m_u_domain.Min() -
		ON_DotProduct(mapping.m_southwest, u_coeff);
	    parameter_transform.m_xform[1][3] = mapping.m_v_domain.Min() -
		ON_DotProduct(mapping.m_southwest, v_coeff);
	    parameter_transform.m_xform[2][3] =
		-ON_DotProduct(mapping.m_southwest, normal);

	    std::unique_ptr<ON_Curve> transformed(model_curve.DuplicateCurve());
	    ON_BoundingBox parameter_bounds;
	    const bool have_bounds = transformed &&
		transformed->Transform(parameter_transform) &&
		transformed->ChangeDimension(2) && transformed->IsValid() &&
		(!require_closed || transformed->IsClosed()) &&
		transformed->GetTightBoundingBox(parameter_bounds) &&
		parameter_bounds.IsValid();
	    bool synchronized = have_bounds;
	    if (synchronized) {
		const ON_Interval domain = model_curve.Domain();
		const int sample_count = std::max(16,
		    2 * model_curve.SpanCount());
		for (int sample_index = 0; sample_index <= sample_count;
		     ++sample_index)
		{
		    const double parameter = domain.ParameterAt(
			(double)sample_index / sample_count);
		    const ON_3dPoint model_point = model_curve.PointAt(parameter);
		    const ON_3dPoint uv = transformed->PointAt(parameter);
		    if (!model_point.IsValid() || !uv.IsValid() ||
			mapping.m_southwest.IsValid() == false ||
			plane.DistanceTo(model_point) > identity_tolerance ||
			plane.DistanceTo(model_point) < -identity_tolerance)
		    {
			synchronized = false;
			break;
		    }
		    const double u_fraction = mapping.m_u_domain.
			NormalizedParameterAt(uv.x);
		    const double v_fraction = mapping.m_v_domain.
			NormalizedParameterAt(uv.y);
		    const ON_3dPoint lifted = mapping.m_southwest +
			u_fraction * mapping.m_u_direction +
			v_fraction * mapping.m_v_direction;
		    if (!lifted.IsValid() ||
			lifted.DistanceTo(model_point) > identity_tolerance)
		    {
			synchronized = false;
			break;
		    }
		}
	    }
	    if (synchronized) {
		return transformed.release();
	    }
	}
    }

    ON_NurbsCurve model_nurbs;
	if (model_curve.GetNurbForm(model_nurbs) <= 0 ||
	!model_nurbs.IsValid())
    {
	return NULL;
    }

    std::unique_ptr<ON_NurbsCurve> parameter_curve(ON_NurbsCurve::New(2,
	model_nurbs.IsRational(), model_nurbs.Order(),
	model_nurbs.CVCount()));
    if (!parameter_curve)
	return NULL;
    for (int knot_index = 0; knot_index < model_nurbs.KnotCount();
	 ++knot_index)
    {
	if (!parameter_curve->SetKnot(knot_index,
		model_nurbs.Knot(knot_index)))
	{
	    return NULL;
	}
    }

    for (int control_index = 0;
	 control_index < model_nurbs.CVCount(); ++control_index)
    {
	ON_4dPoint control_vertex;
	if (!model_nurbs.GetCV(control_index, control_vertex) ||
	    !std::isfinite(control_vertex.x) ||
	    !std::isfinite(control_vertex.y) ||
	    !std::isfinite(control_vertex.z) ||
	    !std::isfinite(control_vertex.w) ||
	    control_vertex.w <= ON_ZERO_TOLERANCE)
	{
	    return NULL;
	}
	const ON_3dPoint control_point(control_vertex.x / control_vertex.w,
	    control_vertex.y / control_vertex.w,
	    control_vertex.z / control_vertex.w);
	if (!control_point.IsValid() ||
	    std::fabs(plane.DistanceTo(control_point)) > identity_tolerance)
	{
	    return NULL;
	}

	ON_2dPoint parameter;
	if (!mapping.ParametersAt(control_point, parameter))
	{
	    return NULL;
	}

	if (model_nurbs.IsRational()) {
	    const double homogeneous[3] = {
		parameter.x * control_vertex.w,
		parameter.y * control_vertex.w,
		control_vertex.w
	    };
	    if (!parameter_curve->SetCV(control_index,
		    ON::homogeneous_rational, homogeneous))
	    {
		return NULL;
	    }
	} else {
	    const double coordinates[2] = {parameter.x, parameter.y};
	    if (!parameter_curve->SetCV(control_index, ON::not_rational,
		    coordinates))
	    {
		return NULL;
	    }
	}
    }

    if (!parameter_curve->IsValid() ||
	(require_closed && !parameter_curve->IsClosed()))
	return NULL;
    return parameter_curve.release();
}


static bool
planar_ruled_event_is_coherent(const ON_Curve &model_curve,
	const ON_Curve &plane_curve, const ON_Curve &ruled_curve,
	const ON_Surface &plane_surface, const ON_Surface &ruled_surface,
	double tolerance)
{
    const ON_Interval domain = model_curve.Domain();
    if (plane_curve.Domain() != domain || ruled_curve.Domain() != domain)
	return false;
    const int sample_count = std::max(16, 2 * model_curve.SpanCount());
    for (int sample_index = 0; sample_index <= sample_count; ++sample_index) {
	const double parameter = domain.ParameterAt(
	    (double)sample_index / sample_count);
	const ON_3dPoint model_point = model_curve.PointAt(parameter);
	const ON_3dPoint plane_parameter = plane_curve.PointAt(parameter);
	const ON_3dPoint ruled_parameter = ruled_curve.PointAt(parameter);
	const double plane_distance = plane_parameter.IsValid() &&
	    model_point.IsValid() ? plane_surface.PointAt(plane_parameter.x,
		plane_parameter.y).DistanceTo(model_point) : DBL_MAX;
	const double ruled_distance = ruled_parameter.IsValid() &&
	    model_point.IsValid() ? ruled_surface.PointAt(ruled_parameter.x,
		ruled_parameter.y).DistanceTo(model_point) : DBL_MAX;
	if (!model_point.IsValid() || !plane_parameter.IsValid() ||
	    !ruled_parameter.IsValid() ||
	    plane_distance > tolerance || ruled_distance > tolerance)
	{
	    bu_log("planar event coherence failed at sample %d: plane %.17g ruled %.17g tolerance %.17g, model %.17g, plane %.17g, ruled %.17g\n",
		sample_index, plane_distance, ruled_distance, tolerance,
		parameter, plane_parameter.x, ruled_parameter.x);
	    return false;
	}
    }
    return true;
}


struct PlanarRuledSectionSample {
    double ruling_parameter;
    ON_3dPoint model_points[2];
    ON_3dPoint plane_parameters[2];
    ON_3dPoint ruled_parameters[2];
};


static bool
sample_planar_ruled_section(const ON_Plane &plane,
	const ON_PlaneSurface &broad_plane_surface,
	const AffinePlanarSurface &plane_mapping,
	const ON_Surface &ruled_surface, int ruling_direction,
	const ON_Interval &curve_domain, double ruling_parameter,
	double isect_tol, double ruled_parameter_tolerance,
	PlanarRuledSectionSample &sample)
{
    const int curve_direction = 1 - ruling_direction;
    std::unique_ptr<ON_Curve> section_curve(ruled_surface.IsoCurve(
	curve_direction, ruling_parameter));
    if (!section_curve || !section_curve->IsValid() ||
	!section_curve->IsClosed() || section_curve->Dimension() != 3)
    {
	return false;
    }

    ON_Interval section_domain = section_curve->Domain();
    section_domain.MakeIncreasing();
    if (!section_domain.IsIncreasing() ||
	std::fabs(section_domain.Min() - curve_domain.Min()) >
	    ruled_parameter_tolerance ||
	std::fabs(section_domain.Max() - curve_domain.Max()) >
	    ruled_parameter_tolerance)
    {
	return false;
    }

    ON_SimpleArray<ON_X_EVENT> section_events;
    const int section_count = ON_Intersect(section_curve.get(),
	&broad_plane_surface, section_events, isect_tol, 2.0 * isect_tol,
	&section_domain, NULL, NULL);
    if (section_count <= 0)
	return false;

    const double curve_length = curve_domain.Length();
    const double parameter_guard = std::max(
	8.0 * ruled_parameter_tolerance,
	4096.0 * ON_EPSILON * std::max(1.0, curve_length));
    std::vector<double> roots;
    for (int event_index = 0; event_index < section_events.Count();
	 ++event_index)
    {
	const ON_X_EVENT &section_event = section_events[event_index];
	if (section_event.m_type != ON_X_EVENT::csx_point)
	    return false;
	const double parameter = section_event.m_a[0];
	if (!parameter_in_domain(parameter, curve_domain,
		ruled_parameter_tolerance) ||
	    parameter <= curve_domain.Min() + parameter_guard ||
	    parameter >= curve_domain.Max() - parameter_guard)
	{
	    /* A root at the periodic seam cannot be paired unambiguously
	     * without splitting the surface parameter curve.  Leave that case
	     * to the general SSI implementation. */
	    return false;
	}

	bool duplicate = false;
	for (size_t root_index = 0; root_index < roots.size(); ++root_index) {
	    if (std::fabs(parameter - roots[root_index]) <= parameter_guard) {
		duplicate = true;
		break;
	    }
	}
	if (!duplicate)
	    roots.push_back(parameter);
    }
    if (roots.size() != 2)
	return false;
    std::sort(roots.begin(), roots.end());
    if (roots[1] - roots[0] <= parameter_guard)
	return false;

    ON_3dVector plane_normal = plane.Normal();
    if (!plane_normal.Unitize())
	return false;
    const double angular_tolerance = std::max(1.0e-10,
	isect_tol / std::max(1.0, section_curve->BoundingBox().Diagonal().Length()));
    sample.ruling_parameter = ruling_parameter;
    for (int branch = 0; branch < 2; ++branch) {
	const ON_2dPoint ruled_parameter = ruling_direction == 0 ?
	    ON_2dPoint(ruling_parameter, roots[branch]) :
	    ON_2dPoint(roots[branch], ruling_parameter);
	const ON_3dPoint model_point = ruled_surface.PointAt(
	    ruled_parameter.x, ruled_parameter.y);
	ON_2dPoint plane_parameter;
	ON_3dVector ruled_normal;
	if (!model_point.IsValid() ||
	    std::fabs(plane.DistanceTo(model_point)) > 2.0 * isect_tol ||
	    !plane_mapping.ParametersAt(model_point, plane_parameter) ||
	    !plane_parameter.IsValid() ||
	    !ruled_surface.EvNormal(ruled_parameter.x, ruled_parameter.y,
		ruled_normal) ||
	    !ruled_normal.Unitize() ||
	    ON_CrossProduct(plane_normal, ruled_normal).Length() <=
		angular_tolerance)
	{
	    return false;
	}

	sample.model_points[branch] = model_point;
	sample.plane_parameters[branch] = ON_3dPoint(plane_parameter.x,
	    plane_parameter.y, 0.0);
	sample.ruled_parameters[branch] = ON_3dPoint(ruled_parameter.x,
	    ruled_parameter.y, 0.0);
    }

    return sample.model_points[0].DistanceTo(sample.model_points[1]) >
	4.0 * isect_tol;
}


static bool
planar_ruled_sample_pairing_is_stable(
	const std::vector<PlanarRuledSectionSample> &samples,
	int curve_direction, const ON_Interval &curve_domain,
	double isect_tol)
{
    if (samples.size() < 2)
	return false;

    const double maximum_parameter_step = 0.45 * curve_domain.Length();
    for (size_t sample_index = 1; sample_index < samples.size();
	 ++sample_index)
    {
	const PlanarRuledSectionSample &previous = samples[sample_index - 1];
	const PlanarRuledSectionSample &current = samples[sample_index];
	const double direct_distance =
	    previous.model_points[0].DistanceTo(current.model_points[0]) +
	    previous.model_points[1].DistanceTo(current.model_points[1]);
	const double crossed_distance =
	    previous.model_points[0].DistanceTo(current.model_points[1]) +
	    previous.model_points[1].DistanceTo(current.model_points[0]);
	if (!std::isfinite(direct_distance) ||
	    !std::isfinite(crossed_distance) ||
	    direct_distance + 4.0 * isect_tol >= crossed_distance)
	{
	    return false;
	}

	for (int branch = 0; branch < 2; ++branch) {
	    if (std::fabs(current.ruled_parameters[branch][curve_direction] -
		    previous.ruled_parameters[branch][curve_direction]) >
		maximum_parameter_step)
	    {
		/* This is either a periodic seam crossing or an under-resolved
		 * pairing.  Both require a split that this conservative path does
		 * not attempt. */
		return false;
	    }
	}
    }
    return true;
}


static bool
make_planar_ruled_sample_curves(
	const std::vector<PlanarRuledSectionSample> &samples, int branch,
	std::unique_ptr<ON_Curve> &model_curve,
	std::unique_ptr<ON_Curve> &plane_curve,
	std::unique_ptr<ON_Curve> &ruled_curve)
{
    ON_3dPointArray model_points;
    ON_3dPointArray plane_parameters;
    ON_3dPointArray ruled_parameters;
    ON_SimpleArray<double> curve_parameters;
    for (size_t sample_index = 0; sample_index < samples.size();
	 ++sample_index)
    {
	model_points.Append(samples[sample_index].model_points[branch]);
	plane_parameters.Append(samples[sample_index].plane_parameters[branch]);
	ruled_parameters.Append(samples[sample_index].ruled_parameters[branch]);
	curve_parameters.Append(samples[sample_index].ruling_parameter);
    }

    std::unique_ptr<ON_PolylineCurve> model_polyline(
	new ON_PolylineCurve(model_points, curve_parameters));
    std::unique_ptr<ON_PolylineCurve> plane_polyline(
	new ON_PolylineCurve(plane_parameters, curve_parameters));
    std::unique_ptr<ON_PolylineCurve> ruled_polyline(
	new ON_PolylineCurve(ruled_parameters, curve_parameters));
    if (!model_polyline || !plane_polyline || !ruled_polyline ||
	!plane_polyline->ChangeDimension(2) ||
	!ruled_polyline->ChangeDimension(2) ||
	!model_polyline->IsValid() || !plane_polyline->IsValid() ||
	!ruled_polyline->IsValid() || model_polyline->IsClosed() ||
	plane_polyline->IsClosed() || ruled_polyline->IsClosed())
    {
	return false;
    }

    model_curve.reset(model_polyline.release());
    plane_curve.reset(plane_polyline.release());
    ruled_curve.reset(ruled_polyline.release());
    return true;
}


static bool
append_general_planar_ruled_ssi_events(
	const ON_Plane &plane, const AffinePlanarSurface &plane_mapping,
	const ON_Surface *plane_surface, const ON_Surface *ruled_surface,
	bool plane_is_surfaceA, int ruling_direction,
	const ON_Interval &ruling_domain, const ON_Interval &curve_domain,
	double coordinate_scale, double isect_tol,
	double ruled_parameter_tolerance,
	ON_ClassArray<ON_SSX_EVENT> &events)
{
    ON_PlaneSurface broad_plane_surface(plane);
    const double plane_extent = 4.0 * std::max(1.0, coordinate_scale);
    if (!broad_plane_surface.SetExtents(0,
	    ON_Interval(-plane_extent, plane_extent)) ||
	!broad_plane_surface.SetExtents(1,
	    ON_Interval(-plane_extent, plane_extent)))
    {
	return false;
    }

    for (int segment_count = 8; segment_count <= 256;
	 segment_count *= 2)
    {
	std::vector<PlanarRuledSectionSample> samples;
	samples.reserve(segment_count + 1);
	bool sampled = true;
	for (int sample_index = 0; sample_index <= segment_count;
	     ++sample_index)
	{
	    PlanarRuledSectionSample sample;
	    const double ruling_parameter = ruling_domain.ParameterAt(
		(double)sample_index / segment_count);
	    if (!sample_planar_ruled_section(plane, broad_plane_surface,
		    plane_mapping, *ruled_surface, ruling_direction,
		    curve_domain, ruling_parameter, isect_tol,
		    ruled_parameter_tolerance, sample))
	    {
		sampled = false;
		break;
	    }
	    samples.push_back(sample);
	}
	if (!sampled)
	    return false;
	if (!planar_ruled_sample_pairing_is_stable(samples,
		1 - ruling_direction, curve_domain, isect_tol))
	{
	    continue;
	}

	std::unique_ptr<ON_Curve> model_curves[2];
	std::unique_ptr<ON_Curve> plane_curves[2];
	std::unique_ptr<ON_Curve> ruled_curves[2];
	bool coherent = true;
	for (int branch = 0; branch < 2; ++branch) {
	    if (!make_planar_ruled_sample_curves(samples, branch,
		    model_curves[branch], plane_curves[branch],
		    ruled_curves[branch]) ||
		!planar_ruled_event_is_coherent(*model_curves[branch],
		    *plane_curves[branch], *ruled_curves[branch],
		    *plane_surface, *ruled_surface, isect_tol))
	    {
		coherent = false;
		break;
	    }
	}
	if (!coherent)
	    continue;

	for (int branch = 0; branch < 2; ++branch) {
	    ON_SSX_EVENT event;
	    ON_Curve *curveA = plane_is_surfaceA ?
		plane_curves[branch].release() : ruled_curves[branch].release();
	    ON_Curve *curveB = plane_is_surfaceA ?
		ruled_curves[branch].release() : plane_curves[branch].release();
	    set_ssx_event_from_curves(event, model_curves[branch].release(),
		curveA, curveB, plane_is_surfaceA ? plane_surface :
		ruled_surface, plane_is_surfaceA ? ruled_surface :
		plane_surface);
	    events.Append(event);
	    event.m_curve3d = NULL;
	    event.m_curveA = NULL;
	    event.m_curveB = NULL;
	}
	return true;
    }
    return false;
}


static bool
append_planar_ruled_ssi_event_ordered(
	const ON_Surface *plane_surface, const ON_Surface *ruled_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double ruled_parameter_tolerance,
	const ON_Interval *ruled_udomain, const ON_Interval *ruled_vdomain)
{
    ON_Plane plane;
    AffinePlanarSurface plane_mapping;
    const ON_NurbsSurface *ruled = ON_NurbsSurface::Cast(ruled_surface);
    const bool ruled_is_planar = ruled_surface->IsPlanar();
    const bool plane_is_planar = plane_surface->IsPlanar(&plane);
    const bool plane_is_affine = plane_is_planar &&
	affine_planar_surface_mapping(plane_surface, plane_mapping);
    if (ruled == NULL || ruled_is_planar || !plane_is_planar ||
	!plane_is_affine)
    {
	return false;
    }

    const ON_BoundingBox bounds = ruled_surface->BoundingBox();
    double coordinate_scale = 1.0;
    if (bounds.IsValid()) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.x));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.y));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.z));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.x));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.y));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.z));
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));

    const ON_Interval ruled_domains[2] = {
	ruled_udomain ? *ruled_udomain : ruled_surface->Domain(0),
	ruled_vdomain ? *ruled_vdomain : ruled_surface->Domain(1)
    };

    for (int ruling_direction = 0; ruling_direction < 2;
	 ++ruling_direction)
    {
	const int curve_direction = 1 - ruling_direction;
	const ON_Interval full_curve_domain = ruled_surface->Domain(
	    curve_direction);
	if (std::fabs(ruled_domains[curve_direction].Min() -
		full_curve_domain.Min()) > ruled_parameter_tolerance ||
	    std::fabs(ruled_domains[curve_direction].Max() -
		full_curve_domain.Max()) > ruled_parameter_tolerance)
	{
	    continue;
	}

	double ruling_parameter = 0.0;
	if (!plane_ruled_isocurve_parameter(plane, *ruled,
		ruling_direction, identity_tolerance, ruling_parameter) ||
	    !parameter_in_domain(ruling_parameter,
		ruled_domains[ruling_direction], ruled_parameter_tolerance))
	{
	    continue;
	}

	std::unique_ptr<ON_Curve> model_curve(
	    ruled_surface->IsoCurve(curve_direction, ruling_parameter));
	if (!model_curve || !model_curve->IsValid() ||
	    !model_curve->IsClosed() || model_curve->Dimension() != 3)
	{
	    continue;
	}
	std::unique_ptr<ON_Curve> plane_curve(
	    affine_planar_parameter_curve(*model_curve, plane, plane_mapping,
		identity_tolerance));
	if (!plane_curve)
	    continue;

	const ON_3dPoint ruled_start = curve_direction == 0 ?
	    ON_3dPoint(full_curve_domain.Min(), ruling_parameter, 0.0) :
	    ON_3dPoint(ruling_parameter, full_curve_domain.Min(), 0.0);
	const ON_3dPoint ruled_end = curve_direction == 0 ?
	    ON_3dPoint(full_curve_domain.Max(), ruling_parameter, 0.0) :
	    ON_3dPoint(ruling_parameter, full_curve_domain.Max(), 0.0);
	std::unique_ptr<ON_LineCurve> ruled_curve(
	    new ON_LineCurve(ruled_start, ruled_end));
	const ON_Interval model_domain = model_curve->Domain();
	const bool ruled_dimension_changed = ruled_curve &&
	    ruled_curve->ChangeDimension(2);
	const bool ruled_domain_set = ruled_dimension_changed &&
	    ruled_curve->SetDomain(model_domain.Min(), model_domain.Max());
	const bool ruled_valid = ruled_domain_set && ruled_curve->IsValid();
	const bool coherent = ruled_valid &&
	    planar_ruled_event_is_coherent(*model_curve, *plane_curve,
		*ruled_curve, *plane_surface, *ruled_surface, isect_tol);
	if (!coherent)
	{
	    continue;
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
	    ruled_curve.release();
	ON_Curve *curveB = plane_is_surfaceA ? ruled_curve.release() :
	    plane_curve.release();
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    plane_is_surfaceA ? plane_surface : ruled_surface,
	    plane_is_surfaceA ? ruled_surface : plane_surface);
	events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
	return true;
    }

    /* If the plane contains the ruling direction, each intersection of a
     * transverse section curve with the plane identifies a complete ruling
     * line on the intersection.  Generic SSI can mistake one of these lines
     * for a short closed retrace when the plane and ruled surface meet near a
     * patch boundary.  Certify the ruling analytically from both endpoint
     * curves before emitting it. */
    for (int ruling_direction = 0; ruling_direction < 2;
	 ++ruling_direction)
    {
	if (ruled->Degree(ruling_direction) != 1 ||
	    ruled->CVCount(ruling_direction) != 2)
	{
	    continue;
	}
	const int curve_direction = 1 - ruling_direction;
	if (!ruled_surface->IsClosed(curve_direction)) {
	    continue;
	}

	ON_Interval ruling_domain = ruled_domains[ruling_direction];
	ON_Interval curve_domain = ruled_domains[curve_direction];
	ruling_domain.MakeIncreasing();
	curve_domain.MakeIncreasing();
	if (!ruling_domain.IsIncreasing() || !curve_domain.IsIncreasing()) {
	    continue;
	}

	std::unique_ptr<ON_Curve> section_curve(ruled_surface->IsoCurve(
	    curve_direction, ruling_domain.Mid()));
	if (!section_curve || !section_curve->IsValid()) {
	    continue;
	}
	/* The finite planar face can occupy only a short interval of a long
	 * ruling.  Its parameter domain may therefore exclude the transverse
	 * section at the ruling midpoint even though the underlying plane cuts
	 * the ruled surface.  Find roots on a temporary, sufficiently broad copy
	 * of that geometric plane; the caller will clip the resulting complete
	 * ruling curves against both trimmed faces. */
	ON_PlaneSurface section_plane_surface(plane);
	const double section_plane_extent = 2.0 * coordinate_scale;
	if (!section_plane_surface.SetExtents(0, ON_Interval(
		-section_plane_extent, section_plane_extent)) ||
	    !section_plane_surface.SetExtents(1, ON_Interval(
		-section_plane_extent, section_plane_extent)))
	{
	    continue;
	}

	ON_SimpleArray<ON_X_EVENT> section_events;
	const int section_count = ON_Intersect(section_curve.get(),
	    &section_plane_surface,
	    section_events, isect_tol, 2.0 * isect_tol, &curve_domain,
	    NULL, NULL);
	if (section_count <= 0) {
	    continue;
	}

	std::vector<ON_3dPoint> accepted_points;
	const int original_event_count = events.Count();
	for (int section_index = 0; section_index < section_events.Count();
	     ++section_index)
	{
	    const ON_X_EVENT &section_event = section_events[section_index];
	    if (section_event.m_type != ON_X_EVENT::csx_point) {
		continue;
	    }
	    double curve_parameter = section_event.m_a[0];
	    if (!parameter_in_domain(curve_parameter, curve_domain,
		    ruled_parameter_tolerance))
	    {
		continue;
	    }
	    curve_parameter = std::max(curve_domain.Min(),
		std::min(curve_domain.Max(), curve_parameter));

	    const ON_2dPoint ruled_start = ruling_direction == 0 ?
		ON_2dPoint(ruling_domain.Min(), curve_parameter) :
		ON_2dPoint(curve_parameter, ruling_domain.Min());
	    const ON_2dPoint ruled_end = ruling_direction == 0 ?
		ON_2dPoint(ruling_domain.Max(), curve_parameter) :
		ON_2dPoint(curve_parameter, ruling_domain.Max());
	    const ON_3dPoint model_start = ruled_surface->PointAt(
		ruled_start.x, ruled_start.y);
	    const ON_3dPoint model_end = ruled_surface->PointAt(
		ruled_end.x, ruled_end.y);
	    if (!model_start.IsValid() || !model_end.IsValid() ||
		model_start.DistanceTo(model_end) <= identity_tolerance ||
		std::fabs(plane.DistanceTo(model_start)) > identity_tolerance ||
		std::fabs(plane.DistanceTo(model_end)) > identity_tolerance)
	    {
		continue;
	    }

	    bool duplicate = false;
	    for (size_t accepted_index = 0;
		 accepted_index < accepted_points.size(); ++accepted_index)
	    {
		if (model_start.DistanceTo(accepted_points[accepted_index]) <=
		    identity_tolerance)
		{
		    duplicate = true;
		    break;
		}
	    }
	    if (duplicate) {
		continue;
	    }

	    std::unique_ptr<ON_Curve> model_curve(ruled_surface->IsoCurve(
		ruling_direction, curve_parameter));
	    if (!model_curve || !model_curve->IsValid() ||
		model_curve->Dimension() != 3)
	    {
		continue;
	    }
	    ON_Interval model_domain = model_curve->Domain();
	    model_domain.MakeIncreasing();
	    if (!model_domain.IsIncreasing() ||
		(std::fabs(model_domain.Min() - ruling_domain.Min()) >
		    ruled_parameter_tolerance ||
		 std::fabs(model_domain.Max() - ruling_domain.Max()) >
		    ruled_parameter_tolerance) &&
		!model_curve->Trim(ruling_domain))
	    {
		continue;
	    }
	    model_domain = model_curve->Domain();

	    ON_2dPoint plane_start;
	    ON_2dPoint plane_end;
	    if (!plane_mapping.ParametersAt(model_start, plane_start) ||
		!plane_mapping.ParametersAt(model_end, plane_end))
	    {
		continue;
	    }

	    std::unique_ptr<ON_LineCurve> plane_curve(
		new ON_LineCurve(plane_start, plane_end));
	    std::unique_ptr<ON_LineCurve> ruled_curve(
		new ON_LineCurve(ruled_start, ruled_end));
	    if (!plane_curve || !ruled_curve ||
		!plane_curve->ChangeDimension(2) ||
		!ruled_curve->ChangeDimension(2) ||
		!plane_curve->SetDomain(model_domain.Min(), model_domain.Max()) ||
		!ruled_curve->SetDomain(model_domain.Min(), model_domain.Max()) ||
		!plane_curve->IsValid() || !ruled_curve->IsValid() ||
		!planar_ruled_event_is_coherent(*model_curve, *plane_curve,
		    *ruled_curve, *plane_surface, *ruled_surface, isect_tol))
	    {
		continue;
	    }

	    ON_SSX_EVENT event;
	    ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
		ruled_curve.release();
	    ON_Curve *curveB = plane_is_surfaceA ? ruled_curve.release() :
		plane_curve.release();
	    set_ssx_event_from_curves(event, model_curve.release(), curveA,
		curveB, plane_is_surfaceA ? plane_surface : ruled_surface,
		plane_is_surfaceA ? ruled_surface : plane_surface);
	    events.Append(event);
	    event.m_curve3d = NULL;
	    event.m_curveA = NULL;
	    event.m_curveB = NULL;
	    accepted_points.push_back(model_start);
	}
	if (events.Count() > original_event_count) {
	    return true;
	}
    }

    /* For an oblique plane, the intersection need not contain any complete
     * ruling.  A closed transverse section of a degree-one ruled surface can
     * nevertheless provide two continuously paired roots at every ruling
     * parameter.  Emit those two branches only when the root count, pairing,
     * and synchronized model/UV polylines remain unambiguous under adaptive
     * refinement. */
    for (int ruling_direction = 0; ruling_direction < 2;
	 ++ruling_direction)
    {
	if (ruled->Degree(ruling_direction) != 1 ||
	    ruled->CVCount(ruling_direction) != 2)
	{
	    continue;
	}
	const int curve_direction = 1 - ruling_direction;
	const ON_Interval full_curve_domain = ruled_surface->Domain(
	    curve_direction);
	if (!ruled_surface->IsClosed(curve_direction) ||
	    std::fabs(ruled_domains[curve_direction].Min() -
		full_curve_domain.Min()) > ruled_parameter_tolerance ||
	    std::fabs(ruled_domains[curve_direction].Max() -
		full_curve_domain.Max()) > ruled_parameter_tolerance)
	{
	    continue;
	}

	ON_Interval ruling_domain = ruled_domains[ruling_direction];
	ON_Interval curve_domain = full_curve_domain;
	ruling_domain.MakeIncreasing();
	curve_domain.MakeIncreasing();
	if (!ruling_domain.IsIncreasing() || !curve_domain.IsIncreasing())
	    continue;
	if (append_general_planar_ruled_ssi_events(plane, plane_mapping,
		plane_surface, ruled_surface, plane_is_surfaceA,
		ruling_direction, ruling_domain, curve_domain, coordinate_scale,
		isect_tol, ruled_parameter_tolerance, events))
	{
	    return true;
	}
    }
    return false;
}


static bool
append_planar_ruled_ssi_event(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_planar_ruled_ssi_event_ordered(surfA, surfB, true,
	events, isect_tol, isect_tolB, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_planar_ruled_ssi_event_ordered(surfB, surfA, false,
	    events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain);
}


static bool
append_planar_revolution_ssi_event_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Plane plane;
    AffinePlanarSurface plane_mapping;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(revolution_surface);
    const ON_LineCurve *profile = revolution && revolution->m_curve ?
	ON_LineCurve::Cast(revolution->m_curve) : NULL;
    if (revolution == NULL || profile == NULL ||
	!plane_surface->IsPlanar(&plane) ||
	!affine_planar_surface_mapping(plane_surface, plane_mapping))
    {
	return false;
    }

    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
    const int rotation_direction = 1 - profile_direction;
    if (!revolution_surface->IsClosed(rotation_direction))
	return false;

    ON_3dVector axis_direction = revolution->m_axis.Direction();
    ON_3dVector plane_normal = plane.Normal();
    const ON_3dPoint profile_start = profile->PointAtStart();
    const ON_3dPoint profile_end = profile->PointAtEnd();
    const double profile_length = profile_start.DistanceTo(profile_end);
    if (!axis_direction.Unitize() || !plane_normal.Unitize() ||
	!profile_start.IsValid() || !profile_end.IsValid() ||
	!(profile_length > ON_ZERO_TOLERANCE))
    {
	return false;
    }

    const ON_BoundingBox bounds = revolution_surface->BoundingBox();
    double coordinate_scale = 1.0;
    if (bounds.IsValid()) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.x));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.y));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min.z));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.x));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.y));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max.z));
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	identity_tolerance / std::max(1.0, profile_length));
    if (ON_CrossProduct(axis_direction, plane_normal).Length() >
	angular_tolerance)
    {
	return false;
    }

    const double start_distance = plane.DistanceTo(profile_start);
    const double end_distance = plane.DistanceTo(profile_end);
    const double distance_change = end_distance - start_distance;
    if (!std::isfinite(start_distance) || !std::isfinite(end_distance) ||
	std::fabs(distance_change) <= identity_tolerance)
    {
	return false;
    }
    double profile_fraction = -start_distance / distance_change;
    const double fraction_tolerance = std::max(64.0 * ON_EPSILON,
	identity_tolerance / std::fabs(distance_change));
    if (!std::isfinite(profile_fraction) ||
	profile_fraction < -fraction_tolerance ||
	profile_fraction > 1.0 + fraction_tolerance)
    {
	return false;
    }
    profile_fraction = std::max(0.0, std::min(1.0, profile_fraction));

    const ON_Interval revolution_domains[2] = {
	revolution_udomain ? *revolution_udomain :
	    revolution_surface->Domain(0),
	revolution_vdomain ? *revolution_vdomain :
	    revolution_surface->Domain(1)
    };
    const ON_Interval full_rotation_domain = revolution_surface->Domain(
	rotation_direction);
    const ON_Interval full_profile_domain = revolution_surface->Domain(
	profile_direction);
    if (std::fabs(revolution_domains[rotation_direction].Min() -
	    full_rotation_domain.Min()) > revolution_parameter_tolerance ||
	std::fabs(revolution_domains[rotation_direction].Max() -
	    full_rotation_domain.Max()) > revolution_parameter_tolerance ||
	std::fabs(profile->Domain().Min() - full_profile_domain.Min()) >
	    revolution_parameter_tolerance ||
	std::fabs(profile->Domain().Max() - full_profile_domain.Max()) >
	    revolution_parameter_tolerance)
    {
	return false;
    }

    const double profile_parameter = profile->Domain().ParameterAt(
	profile_fraction);
    if (!parameter_in_domain(profile_parameter,
	    revolution_domains[profile_direction],
	    revolution_parameter_tolerance))
    {
	return false;
    }

    std::unique_ptr<ON_Curve> model_curve(revolution_surface->IsoCurve(
	rotation_direction, profile_parameter));
    if (!model_curve || !model_curve->IsValid() ||
	!model_curve->IsClosed() || model_curve->Dimension() != 3)
    {
	return false;
    }
    const ON_Interval model_domain = model_curve->Domain();
    if (std::fabs(model_domain.Min() - full_rotation_domain.Min()) >
	    revolution_parameter_tolerance ||
	std::fabs(model_domain.Max() - full_rotation_domain.Max()) >
	    revolution_parameter_tolerance)
    {
	return false;
    }
    for (int sample_index = 0; sample_index <= 16; ++sample_index) {
	const ON_3dPoint point = model_curve->PointAt(model_domain.ParameterAt(
	    (double)sample_index / 16.0));
	if (!point.IsValid() ||
	    std::fabs(plane.DistanceTo(point)) > identity_tolerance)
	{
	    return false;
	}
    }

    std::unique_ptr<ON_Curve> plane_curve(
	affine_planar_parameter_curve(*model_curve, plane, plane_mapping,
	    identity_tolerance));
    if (!plane_curve)
	return false;

    const ON_3dPoint revolution_start = rotation_direction == 0 ?
	ON_3dPoint(full_rotation_domain.Min(), profile_parameter, 0.0) :
	ON_3dPoint(profile_parameter, full_rotation_domain.Min(), 0.0);
    const ON_3dPoint revolution_end = rotation_direction == 0 ?
	ON_3dPoint(full_rotation_domain.Max(), profile_parameter, 0.0) :
	ON_3dPoint(profile_parameter, full_rotation_domain.Max(), 0.0);
    std::unique_ptr<ON_LineCurve> revolution_curve(
	new ON_LineCurve(revolution_start, revolution_end));
    if (!revolution_curve || !revolution_curve->ChangeDimension(2) ||
	!revolution_curve->SetDomain(model_domain.Min(), model_domain.Max()) ||
	!revolution_curve->IsValid() ||
	!planar_ruled_event_is_coherent(*model_curve, *plane_curve,
	    *revolution_curve, *plane_surface, *revolution_surface, isect_tol))
    {
	return false;
    }

    ON_SSX_EVENT event;
    ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
	revolution_curve.release();
    ON_Curve *curveB = plane_is_surfaceA ? revolution_curve.release() :
	plane_curve.release();
    set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	plane_is_surfaceA ? plane_surface : revolution_surface,
	plane_is_surfaceA ? revolution_surface : plane_surface);
    events.Append(event);
    event.m_curve3d = NULL;
    event.m_curveA = NULL;
    event.m_curveB = NULL;
    return true;
}


/* A non-linear revolution profile can cross a plane normal to the revolution
 * axis more than once.  Each profile/plane point intersection sweeps out one
 * complete rotation isocurve.  Construct those curves directly instead of
 * asking general SSI to refine the plane against a two-dimensional tangent
 * neighborhood of every resulting circle. */
static bool
append_general_planar_revolution_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Plane plane;
    AffinePlanarSurface plane_mapping;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(revolution_surface);
    const ON_Curve *profile = revolution ? revolution->m_curve : NULL;
    if (revolution == NULL || profile == NULL ||
	ON_LineCurve::Cast(profile) != NULL ||
	!plane_surface->IsPlanar(&plane) ||
	!affine_planar_surface_mapping(plane_surface, plane_mapping))
    {
	return false;
    }

    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
    const int rotation_direction = 1 - profile_direction;
    if (!revolution_surface->IsClosed(rotation_direction))
	return false;

    ON_3dVector axis_direction = revolution->m_axis.Direction();
    ON_3dVector plane_normal = plane.Normal();
    const ON_BoundingBox bounds = revolution_surface->BoundingBox();
    const ON_BoundingBox profile_bounds = profile->BoundingBox();
    if (!axis_direction.Unitize() || !plane_normal.Unitize() ||
	!bounds.IsValid() || !profile_bounds.IsValid())
    {
	return false;
    }

    double coordinate_scale = 1.0;
    for (int axis = 0; axis < 3; ++axis) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[axis]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[axis]));
    }
    const double profile_scale = profile_bounds.Diagonal().Length();
    if (!(profile_scale > ON_ZERO_TOLERANCE) ||
	!std::isfinite(profile_scale))
    {
	return false;
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	identity_tolerance / std::max(1.0, profile_scale));
    if (ON_CrossProduct(axis_direction, plane_normal).Length() >
	angular_tolerance)
    {
	return false;
    }

    ON_Interval revolution_domains[2] = {
	revolution_udomain ? *revolution_udomain :
	    revolution_surface->Domain(0),
	revolution_vdomain ? *revolution_vdomain :
	    revolution_surface->Domain(1)
    };
    for (int direction = 0; direction < 2; ++direction)
	revolution_domains[direction].MakeIncreasing();
    ON_Interval full_rotation_domain = revolution_surface->Domain(
	rotation_direction);
    ON_Interval full_profile_domain = revolution_surface->Domain(
	profile_direction);
    full_rotation_domain.MakeIncreasing();
    full_profile_domain.MakeIncreasing();
    ON_Interval profile_domain = profile->Domain();
    profile_domain.MakeIncreasing();
    if (!revolution_domains[0].IsIncreasing() ||
	!revolution_domains[1].IsIncreasing() ||
	!full_rotation_domain.IsIncreasing() ||
	!full_profile_domain.IsIncreasing() || !profile_domain.IsIncreasing() ||
	std::fabs(revolution_domains[rotation_direction].Min() -
	    full_rotation_domain.Min()) > revolution_parameter_tolerance ||
	std::fabs(revolution_domains[rotation_direction].Max() -
	    full_rotation_domain.Max()) > revolution_parameter_tolerance ||
	std::fabs(profile_domain.Min() - full_profile_domain.Min()) >
	    revolution_parameter_tolerance ||
	std::fabs(profile_domain.Max() - full_profile_domain.Max()) >
	    revolution_parameter_tolerance)
    {
	return false;
    }

    /* Use a broad temporary plane so profile roots are not lost merely
     * because the finite planar face occupies a different part of the same
     * support plane.  Face trimming later clips the complete SSI circles. */
    ON_PlaneSurface section_plane_surface(plane);
    const double section_plane_extent = 2.0 * coordinate_scale;
    if (!section_plane_surface.SetExtents(0, ON_Interval(
	    -section_plane_extent, section_plane_extent)) ||
	!section_plane_surface.SetExtents(1, ON_Interval(
	    -section_plane_extent, section_plane_extent)))
    {
	return false;
    }

    ON_SimpleArray<ON_X_EVENT> section_events;
    const int section_count = ON_Intersect(profile, &section_plane_surface,
	section_events, isect_tol, 2.0 * isect_tol,
	&revolution_domains[profile_direction], NULL, NULL);
    if (section_count <= 0) {
	bu_log("transverse-plane revolution SSI found %d profile roots\n",
	    section_count);
	return false;
    }

    std::vector<double> profile_parameters;
    std::vector<ON_3dPoint> profile_points;
    for (int event_index = 0; event_index < section_events.Count();
	 ++event_index)
    {
	const ON_X_EVENT &event = section_events[event_index];
	if (event.m_type != ON_X_EVENT::csx_point) {
	    bu_log("transverse-plane revolution SSI rejected root type %d\n",
		(int)event.m_type);
	    return false;
	}
	double parameter = event.m_a[0];
	if (!parameter_in_domain(parameter,
		revolution_domains[profile_direction],
		revolution_parameter_tolerance))
	{
	    continue;
	}
	parameter = std::max(revolution_domains[profile_direction].Min(),
	    std::min(revolution_domains[profile_direction].Max(), parameter));
	const ON_3dPoint point = profile->PointAt(parameter);
	if (!point.IsValid() ||
	    std::fabs(plane.DistanceTo(point)) > identity_tolerance)
	{
	    return false;
	}

	bool duplicate = false;
	for (size_t point_index = 0; point_index < profile_points.size();
	     ++point_index)
	{
	    if (point.DistanceTo(profile_points[point_index]) <=
		identity_tolerance)
	    {
		duplicate = true;
		break;
	    }
	}
	if (!duplicate) {
	    profile_parameters.push_back(parameter);
	    profile_points.push_back(point);
	}
    }
    if (profile_parameters.empty()) {
	bu_log("transverse-plane revolution SSI retained no profile roots\n");
	return false;
    }

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (size_t parameter_index = 0;
	 parameter_index < profile_parameters.size(); ++parameter_index)
    {
	const double profile_parameter = profile_parameters[parameter_index];
	std::unique_ptr<ON_Curve> model_curve(revolution_surface->IsoCurve(
	    rotation_direction, profile_parameter));
	if (!model_curve || !model_curve->IsValid() ||
	    !model_curve->IsClosed() || model_curve->Dimension() != 3)
	{
	    bu_log("transverse-plane revolution SSI rejected model curve\n");
	    return false;
	}
	const ON_Interval model_domain = model_curve->Domain();
	if (std::fabs(model_domain.Min() - full_rotation_domain.Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(model_domain.Max() - full_rotation_domain.Max()) >
		revolution_parameter_tolerance)
	{
	    return false;
	}
	for (int sample_index = 0; sample_index <= 16; ++sample_index) {
	    const ON_3dPoint point = model_curve->PointAt(
		model_domain.ParameterAt((double)sample_index / 16.0));
	    if (!point.IsValid() ||
		std::fabs(plane.DistanceTo(point)) > identity_tolerance)
	    {
		return false;
	    }
	}

	std::unique_ptr<ON_Curve> plane_curve(
	    affine_planar_parameter_curve(*model_curve, plane, plane_mapping,
		identity_tolerance));
	if (!plane_curve)
	{
	    bu_log("transverse-plane revolution SSI rejected plane curve\n");
	    return false;
	}

	const ON_3dPoint revolution_start = rotation_direction == 0 ?
	    ON_3dPoint(full_rotation_domain.Min(), profile_parameter, 0.0) :
	    ON_3dPoint(profile_parameter, full_rotation_domain.Min(), 0.0);
	const ON_3dPoint revolution_end = rotation_direction == 0 ?
	    ON_3dPoint(full_rotation_domain.Max(), profile_parameter, 0.0) :
	    ON_3dPoint(profile_parameter, full_rotation_domain.Max(), 0.0);
	std::unique_ptr<ON_LineCurve> revolution_curve(
	    new ON_LineCurve(revolution_start, revolution_end));
	if (!revolution_curve || !revolution_curve->ChangeDimension(2) ||
	    !revolution_curve->SetDomain(model_domain.Min(), model_domain.Max()) ||
	    !revolution_curve->IsValid())
	{
	    bu_log("transverse-plane revolution SSI rejected coherence\n");
	    return false;
	}
	for (int sample_index = 0; sample_index <= 32; ++sample_index) {
	    const double parameter = model_domain.ParameterAt(
		(double)sample_index / 32.0);
	    const ON_3dPoint model_point = model_curve->PointAt(parameter);
	    const ON_3dPoint uv = revolution_curve->PointAt(parameter);
	    if (!model_point.IsValid() || !uv.IsValid() ||
		revolution_surface->PointAt(uv.x, uv.y).DistanceTo(model_point) >
		    isect_tol)
	    {
		bu_log("transverse-plane revolution SSI rejected revolution "
		    "parameter curve\n");
		return false;
	    }
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
	    revolution_curve.release();
	ON_Curve *curveB = plane_is_surfaceA ? revolution_curve.release() :
	    plane_curve.release();
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    plane_is_surfaceA ? plane_surface : revolution_surface,
	    plane_is_surfaceA ? revolution_surface : plane_surface);
	generated_events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    bu_log("transverse-plane revolution SSI generated %d circles\n",
	generated_events.Count());
    return generated_events.Count() > 0;
}


static bool
append_axis_planar_revolution_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain);


static bool
append_parallel_planar_spheroid_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain);


static bool
append_parallel_planar_torus_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain);


static bool
append_planar_revolution_ssi_event(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_planar_revolution_ssi_event_ordered(surfA, surfB, true,
	events, isect_tol, isect_tolB, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_general_planar_revolution_ssi_events_ordered(surfA, surfB, true,
	    events, isect_tol, isect_tolB, surfaceB_udomain,
	    surfaceB_vdomain) ||
	append_parallel_planar_spheroid_ssi_events_ordered(surfA, surfB,
	    true, events, isect_tol, isect_tolB, surfaceB_udomain,
	    surfaceB_vdomain) ||
	append_parallel_planar_torus_ssi_events_ordered(surfA, surfB, true,
	    events, isect_tol, isect_tolB, surfaceB_udomain,
	    surfaceB_vdomain) ||
	append_axis_planar_revolution_ssi_events_ordered(surfA, surfB, true,
	    events, isect_tol, isect_tolB, surfaceB_udomain,
	    surfaceB_vdomain) ||
	append_planar_revolution_ssi_event_ordered(surfB, surfA, false,
	    events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain) ||
	append_general_planar_revolution_ssi_events_ordered(surfB, surfA,
	    false, events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain) ||
	append_parallel_planar_spheroid_ssi_events_ordered(surfB, surfA,
	    false, events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain) ||
	append_parallel_planar_torus_ssi_events_ordered(surfB, surfA, false,
	    events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain) ||
	append_axis_planar_revolution_ssi_events_ordered(surfB, surfA, false,
	    events, isect_tol, isect_tolA, surfaceA_udomain,
	    surfaceA_vdomain);
}


static bool
revolution_meridian_frame(const ON_RevSurface &revolution,
	double identity_tolerance, ON_3dVector &axis_direction,
	ON_3dVector &radial_direction)
{
    if (revolution.m_curve == NULL)
	return false;
    axis_direction = revolution.m_axis.Direction();
    if (!axis_direction.Unitize())
	return false;

    ON_NurbsCurve profile;
    if (revolution.m_curve->GetNurbForm(profile) <= 0 ||
	!profile.IsValid())
    {
	return false;
    }

    std::vector<ON_3dPoint> controls;
    controls.reserve(profile.CVCount());
    double maximum_radius = 0.0;
    for (int control_index = 0; control_index < profile.CVCount();
	 ++control_index)
    {
	ON_3dPoint control;
	if (profile.IsRational()) {
	    ON_4dPoint homogeneous;
	    if (!profile.GetCV(control_index, homogeneous) ||
		!std::isfinite(homogeneous.w) ||
		homogeneous.w <= ON_ZERO_TOLERANCE)
	    {
		return false;
	    }
	    control = ON_3dPoint(homogeneous.x / homogeneous.w,
		homogeneous.y / homogeneous.w,
		homogeneous.z / homogeneous.w);
	} else if (!profile.GetCV(control_index, control)) {
	    return false;
	}
	if (!control.IsValid())
	    return false;
	controls.push_back(control);
	const ON_3dVector offset = control - revolution.m_axis.from;
	const ON_3dVector radial = offset - axis_direction *
	    ON_DotProduct(offset, axis_direction);
	if (radial.Length() > maximum_radius) {
	    maximum_radius = radial.Length();
	    radial_direction = radial;
	}
    }
    if (!radial_direction.Unitize())
	return false;

    ON_3dVector meridian_normal = ON_CrossProduct(axis_direction,
	radial_direction);
    if (!meridian_normal.Unitize())
	return false;
    for (const ON_3dPoint &control : controls) {
	const ON_3dVector offset = control - revolution.m_axis.from;
	if (std::fabs(ON_DotProduct(offset, meridian_normal)) >
		identity_tolerance)
	{
	    return false;
	}
    }
    return true;
}


struct SpheroidRevolutionGeometry {
    const ON_RevSurface *revolution;
    const ON_Curve *profile;
    ON_NurbsCurve profile_nurbs;
    int profile_nurbs_form;
    int profile_direction;
    int rotation_direction;
    ON_Interval domains[2];
    ON_Interval full_domains[2];
    ON_Interval angle_domain;
    ON_3dPoint center;
    ON_3dVector axis_direction;
    ON_3dVector radial_direction;
    ON_3dVector azimuthal_direction;
    double radial_radius;
    double axial_radius;
    double identity_tolerance;
};


static bool
spheroid_revolution_geometry(const ON_Surface *surface,
	double intersection_tolerance, double parameter_tolerance,
	const ON_Interval *udomain, const ON_Interval *vdomain,
	SpheroidRevolutionGeometry &geometry)
{
    const ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    const ON_Curve *profile = revolution ? revolution->m_curve : NULL;
    if (revolution == NULL || profile == NULL ||
	!surface->IsClosed(revolution->m_bTransposed ? 1 : 0))
    {
	return false;
    }

    geometry.revolution = revolution;
    geometry.profile = profile;
    geometry.profile_direction = revolution->m_bTransposed ? 0 : 1;
    geometry.rotation_direction = 1 - geometry.profile_direction;
    geometry.domains[0] = udomain ? *udomain : surface->Domain(0);
    geometry.domains[1] = vdomain ? *vdomain : surface->Domain(1);
    geometry.full_domains[0] = surface->Domain(0);
    geometry.full_domains[1] = surface->Domain(1);
    for (int direction = 0; direction < 2; ++direction) {
	geometry.domains[direction].MakeIncreasing();
	geometry.full_domains[direction].MakeIncreasing();
	if (!geometry.domains[direction].IsIncreasing() ||
	    !geometry.full_domains[direction].IsIncreasing() ||
	    std::fabs(geometry.domains[direction].Min() -
		geometry.full_domains[direction].Min()) >
		parameter_tolerance ||
	    std::fabs(geometry.domains[direction].Max() -
		geometry.full_domains[direction].Max()) >
		parameter_tolerance)
	{
	    return false;
	}
    }

    geometry.angle_domain = ON_Interval(revolution->m_angle);
    geometry.angle_domain.MakeIncreasing();
    if (!geometry.angle_domain.IsIncreasing() ||
	std::fabs(geometry.angle_domain.Length() - 2.0 * ON_PI) >
	    std::max(parameter_tolerance, 1024.0 * ON_EPSILON))
    {
	return false;
    }

    const ON_BoundingBox bounds = surface->BoundingBox();
    if (!bounds.IsValid())
	return false;
    double coordinate_scale = 1.0;
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[coordinate]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[coordinate]));
    }
    geometry.identity_tolerance = std::min(intersection_tolerance,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    if (!(geometry.identity_tolerance > 0.0) ||
	!revolution_meridian_frame(*revolution,
	    geometry.identity_tolerance, geometry.axis_direction,
	    geometry.radial_direction))
    {
	return false;
    }
    geometry.azimuthal_direction = ON_CrossProduct(
	geometry.axis_direction, geometry.radial_direction);
    if (!geometry.azimuthal_direction.Unitize())
	return false;

    ON_Interval profile_domain = profile->Domain();
    profile_domain.MakeIncreasing();
    if (!profile_domain.IsIncreasing() ||
	std::fabs(profile_domain.Min() -
	    geometry.full_domains[geometry.profile_direction].Min()) >
	    parameter_tolerance ||
	std::fabs(profile_domain.Max() -
	    geometry.full_domains[geometry.profile_direction].Max()) >
	    parameter_tolerance)
    {
	return false;
    }

    const ON_3dPoint first = profile->PointAt(profile_domain.Min());
    const ON_3dPoint middle = profile->PointAt(profile_domain.Mid());
    const ON_3dPoint last = profile->PointAt(profile_domain.Max());
    if (!first.IsValid() || !middle.IsValid() || !last.IsValid())
	return false;
    const ON_3dVector first_offset = first - revolution->m_axis.from;
    const ON_3dVector middle_offset = middle - revolution->m_axis.from;
    const ON_3dVector last_offset = last - revolution->m_axis.from;
    const double first_axis = ON_DotProduct(first_offset,
	geometry.axis_direction);
    const double middle_axis = ON_DotProduct(middle_offset,
	geometry.axis_direction);
    const double last_axis = ON_DotProduct(last_offset,
	geometry.axis_direction);
    const double center_axis = 0.5 * (first_axis + last_axis);
    const ON_3dVector first_radial = first_offset -
	first_axis * geometry.axis_direction;
    const ON_3dVector middle_radial = middle_offset -
	middle_axis * geometry.axis_direction;
    const ON_3dVector last_radial = last_offset -
	last_axis * geometry.axis_direction;
    geometry.radial_radius = ON_DotProduct(middle_radial,
	geometry.radial_direction);
    geometry.axial_radius = 0.5 * std::fabs(last_axis - first_axis);
    geometry.center = revolution->m_axis.from +
	center_axis * geometry.axis_direction;
    if (!(geometry.radial_radius > geometry.identity_tolerance) ||
	!(geometry.axial_radius > geometry.identity_tolerance) ||
	first_radial.Length() > geometry.identity_tolerance ||
	last_radial.Length() > geometry.identity_tolerance ||
	std::fabs(middle_axis - center_axis) >
	    geometry.identity_tolerance)
    {
	return false;
    }

    /* Certify that the complete meridian is the positive-radius half of
     * one axis-aligned ellipse.  This deliberately recognizes only the
     * spheroid representation; arbitrary surfaces of revolution continue
     * through the general SSI implementation. */
    static const int profile_sample_count = 32;
    const double radius_scale = std::max(geometry.radial_radius,
	geometry.axial_radius);
    for (int sample_index = 0; sample_index <= profile_sample_count;
	 ++sample_index)
    {
	const ON_3dPoint point = profile->PointAt(profile_domain.ParameterAt(
	    (double)sample_index / profile_sample_count));
	if (!point.IsValid())
	    return false;
	const ON_3dVector offset = point - geometry.center;
	const double axis_coordinate = ON_DotProduct(offset,
	    geometry.axis_direction);
	const double radial_coordinate = ON_DotProduct(offset,
	    geometry.radial_direction);
	const double off_meridian = std::fabs(ON_DotProduct(offset,
	    geometry.azimuthal_direction));
	if (radial_coordinate < -geometry.identity_tolerance ||
	    off_meridian > geometry.identity_tolerance)
	{
	    return false;
	}
	const double normalized_radius = std::sqrt(
	    (radial_coordinate * radial_coordinate) /
		(geometry.radial_radius * geometry.radial_radius) +
	    (axis_coordinate * axis_coordinate) /
		(geometry.axial_radius * geometry.axial_radius));
	if (!std::isfinite(normalized_radius) ||
	    std::fabs(normalized_radius - 1.0) * radius_scale >
		8.0 * geometry.identity_tolerance)
	{
	    return false;
	}
    }

    geometry.profile_nurbs_form = profile->GetNurbForm(
	geometry.profile_nurbs);
    return (geometry.profile_nurbs_form == 1 ||
	geometry.profile_nurbs_form == 2) &&
	geometry.profile_nurbs.IsValid();
}


static bool
spheroid_surface_parameter(const SpheroidRevolutionGeometry &geometry,
	const ON_3dPoint &model_point, double rotation_seed,
	double tolerance, ON_2dPoint &parameter)
{
    if (!model_point.IsValid() || !std::isfinite(rotation_seed))
	return false;
    const ON_3dVector axis_offset = model_point -
	geometry.revolution->m_axis.from;
    const double axis_coordinate = ON_DotProduct(axis_offset,
	geometry.axis_direction);
    const ON_3dVector radial = axis_offset -
	axis_coordinate * geometry.axis_direction;
    const double radial_length = radial.Length();
    if (!(radial_length > geometry.identity_tolerance))
	return false;

    double angle = std::atan2(ON_DotProduct(radial,
	geometry.azimuthal_direction), ON_DotProduct(radial,
	geometry.radial_direction));
    while (angle < geometry.angle_domain.Min())
	angle += 2.0 * ON_PI;
    while (angle > geometry.angle_domain.Max())
	angle -= 2.0 * ON_PI;
    double rotation_parameter = geometry.revolution->m_t.ParameterAt(
	geometry.angle_domain.NormalizedParameterAt(angle));
    const double rotation_period =
	geometry.full_domains[geometry.rotation_direction].Length();
    if (!(rotation_period > ON_ZERO_TOLERANCE) ||
	!std::isfinite(rotation_parameter))
    {
	return false;
    }
    rotation_parameter += std::round((rotation_seed -
	rotation_parameter) / rotation_period) * rotation_period;

    const ON_3dPoint profile_point = geometry.revolution->m_axis.from +
	axis_coordinate * geometry.axis_direction +
	radial_length * geometry.radial_direction;
    double nurbs_parameter = geometry.profile_nurbs.Domain().Mid();
    const double projection_tolerance = std::max(tolerance,
	8.0 * geometry.identity_tolerance);
    if (!ON_NurbsCurve_GetClosestPoint(&nurbs_parameter,
	    &geometry.profile_nurbs, profile_point, projection_tolerance,
	    NULL))
    {
	return false;
    }
    double profile_parameter = nurbs_parameter;
    if (geometry.profile_nurbs_form == 2 &&
	!geometry.profile->GetCurveParameterFromNurbFormParameter(
	    nurbs_parameter, &profile_parameter))
    {
	return false;
    }
    if (!parameter_in_domain(profile_parameter,
	    geometry.full_domains[geometry.profile_direction],
	    8.0 * ON_EPSILON * std::max(1.0,
		geometry.full_domains[geometry.profile_direction].Length())))
    {
	return false;
    }

    if (geometry.rotation_direction == 0) {
	parameter = ON_2dPoint(rotation_parameter, profile_parameter);
    } else {
	parameter = ON_2dPoint(profile_parameter, rotation_parameter);
    }
    const ON_3dPoint lifted = geometry.revolution->PointAt(parameter.x,
	parameter.y);
    return lifted.IsValid() &&
	lifted.DistanceTo(model_point) <= tolerance;
}


static bool
append_spheroid_parameter_beziers(
	const SpheroidRevolutionGeometry &geometry,
	const ON_Curve &model_curve, double first_parameter,
	double second_parameter, const ON_2dPoint &first_surface_parameter,
	const ON_2dPoint &second_surface_parameter, double tolerance,
	int depth, size_t &segment_count,
	std::vector<std::unique_ptr<ON_Curve> > &segments)
{
    static const int maximum_depth = 20;
    static const size_t maximum_segment_count = 16384;
    const double interval = second_parameter - first_parameter;
    if (!(interval > ON_ZERO_TOLERANCE) || depth > maximum_depth ||
	segment_count >= maximum_segment_count)
    {
	return false;
    }

    const double one_third_parameter = first_parameter + interval / 3.0;
    const double two_thirds_parameter = first_parameter +
	2.0 * interval / 3.0;
    ON_2dPoint one_third_surface_parameter;
    ON_2dPoint two_thirds_surface_parameter;
    const double rotation_one_third = (2.0 / 3.0) *
	first_surface_parameter[geometry.rotation_direction] + (1.0 / 3.0) *
	second_surface_parameter[geometry.rotation_direction];
    const double rotation_two_thirds = (1.0 / 3.0) *
	first_surface_parameter[geometry.rotation_direction] + (2.0 / 3.0) *
	second_surface_parameter[geometry.rotation_direction];
    if (!spheroid_surface_parameter(geometry,
	    model_curve.PointAt(one_third_parameter), rotation_one_third,
	    tolerance, one_third_surface_parameter) ||
	!spheroid_surface_parameter(geometry,
	    model_curve.PointAt(two_thirds_parameter), rotation_two_thirds,
	    tolerance, two_thirds_surface_parameter))
    {
	return false;
    }

    const ON_3dPoint first(first_surface_parameter.x,
	first_surface_parameter.y, 0.0);
    const ON_3dPoint second(second_surface_parameter.x,
	second_surface_parameter.y, 0.0);
    const ON_3dPoint third_sample(one_third_surface_parameter.x,
	one_third_surface_parameter.y, 0.0);
    const ON_3dPoint two_thirds_sample(two_thirds_surface_parameter.x,
	two_thirds_surface_parameter.y, 0.0);
    const ON_3dPoint first_control(
	(54.0 * third_sample.x - 27.0 * two_thirds_sample.x -
	    15.0 * first.x + 6.0 * second.x) / 18.0,
	(54.0 * third_sample.y - 27.0 * two_thirds_sample.y -
	    15.0 * first.y + 6.0 * second.y) / 18.0,
	0.0);
    const ON_3dPoint second_control(
	(54.0 * two_thirds_sample.x - 27.0 * third_sample.x +
	    6.0 * first.x - 15.0 * second.x) / 18.0,
	(54.0 * two_thirds_sample.y - 27.0 * third_sample.y +
	    6.0 * first.y - 15.0 * second.y) / 18.0,
	0.0);
    ON_3dPointArray controls;
    controls.Append(first);
    controls.Append(first_control);
    controls.Append(second_control);
    controls.Append(second);

    ON_BezierCurve bezier(controls);
    std::unique_ptr<ON_NurbsCurve> candidate_nurbs(ON_NurbsCurve::New());
    std::unique_ptr<ON_Curve> candidate;
    if (bezier.IsValid() && candidate_nurbs &&
	bezier.GetNurbForm(*candidate_nurbs) > 0)
    {
	candidate.reset(candidate_nurbs.release());
    }
    bool accepted = candidate && candidate->ChangeDimension(2) &&
	candidate->SetDomain(first_parameter, second_parameter) &&
	candidate->IsValid();
    static const double validation_fractions[9] = {
	0.0625, 0.125, 0.25, 0.375, 0.5,
	0.625, 0.75, 0.875, 0.9375
    };
    if (accepted) {
	for (int sample_index = 0; sample_index < 9; ++sample_index) {
	    const double parameter = first_parameter +
		validation_fractions[sample_index] * interval;
	    const ON_3dPoint model_point = model_curve.PointAt(parameter);
	    const ON_3dPoint surface_parameter = candidate->PointAt(parameter);
	    const ON_3dPoint lifted = geometry.revolution->PointAt(
		surface_parameter.x, surface_parameter.y);
	    if (!model_point.IsValid() || !surface_parameter.IsValid() ||
		!lifted.IsValid() || lifted.DistanceTo(model_point) >
		    0.5 * tolerance)
	    {
		accepted = false;
		break;
	    }
	}
    }
    if (accepted) {
	segments.push_back(std::move(candidate));
	++segment_count;
	return true;
    }

    if (depth == maximum_depth || segment_count >= maximum_segment_count)
	return false;
    const double middle_parameter = 0.5 * (first_parameter +
	second_parameter);
    const double middle_rotation = 0.5 *
	(first_surface_parameter[geometry.rotation_direction] +
	 second_surface_parameter[geometry.rotation_direction]);
    ON_2dPoint middle_surface_parameter;
    if (!spheroid_surface_parameter(geometry,
	    model_curve.PointAt(middle_parameter), middle_rotation, tolerance,
	    middle_surface_parameter))
    {
	return false;
    }
    return append_spheroid_parameter_beziers(geometry, model_curve,
	first_parameter, middle_parameter, first_surface_parameter,
	middle_surface_parameter, tolerance, depth + 1, segment_count,
	segments) &&
	append_spheroid_parameter_beziers(geometry, model_curve,
	    middle_parameter, second_parameter, middle_surface_parameter,
	    second_surface_parameter, tolerance, depth + 1, segment_count,
	    segments);
}


static ON_Curve *
spheroid_parameter_curve(const SpheroidRevolutionGeometry &geometry,
	const ON_Curve &model_curve, double tolerance)
{
    ON_Interval domain = model_curve.Domain();
    domain.MakeIncreasing();
    if (!domain.IsIncreasing())
	return NULL;

    ON_2dPoint first;
    ON_2dPoint middle;
    ON_2dPoint second;
    const double initial_rotation =
	geometry.full_domains[geometry.rotation_direction].Mid();
    if (!spheroid_surface_parameter(geometry, model_curve.PointAtStart(),
	    initial_rotation, tolerance, first) ||
	!spheroid_surface_parameter(geometry,
	    model_curve.PointAt(domain.Mid()),
	    first[geometry.rotation_direction], tolerance, middle) ||
	!spheroid_surface_parameter(geometry, model_curve.PointAtEnd(),
	    2.0 * middle[geometry.rotation_direction] -
		first[geometry.rotation_direction], tolerance, second))
    {
	return NULL;
    }
    const double rotation_period =
	geometry.full_domains[geometry.rotation_direction].Length();
    const double rotation_shift = std::round((
	geometry.full_domains[geometry.rotation_direction].Mid() -
	middle[geometry.rotation_direction]) / rotation_period) *
	rotation_period;
    first[geometry.rotation_direction] += rotation_shift;
    middle[geometry.rotation_direction] += rotation_shift;
    second[geometry.rotation_direction] += rotation_shift;

    std::vector<std::unique_ptr<ON_Curve> > segments;
    size_t segment_count = 0;
    if (!append_spheroid_parameter_beziers(geometry, model_curve,
	    domain.Min(), domain.Max(), first, second, tolerance, 0,
	    segment_count, segments) || segments.empty())
    {
	return NULL;
    }
    if (segments.size() == 1)
	return segments.front().release();

    std::unique_ptr<ON_PolyCurve> result(new ON_PolyCurve());
    result->Reserve(static_cast<int>(segments.size()));
    for (size_t segment_index = 0; segment_index < segments.size();
	 ++segment_index)
    {
	if (!result->Append(segments[segment_index].release()))
	    return NULL;
    }
    if (!result->IsValid() || result->Domain() != domain)
	return NULL;
    std::unique_ptr<ON_NurbsCurve> joined(ON_NurbsCurve::New());
    if (!joined || result->GetNurbForm(*joined) <= 0 ||
	!joined->IsValid() || joined->Domain() != domain)
    {
	return NULL;
    }
    return joined.release();
}


struct ParallelRevolutionGeometry {
    const ON_Surface *surface;
    const ON_RevSurface *revolution;
    const ON_Curve *profile;
    int profile_direction;
    int rotation_direction;
    ON_Interval domains[2];
    ON_Interval full_domains[2];
    ON_Interval angle_domain;
    ON_3dVector axis_direction;
    ON_3dVector radial_direction;
    ON_3dVector azimuthal_direction;
    double identity_tolerance;
};


static bool
parallel_revolution_geometry(const ON_Surface *surface,
	double intersection_tolerance, double parameter_tolerance,
	const ON_Interval *udomain, const ON_Interval *vdomain,
	ParallelRevolutionGeometry &geometry)
{
    const ON_RevSurface *revolution = ON_RevSurface::Cast(surface);
    const ON_Curve *profile = revolution ? revolution->m_curve : NULL;
    if (revolution == NULL || profile == NULL)
	return false;

    geometry.surface = surface;
    geometry.revolution = revolution;
    geometry.profile = profile;
    geometry.profile_direction = revolution->m_bTransposed ? 0 : 1;
    geometry.rotation_direction = 1 - geometry.profile_direction;
    if (!surface->IsClosed(geometry.rotation_direction))
	return false;

    geometry.domains[0] = udomain ? *udomain : surface->Domain(0);
    geometry.domains[1] = vdomain ? *vdomain : surface->Domain(1);
    geometry.full_domains[0] = surface->Domain(0);
    geometry.full_domains[1] = surface->Domain(1);
    for (int direction = 0; direction < 2; ++direction) {
	geometry.domains[direction].MakeIncreasing();
	geometry.full_domains[direction].MakeIncreasing();
	if (!geometry.domains[direction].IsIncreasing() ||
	    !geometry.full_domains[direction].IsIncreasing() ||
	    std::fabs(geometry.domains[direction].Min() -
		geometry.full_domains[direction].Min()) > parameter_tolerance ||
	    std::fabs(geometry.domains[direction].Max() -
		geometry.full_domains[direction].Max()) > parameter_tolerance)
	{
	    return false;
	}
    }

    ON_Interval profile_domain = profile->Domain();
    profile_domain.MakeIncreasing();
    if (!profile_domain.IsIncreasing() ||
	std::fabs(profile_domain.Min() -
	    geometry.full_domains[geometry.profile_direction].Min()) >
		parameter_tolerance ||
	std::fabs(profile_domain.Max() -
	    geometry.full_domains[geometry.profile_direction].Max()) >
		parameter_tolerance)
    {
	return false;
    }

    geometry.angle_domain = ON_Interval(revolution->m_angle);
    geometry.angle_domain.MakeIncreasing();
    if (!geometry.angle_domain.IsIncreasing() ||
	std::fabs(geometry.angle_domain.Length() - 2.0 * ON_PI) >
	    std::max(parameter_tolerance, 1024.0 * ON_EPSILON))
    {
	return false;
    }

    const ON_BoundingBox bounds = surface->BoundingBox();
    if (!bounds.IsValid())
	return false;
    double coordinate_scale = 1.0;
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[coordinate]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[coordinate]));
    }
    geometry.identity_tolerance = std::min(intersection_tolerance,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    if (!(geometry.identity_tolerance > 0.0) ||
	!revolution_meridian_frame(*revolution,
	    geometry.identity_tolerance, geometry.axis_direction,
	    geometry.radial_direction))
    {
	return false;
    }
    geometry.azimuthal_direction = ON_CrossProduct(
	geometry.axis_direction, geometry.radial_direction);
    if (!geometry.azimuthal_direction.Unitize())
	return false;

    /* The specialized construction uses the profile parameter as an axial
	 * coordinate.  Restrict it to simple, one-sided, axially monotone
	 * meridians; folded or signed-radius profiles stay on general SSI. */
    static const int certification_sample_count = 64;
    int axial_direction = 0;
    double previous_axis = 0.0;
    for (int sample_index = 0; sample_index <= certification_sample_count;
	 ++sample_index)
    {
	const double parameter = profile_domain.ParameterAt(
	    (double)sample_index / certification_sample_count);
	const ON_3dPoint point = profile->PointAt(parameter);
	if (!point.IsValid())
	    return false;
	const ON_3dVector offset = point - revolution->m_axis.from;
	const double axis_coordinate = ON_DotProduct(offset,
	    geometry.axis_direction);
	const ON_3dVector radial = offset -
	    axis_coordinate * geometry.axis_direction;
	const double signed_radius = ON_DotProduct(radial,
	    geometry.radial_direction);
	const double off_meridian = std::fabs(ON_DotProduct(radial,
	    geometry.azimuthal_direction));
	if (signed_radius < -geometry.identity_tolerance ||
	    off_meridian > geometry.identity_tolerance)
	{
	    return false;
	}
	if (sample_index > 0) {
	    const double delta = axis_coordinate - previous_axis;
	    if (std::fabs(delta) > geometry.identity_tolerance) {
		const int direction = delta > 0.0 ? 1 : -1;
		if (axial_direction != 0 && direction != axial_direction)
		    return false;
		axial_direction = direction;
	    }
	}
	previous_axis = axis_coordinate;
    }
    return axial_direction != 0;
}


struct ParallelRevolutionCircle {
    double profile_parameter;
    double profile_radius;
    double spheroid_radius;
    double line_coordinate;
    double height_squared;
    ON_3dPoint spheroid_axis_point;
    ON_3dPoint profile_axis_point;
};


static bool
parallel_revolution_circle(const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	double axis_separation, double profile_parameter,
	ParallelRevolutionCircle &circle)
{
    const ON_3dPoint profile_point =
	profile_geometry.profile->PointAt(profile_parameter);
    if (!profile_point.IsValid() || !(axis_separation > 0.0))
	return false;

    const ON_3dVector profile_axis_offset = profile_point -
	profile_geometry.revolution->m_axis.from;
    const double profile_axis_coordinate = ON_DotProduct(
	profile_axis_offset, profile_geometry.axis_direction);
    const ON_3dVector profile_radial = profile_axis_offset -
	profile_axis_coordinate * profile_geometry.axis_direction;
    const double profile_radius = ON_DotProduct(profile_radial,
	profile_geometry.radial_direction);
    if (!std::isfinite(profile_radius) ||
	profile_radius < -profile_geometry.identity_tolerance)
    {
	return false;
    }

    const double spheroid_axis_coordinate = ON_DotProduct(
	profile_point - spheroid.center, spheroid.axis_direction);
    double normalized_height = spheroid_axis_coordinate /
	spheroid.axial_radius;
    const double normalized_tolerance = spheroid.identity_tolerance /
	spheroid.axial_radius;
    if (normalized_height < -1.0 - normalized_tolerance ||
	normalized_height > 1.0 + normalized_tolerance)
    {
	return false;
    }
    normalized_height = std::max(-1.0,
	std::min(1.0, normalized_height));
    const double spheroid_radius = spheroid.radial_radius * std::sqrt(
	std::max(0.0, 1.0 - normalized_height * normalized_height));

    const ON_3dPoint spheroid_axis_point = spheroid.center +
	spheroid_axis_coordinate * spheroid.axis_direction;
    const double profile_plane_coordinate = ON_DotProduct(
	profile_point - profile_geometry.revolution->m_axis.from,
	spheroid.axis_direction);
    const ON_3dPoint profile_axis_point =
	profile_geometry.revolution->m_axis.from +
	profile_plane_coordinate * spheroid.axis_direction;
    const double line_coordinate = (axis_separation * axis_separation +
	spheroid_radius * spheroid_radius - profile_radius * profile_radius) /
	(2.0 * axis_separation);

    circle.profile_parameter = profile_parameter;
    circle.profile_radius = std::max(0.0, profile_radius);
    circle.spheroid_radius = spheroid_radius;
    circle.line_coordinate = line_coordinate;
    circle.height_squared = spheroid_radius * spheroid_radius -
	line_coordinate * line_coordinate;
    circle.spheroid_axis_point = spheroid_axis_point;
    circle.profile_axis_point = profile_axis_point;
    return std::isfinite(circle.height_squared) &&
	std::isfinite(circle.line_coordinate) &&
	circle.spheroid_axis_point.IsValid() &&
	circle.profile_axis_point.IsValid();
}


static bool
parallel_revolution_profile_parameter(
	const ParallelRevolutionGeometry &geometry,
	const ON_3dPoint &model_point, double profile_parameter,
	double rotation_seed, ON_2dPoint &parameter)
{
    const ON_3dVector offset = model_point - geometry.revolution->m_axis.from;
    const double axis_coordinate = ON_DotProduct(offset,
	geometry.axis_direction);
    ON_3dVector radial = offset -
	axis_coordinate * geometry.axis_direction;
    if (!radial.Unitize())
	return false;

    double angle = std::atan2(ON_DotProduct(radial,
	geometry.azimuthal_direction), ON_DotProduct(radial,
	geometry.radial_direction));
    while (angle < geometry.angle_domain.Min())
	angle += 2.0 * ON_PI;
    while (angle > geometry.angle_domain.Max())
	angle -= 2.0 * ON_PI;
    double rotation_parameter = geometry.revolution->m_t.ParameterAt(
	geometry.angle_domain.NormalizedParameterAt(angle));
    const double period =
	geometry.full_domains[geometry.rotation_direction].Length();
    if (!(period > ON_ZERO_TOLERANCE) ||
	!std::isfinite(rotation_parameter))
    {
	return false;
    }
    rotation_parameter += std::round((rotation_seed - rotation_parameter) /
	period) * period;

    if (geometry.rotation_direction == 0) {
	parameter = ON_2dPoint(rotation_parameter, profile_parameter);
    } else {
	parameter = ON_2dPoint(profile_parameter, rotation_parameter);
    }
    return parameter.IsValid();
}


struct ParallelRevolutionSample {
    double parameter;
    ON_3dPoint model_point;
    ON_2dPoint spheroid_parameter;
    ON_2dPoint profile_parameter;
};


static ON_3dPoint
periodic_surface_point(const ON_Surface *surface, ON_2dPoint parameter,
	int rotation_direction, const ON_Interval &rotation_domain)
{
    const double period = rotation_domain.Length();
    if (surface == NULL || !(period > ON_ZERO_TOLERANCE))
	return ON_3dPoint::UnsetPoint;
    while (parameter[rotation_direction] < rotation_domain.Min())
	parameter[rotation_direction] += period;
    while (parameter[rotation_direction] > rotation_domain.Max())
	parameter[rotation_direction] -= period;
    return surface->PointAt(parameter.x, parameter.y);
}


static bool
parallel_revolution_sample(
	const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	double axis_separation, const ON_3dVector &line_direction,
	const ON_3dVector &transverse_direction, double tangent_parameter,
	double boundary_parameter, double curve_parameter,
	double spheroid_rotation_seed, double profile_rotation_seed,
	double tolerance, ParallelRevolutionSample &sample)
{
    const double profile_parameter = tangent_parameter +
	curve_parameter * curve_parameter *
	(boundary_parameter - tangent_parameter);
    ParallelRevolutionCircle circle;
    if (!parallel_revolution_circle(spheroid, profile_geometry,
	    axis_separation, profile_parameter, circle))
    {
	return false;
    }

    double height_squared = circle.height_squared;
    const double squared_tolerance = tolerance * tolerance;
    if (std::fabs(curve_parameter) <= 64.0 * ON_EPSILON) {
	height_squared = 0.0;
    } else if (height_squared < -squared_tolerance) {
	return false;
    }
    const double height = std::sqrt(std::max(0.0, height_squared));
    const double signed_height = curve_parameter < 0.0 ? height : -height;
    const ON_3dPoint model_point = circle.spheroid_axis_point +
	circle.line_coordinate * line_direction +
	signed_height * transverse_direction;
    if (!model_point.IsValid())
	return false;

    ON_2dPoint spheroid_parameter;
    ON_2dPoint revolution_parameter;
    if (!spheroid_surface_parameter(spheroid, model_point,
	    spheroid_rotation_seed, tolerance, spheroid_parameter) ||
	!parallel_revolution_profile_parameter(profile_geometry, model_point,
	    profile_parameter, profile_rotation_seed, revolution_parameter))
    {
	return false;
    }

    const ON_3dPoint spheroid_lift = periodic_surface_point(
	spheroid.revolution, spheroid_parameter, spheroid.rotation_direction,
	spheroid.full_domains[spheroid.rotation_direction]);
    const ON_3dPoint profile_lift = periodic_surface_point(
	profile_geometry.surface, revolution_parameter,
	profile_geometry.rotation_direction,
	profile_geometry.full_domains[profile_geometry.rotation_direction]);
    if (!spheroid_lift.IsValid() || !profile_lift.IsValid() ||
	spheroid_lift.DistanceTo(model_point) > tolerance ||
	profile_lift.DistanceTo(model_point) > tolerance)
    {
	return false;
    }

    sample.parameter = curve_parameter;
    sample.model_point = model_point;
    sample.spheroid_parameter = spheroid_parameter;
    sample.profile_parameter = revolution_parameter;
    return true;
}


static double
parallel_revolution_interval_error(
	const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	const ParallelRevolutionSample &first,
	const ParallelRevolutionSample &middle,
	const ParallelRevolutionSample &second)
{
    const ON_3dPoint model_linear(
	0.5 * (first.model_point.x + second.model_point.x),
	0.5 * (first.model_point.y + second.model_point.y),
	0.5 * (first.model_point.z + second.model_point.z));
    const ON_2dPoint spheroid_linear(
	0.5 * (first.spheroid_parameter.x + second.spheroid_parameter.x),
	0.5 * (first.spheroid_parameter.y + second.spheroid_parameter.y));
    const ON_2dPoint profile_linear(
	0.5 * (first.profile_parameter.x + second.profile_parameter.x),
	0.5 * (first.profile_parameter.y + second.profile_parameter.y));
    const ON_3dPoint spheroid_lift = periodic_surface_point(
	spheroid.revolution, spheroid_linear, spheroid.rotation_direction,
	spheroid.full_domains[spheroid.rotation_direction]);
    const ON_3dPoint profile_lift = periodic_surface_point(
	profile_geometry.surface, profile_linear,
	profile_geometry.rotation_direction,
	profile_geometry.full_domains[profile_geometry.rotation_direction]);
    if (!model_linear.IsValid() || !spheroid_lift.IsValid() ||
	!profile_lift.IsValid())
    {
	return DBL_MAX;
    }
    return std::max(model_linear.DistanceTo(middle.model_point),
	std::max(spheroid_lift.DistanceTo(middle.model_point),
	    profile_lift.DistanceTo(middle.model_point)));
}


static bool
append_adaptive_parallel_revolution_interval(
	const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	double axis_separation, const ON_3dVector &line_direction,
	const ON_3dVector &transverse_direction, double tangent_parameter,
	double boundary_parameter, const ParallelRevolutionSample &first,
	const ParallelRevolutionSample &second, double tolerance, int depth,
	size_t &sample_count, std::vector<ParallelRevolutionSample> &samples)
{
    static const int maximum_depth = 20;
    static const size_t maximum_sample_count = 32769;
    if (depth > maximum_depth || sample_count >= maximum_sample_count)
	return false;

    const double middle_parameter = 0.5 *
	(first.parameter + second.parameter);
    ParallelRevolutionSample middle;
    if (!parallel_revolution_sample(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, middle_parameter,
	    0.5 * (first.spheroid_parameter[spheroid.rotation_direction] +
		second.spheroid_parameter[spheroid.rotation_direction]),
	    0.5 * (first.profile_parameter[
		profile_geometry.rotation_direction] +
		second.profile_parameter[
		profile_geometry.rotation_direction]),
	    tolerance, middle))
    {
	return false;
    }

    const double error = parallel_revolution_interval_error(spheroid,
	profile_geometry, first, middle, second);
    if (error <= 0.25 * tolerance) {
	samples.push_back(second);
	++sample_count;
	return true;
    }
    if (depth == maximum_depth || sample_count + 2 > maximum_sample_count)
	return false;
    return append_adaptive_parallel_revolution_interval(spheroid,
	profile_geometry, axis_separation, line_direction,
	transverse_direction, tangent_parameter, boundary_parameter, first,
	middle, tolerance, depth + 1, sample_count, samples) &&
	append_adaptive_parallel_revolution_interval(spheroid,
	    profile_geometry, axis_separation, line_direction,
	    transverse_direction, tangent_parameter, boundary_parameter,
	    middle, second, tolerance, depth + 1, sample_count, samples);
}


static bool
parallel_revolution_seam_sample(
	const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	double axis_separation, const ON_3dVector &line_direction,
	const ON_3dVector &transverse_direction, double tangent_parameter,
	double boundary_parameter, const ParallelRevolutionSample &first,
	const ParallelRevolutionSample &second, bool spheroid_seam,
	double seam_parameter, double parameter_tolerance, double tolerance,
	ParallelRevolutionSample &root)
{
    const int direction = spheroid_seam ? spheroid.rotation_direction :
	profile_geometry.rotation_direction;
    double first_value = spheroid_seam ?
	first.spheroid_parameter[direction] : first.profile_parameter[direction];
    double second_value = spheroid_seam ?
	second.spheroid_parameter[direction] : second.profile_parameter[direction];
    if ((first_value - seam_parameter) * (second_value - seam_parameter) >
	0.0)
    {
	return false;
    }

    ParallelRevolutionSample lower(first);
    ParallelRevolutionSample upper(second);
    for (int iteration = 0; iteration < 80; ++iteration) {
	const double middle_parameter = 0.5 *
	    (lower.parameter + upper.parameter);
	ParallelRevolutionSample middle;
	if (!parallel_revolution_sample(spheroid, profile_geometry,
		axis_separation, line_direction, transverse_direction,
		tangent_parameter, boundary_parameter, middle_parameter,
		0.5 * (lower.spheroid_parameter[
		    spheroid.rotation_direction] +
		    upper.spheroid_parameter[spheroid.rotation_direction]),
		0.5 * (lower.profile_parameter[
		    profile_geometry.rotation_direction] +
		    upper.profile_parameter[
		    profile_geometry.rotation_direction]),
		tolerance, middle))
	{
	    return false;
	}
	const double middle_value = spheroid_seam ?
	    middle.spheroid_parameter[direction] :
	    middle.profile_parameter[direction];
	root = middle;
	if (std::fabs(middle_value - seam_parameter) <=
		0.01 * parameter_tolerance ||
	    upper.parameter - lower.parameter <=
		128.0 * ON_EPSILON)
	{
	    break;
	}
	if ((first_value - seam_parameter) *
	    (middle_value - seam_parameter) <= 0.0)
	{
	    upper = middle;
	    second_value = middle_value;
	} else {
	    lower = middle;
	    first_value = middle_value;
	}
    }
    if (spheroid_seam) {
	root.spheroid_parameter[direction] = seam_parameter;
    } else {
	root.profile_parameter[direction] = seam_parameter;
    }
    return root.model_point.IsValid();
}


static bool
normalize_parallel_revolution_parameters(
	std::vector<ParallelRevolutionSample> &samples, bool spheroid_parameters,
	int rotation_direction, const ON_Interval &rotation_domain,
	double parameter_tolerance)
{
    if (samples.empty() || !rotation_domain.IsIncreasing())
	return false;
    const double period = rotation_domain.Length();
    if (!(period > ON_ZERO_TOLERANCE))
	return false;

    const ON_2dPoint &first_parameter = spheroid_parameters ?
	samples.front().spheroid_parameter : samples.front().profile_parameter;
    const ON_2dPoint &last_parameter = spheroid_parameters ?
	samples.back().spheroid_parameter : samples.back().profile_parameter;
    const double middle_rotation = 0.5 *
	(first_parameter[rotation_direction] +
	 last_parameter[rotation_direction]);
    const double shift = std::round((rotation_domain.Mid() -
	middle_rotation) / period) * period;
    for (ParallelRevolutionSample &sample : samples) {
	ON_2dPoint &parameter = spheroid_parameters ?
	    sample.spheroid_parameter : sample.profile_parameter;
	parameter[rotation_direction] += shift;
	if (std::fabs(parameter[rotation_direction] -
		rotation_domain.Min()) <= parameter_tolerance)
	{
	    parameter[rotation_direction] = rotation_domain.Min();
	}
	if (std::fabs(parameter[rotation_direction] -
		rotation_domain.Max()) <= parameter_tolerance)
	{
	    parameter[rotation_direction] = rotation_domain.Max();
	}
	if (parameter[rotation_direction] < rotation_domain.Min() -
		parameter_tolerance ||
	    parameter[rotation_direction] > rotation_domain.Max() +
		parameter_tolerance)
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel revolution normalization rejected %s %.17g in %.17g %.17g, shift %.17g midpoint %.17g tolerance %.17g\n",
		    spheroid_parameters ? "spheroid" : "profile",
		    parameter[rotation_direction], rotation_domain.Min(),
		    rotation_domain.Max(), shift,
		    middle_rotation + shift, parameter_tolerance);
	    return false;
	}
    }
    return true;
}


struct ParallelRevolutionCurveSegment {
    std::unique_ptr<ON_Curve> model_curve;
    std::unique_ptr<ON_Curve> spheroid_curve;
    std::unique_ptr<ON_Curve> profile_curve;
};


static ON_Curve *
parallel_revolution_cubic_curve(const ON_3dPoint &first,
	const ON_3dPoint &one_third, const ON_3dPoint &two_thirds,
	const ON_3dPoint &second, const ON_Interval &domain, int dimension)
{
    const ON_3dPoint first_control(
	(54.0 * one_third.x - 27.0 * two_thirds.x - 15.0 * first.x +
	    6.0 * second.x) / 18.0,
	(54.0 * one_third.y - 27.0 * two_thirds.y - 15.0 * first.y +
	    6.0 * second.y) / 18.0,
	(54.0 * one_third.z - 27.0 * two_thirds.z - 15.0 * first.z +
	    6.0 * second.z) / 18.0);
    const ON_3dPoint second_control(
	(54.0 * two_thirds.x - 27.0 * one_third.x + 6.0 * first.x -
	    15.0 * second.x) / 18.0,
	(54.0 * two_thirds.y - 27.0 * one_third.y + 6.0 * first.y -
	    15.0 * second.y) / 18.0,
	(54.0 * two_thirds.z - 27.0 * one_third.z + 6.0 * first.z -
	    15.0 * second.z) / 18.0);
    ON_3dPointArray controls;
    controls.Append(first);
    controls.Append(first_control);
    controls.Append(second_control);
    controls.Append(second);
    ON_BezierCurve bezier(controls);
    std::unique_ptr<ON_NurbsCurve> curve(ON_NurbsCurve::New());
    if (!bezier.IsValid() || !curve || bezier.GetNurbForm(*curve) <= 0 ||
	!curve->ChangeDimension(dimension) ||
	!curve->SetDomain(domain.Min(), domain.Max()) || !curve->IsValid())
    {
	return NULL;
    }
    return curve.release();
}


static bool
append_parallel_revolution_cubic_segments(
	const SpheroidRevolutionGeometry &spheroid,
	const ParallelRevolutionGeometry &profile_geometry,
	double axis_separation, const ON_3dVector &line_direction,
	const ON_3dVector &transverse_direction, double tangent_parameter,
	double boundary_parameter, const ParallelRevolutionSample &first,
	const ParallelRevolutionSample &second, double tolerance, int depth,
	size_t &segment_count,
	std::vector<ParallelRevolutionCurveSegment> &segments)
{
    static const int maximum_depth = 18;
    static const size_t maximum_segment_count = 4096;
    const ON_Interval domain(first.parameter, second.parameter);
    if (!domain.IsIncreasing() || depth > maximum_depth ||
	segment_count >= maximum_segment_count)
    {
	return false;
    }

    ParallelRevolutionSample one_third;
    ParallelRevolutionSample two_thirds;
    const double one_third_parameter = first.parameter +
	domain.Length() / 3.0;
    const double two_thirds_parameter = first.parameter +
	2.0 * domain.Length() / 3.0;
    const double spheroid_first_rotation =
	first.spheroid_parameter[spheroid.rotation_direction];
    const double spheroid_second_rotation =
	second.spheroid_parameter[spheroid.rotation_direction];
    const double profile_first_rotation =
	first.profile_parameter[profile_geometry.rotation_direction];
    const double profile_second_rotation =
	second.profile_parameter[profile_geometry.rotation_direction];
    if (!parallel_revolution_sample(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, one_third_parameter,
	    (2.0 * spheroid_first_rotation + spheroid_second_rotation) / 3.0,
	    (2.0 * profile_first_rotation + profile_second_rotation) / 3.0,
	    tolerance, one_third) ||
	!parallel_revolution_sample(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, two_thirds_parameter,
	    (spheroid_first_rotation + 2.0 * spheroid_second_rotation) / 3.0,
	    (profile_first_rotation + 2.0 * profile_second_rotation) / 3.0,
	    tolerance, two_thirds))
    {
	return false;
    }

    const ON_3dPoint first_spheroid(first.spheroid_parameter.x,
	first.spheroid_parameter.y, 0.0);
    const ON_3dPoint one_third_spheroid(one_third.spheroid_parameter.x,
	one_third.spheroid_parameter.y, 0.0);
    const ON_3dPoint two_thirds_spheroid(two_thirds.spheroid_parameter.x,
	two_thirds.spheroid_parameter.y, 0.0);
    const ON_3dPoint second_spheroid(second.spheroid_parameter.x,
	second.spheroid_parameter.y, 0.0);
    const ON_3dPoint first_profile(first.profile_parameter.x,
	first.profile_parameter.y, 0.0);
    const ON_3dPoint one_third_profile(one_third.profile_parameter.x,
	one_third.profile_parameter.y, 0.0);
    const ON_3dPoint two_thirds_profile(two_thirds.profile_parameter.x,
	two_thirds.profile_parameter.y, 0.0);
    const ON_3dPoint second_profile(second.profile_parameter.x,
	second.profile_parameter.y, 0.0);

    ParallelRevolutionCurveSegment candidate;
    candidate.model_curve.reset(parallel_revolution_cubic_curve(
	first.model_point, one_third.model_point, two_thirds.model_point,
	second.model_point, domain, 3));
    candidate.spheroid_curve.reset(parallel_revolution_cubic_curve(
	first_spheroid, one_third_spheroid, two_thirds_spheroid,
	second_spheroid, domain, 2));
    candidate.profile_curve.reset(parallel_revolution_cubic_curve(
	first_profile, one_third_profile, two_thirds_profile,
	second_profile, domain, 2));

    bool accepted = candidate.model_curve && candidate.spheroid_curve &&
	candidate.profile_curve;
    static const double validation_fractions[9] = {
	0.0625, 0.125, 0.25, 0.375, 0.5,
	0.625, 0.75, 0.875, 0.9375
    };
    if (accepted) {
	for (int sample_index = 0; sample_index < 9; ++sample_index) {
	    const double parameter = domain.ParameterAt(
		validation_fractions[sample_index]);
	    const ON_3dPoint candidate_model =
		candidate.model_curve->PointAt(parameter);
	    const ON_3dPoint candidate_spheroid =
		candidate.spheroid_curve->PointAt(parameter);
	    const ON_3dPoint candidate_profile =
		candidate.profile_curve->PointAt(parameter);
	    ParallelRevolutionSample exact;
	    if (!candidate_model.IsValid() || !candidate_spheroid.IsValid() ||
		!candidate_profile.IsValid() ||
		!parallel_revolution_sample(spheroid, profile_geometry,
		    axis_separation, line_direction, transverse_direction,
		    tangent_parameter, boundary_parameter, parameter,
		    candidate_spheroid[spheroid.rotation_direction],
		    candidate_profile[profile_geometry.rotation_direction],
		    tolerance, exact))
	    {
		accepted = false;
		break;
	    }
	    const ON_3dPoint spheroid_lift = periodic_surface_point(
		spheroid.revolution,
		ON_2dPoint(candidate_spheroid.x, candidate_spheroid.y),
		spheroid.rotation_direction,
		spheroid.full_domains[spheroid.rotation_direction]);
	    const ON_3dPoint profile_lift = periodic_surface_point(
		profile_geometry.surface,
		ON_2dPoint(candidate_profile.x, candidate_profile.y),
		profile_geometry.rotation_direction,
		profile_geometry.full_domains[
		    profile_geometry.rotation_direction]);
	    if (!spheroid_lift.IsValid() || !profile_lift.IsValid() ||
		candidate_model.DistanceTo(exact.model_point) >
		    0.25 * tolerance ||
		spheroid_lift.DistanceTo(exact.model_point) >
		    0.5 * tolerance ||
		profile_lift.DistanceTo(exact.model_point) >
		    0.5 * tolerance)
	    {
		accepted = false;
		break;
	    }
	}
    }
    if (accepted) {
	segments.push_back(std::move(candidate));
	++segment_count;
	return true;
    }

    if (depth == maximum_depth || segment_count >= maximum_segment_count)
	return false;
    ParallelRevolutionSample middle;
    if (!parallel_revolution_sample(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, domain.Mid(),
	    0.5 * (spheroid_first_rotation + spheroid_second_rotation),
	    0.5 * (profile_first_rotation + profile_second_rotation),
	    tolerance, middle))
    {
	return false;
    }
    return append_parallel_revolution_cubic_segments(spheroid,
	profile_geometry, axis_separation, line_direction,
	transverse_direction, tangent_parameter, boundary_parameter, first,
	middle, tolerance, depth + 1, segment_count, segments) &&
	append_parallel_revolution_cubic_segments(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, middle, second, tolerance,
	    depth + 1, segment_count, segments);
}


static bool
join_parallel_revolution_cubic_segments(
	std::vector<ParallelRevolutionCurveSegment> &segments,
	std::unique_ptr<ON_Curve> &model_curve,
	std::unique_ptr<ON_Curve> &spheroid_curve,
	std::unique_ptr<ON_Curve> &profile_curve)
{
    if (segments.empty())
	return false;
    if (segments.size() == 1) {
	model_curve = std::move(segments[0].model_curve);
	spheroid_curve = std::move(segments[0].spheroid_curve);
	profile_curve = std::move(segments[0].profile_curve);
	return model_curve && spheroid_curve && profile_curve;
    }

    std::unique_ptr<ON_PolyCurve> model_polycurve(new ON_PolyCurve());
    std::unique_ptr<ON_PolyCurve> spheroid_polycurve(new ON_PolyCurve());
    std::unique_ptr<ON_PolyCurve> profile_polycurve(new ON_PolyCurve());
    model_polycurve->Reserve(static_cast<int>(segments.size()));
    spheroid_polycurve->Reserve(static_cast<int>(segments.size()));
    profile_polycurve->Reserve(static_cast<int>(segments.size()));
    for (ParallelRevolutionCurveSegment &segment : segments) {
	if (!model_polycurve->Append(segment.model_curve.release()) ||
	    !spheroid_polycurve->Append(segment.spheroid_curve.release()) ||
	    !profile_polycurve->Append(segment.profile_curve.release()))
	{
	    return false;
	}
    }
    if (!model_polycurve->IsValid() || !spheroid_polycurve->IsValid() ||
	!profile_polycurve->IsValid())
    {
	return false;
    }
    std::unique_ptr<ON_NurbsCurve> joined_model(ON_NurbsCurve::New());
    std::unique_ptr<ON_NurbsCurve> joined_spheroid(ON_NurbsCurve::New());
    std::unique_ptr<ON_NurbsCurve> joined_profile(ON_NurbsCurve::New());
    if (!joined_model || !joined_spheroid || !joined_profile ||
	model_polycurve->GetNurbForm(*joined_model) <= 0 ||
	spheroid_polycurve->GetNurbForm(*joined_spheroid) <= 0 ||
	profile_polycurve->GetNurbForm(*joined_profile) <= 0 ||
	!joined_model->IsValid() || !joined_spheroid->IsValid() ||
	!joined_profile->IsValid() ||
	joined_model->Domain() != model_polycurve->Domain() ||
	joined_spheroid->Domain() != spheroid_polycurve->Domain() ||
	joined_profile->Domain() != profile_polycurve->Domain())
    {
	return false;
    }
    model_curve.reset(joined_model.release());
    spheroid_curve.reset(joined_spheroid.release());
    profile_curve.reset(joined_profile.release());
    return true;
}


struct ParallelPlanarTorusGeometry {
    const ON_Surface *plane_surface;
    const ON_Surface *revolution_surface;
    const ON_RevSurface *revolution;
    const ON_Curve *profile;
    AffinePlanarSurface plane_mapping;
    ON_Plane plane;
    int profile_direction;
    int rotation_direction;
    ON_Interval domains[2];
    ON_Interval full_domains[2];
    ON_Interval angle_domain;
    ON_3dVector axis_direction;
    ON_3dVector radial_direction;
    ON_3dVector azimuthal_direction;
    ON_3dVector plane_normal;
    ON_3dVector plane_direction;
    double plane_distance;
    double identity_tolerance;
    double parameter_tolerance;
};


struct ParallelPlanarTorusSample {
    double parameter;
    ON_3dPoint model_point;
    ON_2dPoint plane_parameter;
    ON_2dPoint revolution_parameter;
};


struct ParallelPlanarTorusCurveSegment {
    std::unique_ptr<ON_Curve> model_curve;
    std::unique_ptr<ON_Curve> plane_curve;
    std::unique_ptr<ON_Curve> revolution_curve;
};


static bool
parallel_planar_torus_sample(const ParallelPlanarTorusGeometry &geometry,
	int branch, double profile_parameter,
	ParallelPlanarTorusSample &sample)
{
    const ON_3dPoint profile_point = geometry.profile->PointAt(
	profile_parameter);
    if (!profile_point.IsValid())
	return false;

    const ON_3dVector profile_offset = profile_point -
	geometry.revolution->m_axis.from;
    const double axis_coordinate = ON_DotProduct(profile_offset,
	geometry.axis_direction);
    const ON_3dVector profile_radial = profile_offset -
	axis_coordinate * geometry.axis_direction;
    const double radius = ON_DotProduct(profile_radial,
	geometry.radial_direction);
    const double off_meridian = std::fabs(ON_DotProduct(profile_radial,
	geometry.azimuthal_direction));
    const double squared_height = radius * radius -
	geometry.plane_distance * geometry.plane_distance;
    if (!std::isfinite(axis_coordinate) || !std::isfinite(radius) ||
	off_meridian > geometry.identity_tolerance ||
	!(squared_height > 0.0))
    {
	return false;
    }

    const ON_3dPoint axis_point = geometry.revolution->m_axis.from +
	axis_coordinate * geometry.axis_direction;
    const double height = std::sqrt(squared_height);
    const ON_3dPoint model_point = axis_point -
	geometry.plane_distance * geometry.plane_normal +
	(branch == 0 ? height : -height) * geometry.plane_direction;
    ON_3dVector model_radial = model_point - axis_point;
    if (!model_point.IsValid() || !model_radial.Unitize())
	return false;

    double angle = std::atan2(ON_DotProduct(model_radial,
	geometry.azimuthal_direction), ON_DotProduct(model_radial,
	geometry.radial_direction));
    while (angle < geometry.angle_domain.Min())
	angle += 2.0 * ON_PI;
    while (angle >= geometry.angle_domain.Max())
	angle -= 2.0 * ON_PI;
    const double rotation_parameter = geometry.revolution->m_t.ParameterAt(
	geometry.angle_domain.NormalizedParameterAt(angle));
    ON_2dPoint revolution_parameter;
    if (geometry.rotation_direction == 0) {
	revolution_parameter = ON_2dPoint(rotation_parameter,
	    profile_parameter);
    } else {
	revolution_parameter = ON_2dPoint(profile_parameter,
	    rotation_parameter);
    }
    ON_2dPoint plane_parameter;
    if (!std::isfinite(rotation_parameter) ||
	!revolution_parameter.IsValid() ||
	!geometry.plane_mapping.ParametersAt(model_point, plane_parameter))
    {
	return false;
    }

    const ON_3dPoint revolution_lift = periodic_surface_point(
	geometry.revolution_surface, revolution_parameter,
	geometry.rotation_direction,
	geometry.full_domains[geometry.rotation_direction]);
    const ON_3dPoint plane_lift = geometry.plane_surface->PointAt(
	plane_parameter.x, plane_parameter.y);
    if (!revolution_lift.IsValid() || !plane_lift.IsValid() ||
	revolution_lift.DistanceTo(model_point) > geometry.identity_tolerance ||
	plane_lift.DistanceTo(model_point) > geometry.identity_tolerance)
    {
	return false;
    }

    sample.parameter = profile_parameter;
    sample.model_point = model_point;
    sample.plane_parameter = plane_parameter;
    sample.revolution_parameter = revolution_parameter;
    return true;
}


static bool
append_parallel_planar_torus_cubic_segments(
	const ParallelPlanarTorusGeometry &geometry, int branch,
	const ParallelPlanarTorusSample &first,
	const ParallelPlanarTorusSample &second, double tolerance, int depth,
	size_t &segment_count,
	std::vector<ParallelPlanarTorusCurveSegment> &segments)
{
    static const int maximum_depth = 18;
    static const size_t maximum_segment_count = 4096;
    const ON_Interval domain(first.parameter, second.parameter);
    if (!domain.IsIncreasing() || depth > maximum_depth ||
	segment_count >= maximum_segment_count)
    {
	return false;
    }

    ParallelPlanarTorusSample one_third;
    ParallelPlanarTorusSample two_thirds;
    if (!parallel_planar_torus_sample(geometry, branch,
	    domain.ParameterAt(1.0 / 3.0), one_third) ||
	!parallel_planar_torus_sample(geometry, branch,
	    domain.ParameterAt(2.0 / 3.0), two_thirds))
    {
	return false;
    }

    const ON_3dPoint first_plane(first.plane_parameter.x,
	first.plane_parameter.y, 0.0);
    const ON_3dPoint one_third_plane(one_third.plane_parameter.x,
	one_third.plane_parameter.y, 0.0);
    const ON_3dPoint two_thirds_plane(two_thirds.plane_parameter.x,
	two_thirds.plane_parameter.y, 0.0);
    const ON_3dPoint second_plane(second.plane_parameter.x,
	second.plane_parameter.y, 0.0);
    const ON_3dPoint first_revolution(first.revolution_parameter.x,
	first.revolution_parameter.y, 0.0);
    const ON_3dPoint one_third_revolution(one_third.revolution_parameter.x,
	one_third.revolution_parameter.y, 0.0);
    const ON_3dPoint two_thirds_revolution(two_thirds.revolution_parameter.x,
	two_thirds.revolution_parameter.y, 0.0);
    const ON_3dPoint second_revolution(second.revolution_parameter.x,
	second.revolution_parameter.y, 0.0);

    ParallelPlanarTorusCurveSegment candidate;
    candidate.model_curve.reset(parallel_revolution_cubic_curve(
	first.model_point, one_third.model_point, two_thirds.model_point,
	second.model_point, domain, 3));
    candidate.plane_curve.reset(parallel_revolution_cubic_curve(first_plane,
	one_third_plane, two_thirds_plane, second_plane, domain, 2));
    candidate.revolution_curve.reset(parallel_revolution_cubic_curve(
	first_revolution, one_third_revolution, two_thirds_revolution,
	second_revolution, domain, 2));

    bool accepted = candidate.model_curve && candidate.plane_curve &&
	candidate.revolution_curve;
    static const double validation_fractions[9] = {
	0.0625, 0.125, 0.25, 0.375, 0.5,
	0.625, 0.75, 0.875, 0.9375
    };
    if (accepted) {
	for (int sample_index = 0; sample_index < 9; ++sample_index) {
	    const double parameter = domain.ParameterAt(
		validation_fractions[sample_index]);
	    const ON_3dPoint candidate_model =
		candidate.model_curve->PointAt(parameter);
	    const ON_3dPoint candidate_plane =
		candidate.plane_curve->PointAt(parameter);
	    const ON_3dPoint candidate_revolution =
		candidate.revolution_curve->PointAt(parameter);
	    ParallelPlanarTorusSample exact;
	    if (!candidate_model.IsValid() || !candidate_plane.IsValid() ||
		!candidate_revolution.IsValid() ||
		!parallel_planar_torus_sample(geometry, branch, parameter,
		    exact))
	    {
		accepted = false;
		break;
	    }
	    const ON_3dPoint plane_lift = geometry.plane_surface->PointAt(
		candidate_plane.x, candidate_plane.y);
	    const ON_3dPoint revolution_lift = periodic_surface_point(
		geometry.revolution_surface,
		ON_2dPoint(candidate_revolution.x, candidate_revolution.y),
		geometry.rotation_direction,
		geometry.full_domains[geometry.rotation_direction]);
	    if (!plane_lift.IsValid() || !revolution_lift.IsValid() ||
		candidate_model.DistanceTo(exact.model_point) >
		    0.25 * tolerance ||
		plane_lift.DistanceTo(candidate_model) > 0.25 * tolerance ||
		revolution_lift.DistanceTo(candidate_model) >
		    0.25 * tolerance)
	    {
		accepted = false;
		break;
	    }
	}
    }
    if (accepted) {
	segments.push_back(std::move(candidate));
	++segment_count;
	return true;
    }

    if (depth == maximum_depth || segment_count >= maximum_segment_count)
	return false;
    ParallelPlanarTorusSample middle;
    if (!parallel_planar_torus_sample(geometry, branch, domain.Mid(),
	    middle))
    {
	return false;
    }
    return append_parallel_planar_torus_cubic_segments(geometry, branch,
	first, middle, tolerance, depth + 1, segment_count, segments) &&
	append_parallel_planar_torus_cubic_segments(geometry, branch, middle,
	    second, tolerance, depth + 1, segment_count, segments);
}


static bool
join_parallel_planar_torus_cubic_segments(
	std::vector<ParallelPlanarTorusCurveSegment> &segments,
	std::unique_ptr<ON_Curve> &model_curve,
	std::unique_ptr<ON_Curve> &plane_curve,
	std::unique_ptr<ON_Curve> &revolution_curve)
{
    if (segments.empty())
	return false;
    if (segments.size() == 1) {
	model_curve = std::move(segments[0].model_curve);
	plane_curve = std::move(segments[0].plane_curve);
	revolution_curve = std::move(segments[0].revolution_curve);
	return model_curve && plane_curve && revolution_curve;
    }

    std::unique_ptr<ON_PolyCurve> model_polycurve(new ON_PolyCurve());
    std::unique_ptr<ON_PolyCurve> plane_polycurve(new ON_PolyCurve());
    std::unique_ptr<ON_PolyCurve> revolution_polycurve(new ON_PolyCurve());
    model_polycurve->Reserve(static_cast<int>(segments.size()));
    plane_polycurve->Reserve(static_cast<int>(segments.size()));
    revolution_polycurve->Reserve(static_cast<int>(segments.size()));
    for (ParallelPlanarTorusCurveSegment &segment : segments) {
	if (!model_polycurve->Append(segment.model_curve.release()) ||
	    !plane_polycurve->Append(segment.plane_curve.release()) ||
	    !revolution_polycurve->Append(segment.revolution_curve.release()))
	{
	    return false;
	}
    }
    if (!model_polycurve->IsValid() || !plane_polycurve->IsValid() ||
	!revolution_polycurve->IsValid())
    {
	return false;
    }

    std::unique_ptr<ON_NurbsCurve> joined_model(ON_NurbsCurve::New());
    std::unique_ptr<ON_NurbsCurve> joined_plane(ON_NurbsCurve::New());
    std::unique_ptr<ON_NurbsCurve> joined_revolution(ON_NurbsCurve::New());
    if (!joined_model || !joined_plane || !joined_revolution ||
	model_polycurve->GetNurbForm(*joined_model) <= 0 ||
	plane_polycurve->GetNurbForm(*joined_plane) <= 0 ||
	revolution_polycurve->GetNurbForm(*joined_revolution) <= 0 ||
	!joined_model->IsValid() || !joined_plane->IsValid() ||
	!joined_revolution->IsValid())
    {
	return false;
    }
    model_curve.reset(joined_model.release());
    plane_curve.reset(joined_plane.release());
    revolution_curve.reset(joined_revolution.release());
    return true;
}


/* A plane parallel to a ring torus axis cuts the torus in two closed
 * quartic-like branches.  General SSI can incorrectly retrace one half of a
 * branch when the planar patch clips a very short section near the inner
 * radius.  Parameterize each branch by the torus profile instead.  Splitting
 * at the exact profile spans preserves the shared inner/outer turning points
 * needed by Boolean loop assembly. */
static bool
append_parallel_planar_torus_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ParallelPlanarTorusGeometry geometry;
    geometry.plane_surface = plane_surface;
    geometry.revolution_surface = revolution_surface;
    geometry.revolution = ON_RevSurface::Cast(revolution_surface);
    geometry.profile = geometry.revolution ? geometry.revolution->m_curve :
	NULL;
    if (plane_surface == NULL || geometry.revolution == NULL ||
	geometry.profile == NULL || !geometry.profile->IsClosed() ||
	!plane_surface->IsPlanar(&geometry.plane) ||
	!affine_planar_surface_mapping(plane_surface, geometry.plane_mapping))
    {
	return false;
    }

    geometry.profile_direction = geometry.revolution->m_bTransposed ? 0 : 1;
    geometry.rotation_direction = 1 - geometry.profile_direction;
    if (!revolution_surface->IsClosed(geometry.rotation_direction))
	return false;
    geometry.domains[0] = revolution_udomain ? *revolution_udomain :
	revolution_surface->Domain(0);
    geometry.domains[1] = revolution_vdomain ? *revolution_vdomain :
	revolution_surface->Domain(1);
    geometry.full_domains[0] = revolution_surface->Domain(0);
    geometry.full_domains[1] = revolution_surface->Domain(1);
    for (int direction = 0; direction < 2; ++direction) {
	geometry.domains[direction].MakeIncreasing();
	geometry.full_domains[direction].MakeIncreasing();
	if (!geometry.domains[direction].IsIncreasing() ||
	    !geometry.full_domains[direction].IsIncreasing() ||
	    std::fabs(geometry.domains[direction].Min() -
		geometry.full_domains[direction].Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(geometry.domains[direction].Max() -
		geometry.full_domains[direction].Max()) >
		revolution_parameter_tolerance)
	{
	    return false;
	}
    }
    ON_Interval profile_domain = geometry.profile->Domain();
    profile_domain.MakeIncreasing();
    if (!profile_domain.IsIncreasing() ||
	std::fabs(profile_domain.Min() -
	    geometry.full_domains[geometry.profile_direction].Min()) >
	    revolution_parameter_tolerance ||
	std::fabs(profile_domain.Max() -
	    geometry.full_domains[geometry.profile_direction].Max()) >
	    revolution_parameter_tolerance)
    {
	return false;
    }

    const ON_BoundingBox bounds = revolution_surface->BoundingBox();
    if (!bounds.IsValid())
	return false;
    double coordinate_scale = 1.0;
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[coordinate]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[coordinate]));
    }
    geometry.identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    geometry.parameter_tolerance = revolution_parameter_tolerance;
    geometry.plane_normal = geometry.plane.Normal();
    if (!geometry.plane_normal.Unitize() ||
	!revolution_meridian_frame(*geometry.revolution,
	    geometry.identity_tolerance, geometry.axis_direction,
	    geometry.radial_direction))
    {
	return false;
    }
    geometry.azimuthal_direction = ON_CrossProduct(geometry.axis_direction,
	geometry.radial_direction);
    geometry.plane_direction = ON_CrossProduct(geometry.axis_direction,
	geometry.plane_normal);
    if (!geometry.azimuthal_direction.Unitize() ||
	!geometry.plane_direction.Unitize())
    {
	return false;
    }

    ON_Arc profile_arc;
    const double profile_scale = geometry.profile->BoundingBox().Diagonal().
	Length();
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	geometry.identity_tolerance / std::max(1.0, profile_scale));
    ON_3dVector profile_normal;
    if (!(profile_scale > ON_ZERO_TOLERANCE) ||
	!geometry.profile->IsArc(NULL, &profile_arc,
	    geometry.identity_tolerance) || !profile_arc.IsCircle() ||
	!profile_arc.plane.IsValid() ||
	!(profile_arc.Radius() > geometry.identity_tolerance))
    {
	return false;
    }
    profile_normal = profile_arc.plane.Normal();
    if (!profile_normal.Unitize() ||
	std::fabs(ON_DotProduct(geometry.axis_direction,
	    geometry.plane_normal)) > angular_tolerance ||
	ON_CrossProduct(profile_normal,
	    geometry.azimuthal_direction).Length() > angular_tolerance)
    {
	return false;
    }

    const ON_3dPoint profile_center = profile_arc.Center();
    const ON_3dVector center_offset = profile_center -
	geometry.revolution->m_axis.from;
    const double center_axis_coordinate = ON_DotProduct(center_offset,
	geometry.axis_direction);
    const ON_3dVector center_radial = center_offset -
	center_axis_coordinate * geometry.axis_direction;
    const double major_radius = ON_DotProduct(center_radial,
	geometry.radial_direction);
    const double center_off_meridian = std::fabs(ON_DotProduct(center_radial,
	geometry.azimuthal_direction));
    const double minimum_radius = major_radius - profile_arc.Radius();
    geometry.plane_distance = geometry.plane.DistanceTo(
	geometry.revolution->m_axis.from);
    if (!std::isfinite(major_radius) ||
	center_off_meridian > geometry.identity_tolerance ||
	!(minimum_radius > geometry.identity_tolerance) ||
	std::fabs(geometry.plane_distance) <= geometry.identity_tolerance ||
	std::fabs(geometry.plane_distance) >= minimum_radius -
	    geometry.identity_tolerance)
    {
	return false;
    }

    geometry.angle_domain = ON_Interval(geometry.revolution->m_angle);
    geometry.angle_domain.MakeIncreasing();
    if (!geometry.angle_domain.IsIncreasing() ||
	std::fabs(geometry.angle_domain.Length() - 2.0 * ON_PI) >
	    std::max(revolution_parameter_tolerance,
		1024.0 * ON_EPSILON))
    {
	return false;
    }

    /* A parameter branch crossing the revolution seam must be split at the
	 * exact crossing.  Leave that uncommon case to general SSI for now rather
	 * than handing clipping an out-of-domain parameter curve. */
    const double rotation_period =
	geometry.full_domains[geometry.rotation_direction].Length();
    for (int branch = 0; branch < 2; ++branch) {
	double minimum_rotation = DBL_MAX;
	double maximum_rotation = -DBL_MAX;
	for (int sample_index = 0; sample_index <= 128; ++sample_index) {
	    ParallelPlanarTorusSample sample;
	    if (!parallel_planar_torus_sample(geometry, branch,
		    profile_domain.ParameterAt((double)sample_index / 128.0),
		    sample))
	    {
		return false;
	    }
	    const double rotation =
		sample.revolution_parameter[geometry.rotation_direction];
	    minimum_rotation = std::min(minimum_rotation, rotation);
	    maximum_rotation = std::max(maximum_rotation, rotation);
	}
	if (!(rotation_period > ON_ZERO_TOLERANCE) ||
	    maximum_rotation - minimum_rotation > 0.5 * rotation_period)
	{
	    return false;
	}
    }

    const int span_count = geometry.profile->SpanCount();
    if (span_count <= 0)
	return false;
    std::vector<double> span_parameters(span_count + 1, 0.0);
    if (!geometry.profile->GetSpanVector(span_parameters.data()))
	return false;

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (int branch = 0; branch < 2; ++branch) {
	for (int span_index = 0; span_index < span_count; ++span_index) {
	    const ON_Interval span_domain(span_parameters[span_index],
		span_parameters[span_index + 1]);
	    ParallelPlanarTorusSample first;
	    ParallelPlanarTorusSample second;
	    if (!span_domain.IsIncreasing() ||
		!parallel_planar_torus_sample(geometry, branch,
		    span_domain.Min(), first) ||
		!parallel_planar_torus_sample(geometry, branch,
		    span_domain.Max(), second))
	    {
		return false;
	    }

	    size_t segment_count = 0;
	    std::vector<ParallelPlanarTorusCurveSegment> segments;
	    if (!append_parallel_planar_torus_cubic_segments(geometry, branch,
		    first, second, isect_tol, 0, segment_count, segments))
	    {
		return false;
	    }
	    std::unique_ptr<ON_Curve> model_curve;
	    std::unique_ptr<ON_Curve> plane_curve;
	    std::unique_ptr<ON_Curve> revolution_curve;
	    if (!join_parallel_planar_torus_cubic_segments(segments,
		    model_curve, plane_curve, revolution_curve) ||
		!planar_ruled_event_is_coherent(*model_curve, *plane_curve,
		    *revolution_curve, *plane_surface, *revolution_surface,
		    isect_tol))
	    {
		return false;
	    }

	    ON_SSX_EVENT event;
	    ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
		revolution_curve.release();
	    ON_Curve *curveB = plane_is_surfaceA ?
		revolution_curve.release() : plane_curve.release();
	    set_ssx_event_from_curves(event, model_curve.release(), curveA,
		curveB, plane_is_surfaceA ? plane_surface : revolution_surface,
		plane_is_surfaceA ? revolution_surface : plane_surface);
	    generated_events.Append(event);
	    event.m_curve3d = NULL;
	    event.m_curveA = NULL;
	    event.m_curveB = NULL;
	}
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    if (DEBUG_BREP_INTERSECT) {
	bu_log("parallel-plane torus SSI generated %d profile-span events\n",
	    generated_events.Count());
    }
    return generated_events.Count() > 0;
}


/* Parallel offset surfaces of revolution reduce at each axial coordinate to
 * two circles.  Generic two-dimensional SSI refinement obscures that simple
 * structure near a circle tangency and can leave disconnected curve
 * fragments.  When one operand is a certified spheroid and the other has a
 * one-sided monotone meridian, construct the two circle-intersection branches
 * as one synchronized curve.  The squared profile parameterization makes the
 * joined curve regular at its tangent point. */
static bool
append_parallel_spheroid_revolution_ssi_events_ordered(
	const ON_Surface *spheroid_surface,
	const ON_Surface *profile_surface, bool spheroid_is_surfaceA,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double spheroid_parameter_tolerance,
	double profile_parameter_tolerance,
	const ON_Interval *spheroid_udomain,
	const ON_Interval *spheroid_vdomain,
	const ON_Interval *profile_udomain,
	const ON_Interval *profile_vdomain)
{
    SpheroidRevolutionGeometry spheroid;
    ParallelRevolutionGeometry profile_geometry;
    const bool have_spheroid = spheroid_revolution_geometry(
	spheroid_surface, isect_tol, spheroid_parameter_tolerance,
	spheroid_udomain, spheroid_vdomain, spheroid);
    const bool have_profile = parallel_revolution_geometry(profile_surface,
	isect_tol, profile_parameter_tolerance, profile_udomain,
	profile_vdomain, profile_geometry);
    if (DEBUG_BREP_INTERSECT)
	bu_log("parallel spheroid/revolution SSI recognition %d %d\n",
	    have_spheroid, have_profile);
    if (!have_spheroid || !have_profile)
    {
	return false;
    }

    ON_3dVector spheroid_axis = spheroid.axis_direction;
    ON_3dVector profile_axis = profile_geometry.axis_direction;
    if (!spheroid_axis.Unitize() || !profile_axis.Unitize() ||
	ON_CrossProduct(spheroid_axis, profile_axis).Length() >
	    std::max(1024.0 * ON_EPSILON,
		isect_tol / std::max(1.0,
		    spheroid_surface->BoundingBox().Diagonal().Length())))
    {
	return false;
    }

    const ON_3dVector axis_offset =
	profile_geometry.revolution->m_axis.from -
	spheroid.revolution->m_axis.from;
    const ON_3dVector transverse_offset = axis_offset - spheroid_axis *
	ON_DotProduct(axis_offset, spheroid_axis);
    const double axis_separation = transverse_offset.Length();
    if (!(axis_separation > isect_tol) || !std::isfinite(axis_separation))
	return false;
    ON_3dVector line_direction = transverse_offset;
    if (!line_direction.Unitize())
	return false;
    ON_3dVector transverse_direction = ON_CrossProduct(spheroid_axis,
	line_direction);
    if (!transverse_direction.Unitize())
	return false;

    ON_Interval profile_domain = profile_geometry.profile->Domain();
    profile_domain.MakeIncreasing();
    if (!profile_domain.IsIncreasing())
	return false;

    static const int root_scan_count = 256;
    bool previous_inside = false;
    bool have_previous = false;
    int transition_count = 0;
    double transition_first = 0.0;
    double transition_second = 0.0;
    ParallelRevolutionCircle first_circle;
    ParallelRevolutionCircle last_circle;
    for (int sample_index = 0; sample_index <= root_scan_count;
	 ++sample_index)
    {
	const double parameter = profile_domain.ParameterAt(
	    (double)sample_index / root_scan_count);
	ParallelRevolutionCircle circle;
	if (!parallel_revolution_circle(spheroid, profile_geometry,
		axis_separation, parameter, circle))
	{
	    return false;
	}
	if (sample_index == 0)
	    first_circle = circle;
	if (sample_index == root_scan_count)
	    last_circle = circle;
	const bool inside = circle.height_squared >= 0.0;
	if (have_previous && inside != previous_inside) {
	    ++transition_count;
	    transition_first = profile_domain.ParameterAt(
		(double)(sample_index - 1) / root_scan_count);
	    transition_second = parameter;
	}
	previous_inside = inside;
	have_previous = true;
    }
    const bool first_inside = first_circle.height_squared >
	isect_tol * isect_tol;
    const bool last_inside = last_circle.height_squared >
	isect_tol * isect_tol;
    if (DEBUG_BREP_INTERSECT)
	bu_log("parallel spheroid/revolution SSI roots transitions %d endpoints %d %d h2 %.17g %.17g\n",
	    transition_count, first_inside, last_inside,
	    first_circle.height_squared, last_circle.height_squared);
    if (transition_count != 1 || first_inside == last_inside)
	return false;

    double inside_parameter = first_inside ? transition_first :
	transition_second;
    double outside_parameter = first_inside ? transition_second :
	transition_first;
    for (int iteration = 0; iteration < 96; ++iteration) {
	const double middle = 0.5 * (inside_parameter + outside_parameter);
	ParallelRevolutionCircle circle;
	if (!parallel_revolution_circle(spheroid, profile_geometry,
		axis_separation, middle, circle))
	{
	    return false;
	}
	if (circle.height_squared >= 0.0)
	    inside_parameter = middle;
	else
	    outside_parameter = middle;
    }
    const double tangent_parameter = 0.5 *
	(inside_parameter + outside_parameter);
    const double boundary_parameter = first_inside ? profile_domain.Min() :
	profile_domain.Max();
    if (std::fabs(tangent_parameter - boundary_parameter) <=
	    128.0 * ON_EPSILON * std::max(1.0, profile_domain.Length()))
    {
	return false;
    }
    if (DEBUG_BREP_INTERSECT)
	bu_log("parallel spheroid/revolution SSI tangent %.17g boundary %.17g\n",
	    tangent_parameter, boundary_parameter);

    static const int initial_half_segment_count = 32;
    std::vector<ParallelRevolutionSample> initial_samples;
    initial_samples.resize(2 * initial_half_segment_count + 1);
    const double spheroid_seed =
	spheroid.full_domains[spheroid.rotation_direction].Mid();
    const double profile_seed = profile_geometry.full_domains[
	profile_geometry.rotation_direction].Mid();
    ParallelRevolutionSample center;
    if (!parallel_revolution_sample(spheroid, profile_geometry,
	    axis_separation, line_direction, transverse_direction,
	    tangent_parameter, boundary_parameter, 0.0, spheroid_seed,
	    profile_seed, isect_tol, center))
    {
	if (DEBUG_BREP_INTERSECT)
	    bu_log("parallel spheroid/revolution SSI rejected center sample\n");
	return false;
    }
    initial_samples[initial_half_segment_count] = center;

    ParallelRevolutionSample previous = center;
    for (int sample_index = 1; sample_index <= initial_half_segment_count;
	 ++sample_index)
    {
	const double parameter = -(double)sample_index /
	    initial_half_segment_count;
	ParallelRevolutionSample sample;
	if (!parallel_revolution_sample(spheroid, profile_geometry,
		axis_separation, line_direction, transverse_direction,
		tangent_parameter, boundary_parameter, parameter,
		previous.spheroid_parameter[spheroid.rotation_direction],
		previous.profile_parameter[
		    profile_geometry.rotation_direction],
		isect_tol, sample))
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel spheroid/revolution SSI rejected left sample %d\n",
		    sample_index);
	    return false;
	}
	initial_samples[initial_half_segment_count - sample_index] = sample;
	previous = sample;
    }
    previous = center;
    for (int sample_index = 1; sample_index <= initial_half_segment_count;
	 ++sample_index)
    {
	const double parameter = (double)sample_index /
	    initial_half_segment_count;
	ParallelRevolutionSample sample;
	if (!parallel_revolution_sample(spheroid, profile_geometry,
		axis_separation, line_direction, transverse_direction,
		tangent_parameter, boundary_parameter, parameter,
		previous.spheroid_parameter[spheroid.rotation_direction],
		previous.profile_parameter[
		    profile_geometry.rotation_direction],
		isect_tol, sample))
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel spheroid/revolution SSI rejected right sample %d\n",
		    sample_index);
	    return false;
	}
	initial_samples[initial_half_segment_count + sample_index] = sample;
	previous = sample;
    }

    /* These samples locate periodic seam crossings.  Final curves are fitted
     * independently below with synchronized adaptive cubics, so the SSI does
     * not expose tens of thousands of linear spans to loop arrangement. */
    std::vector<ParallelRevolutionSample> samples(initial_samples);

    std::vector<ParallelRevolutionSample> seam_samples;
    std::vector<double> split_parameters;
    split_parameters.push_back(-1.0);
    split_parameters.push_back(1.0);
    const auto find_seams = [&](bool spheroid_seams,
	const ON_Interval &rotation_domain, int rotation_direction,
	double parameter_tolerance) -> bool {
	const double period = rotation_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    return false;
	for (size_t sample_index = 1; sample_index + 1 < samples.size();
	     ++sample_index)
	{
	    const ParallelRevolutionSample &sample = samples[sample_index];
	    const double value = spheroid_seams ?
		sample.spheroid_parameter[rotation_direction] :
		sample.profile_parameter[rotation_direction];
	    const double seam = rotation_domain.Min() + std::round(
		(value - rotation_domain.Min()) / period) * period;
	    if (std::fabs(value - seam) <= parameter_tolerance)
		split_parameters.push_back(sample.parameter);
	}
	for (size_t sample_index = 1; sample_index < samples.size();
	     ++sample_index)
	{
	    const ParallelRevolutionSample &first = samples[sample_index - 1];
	    const ParallelRevolutionSample &second = samples[sample_index];
	    const double first_value = spheroid_seams ?
		first.spheroid_parameter[rotation_direction] :
		first.profile_parameter[rotation_direction];
	    const double second_value = spheroid_seams ?
		second.spheroid_parameter[rotation_direction] :
		second.profile_parameter[rotation_direction];
	    const double minimum_value = std::min(first_value, second_value);
	    const double maximum_value = std::max(first_value, second_value);
	    const long long first_boundary = static_cast<long long>(std::ceil(
		(minimum_value - rotation_domain.Min()) / period));
	    const long long last_boundary = static_cast<long long>(std::floor(
		(maximum_value - rotation_domain.Min()) / period));
	    for (long long boundary_index = first_boundary;
		 boundary_index <= last_boundary; ++boundary_index)
	    {
		const double seam = rotation_domain.Min() +
		    boundary_index * period;
		if (seam <= minimum_value + parameter_tolerance ||
		    seam >= maximum_value - parameter_tolerance)
		{
		    continue;
		}
		ParallelRevolutionSample root;
		if (!parallel_revolution_seam_sample(spheroid,
			profile_geometry, axis_separation, line_direction,
			transverse_direction, tangent_parameter,
			boundary_parameter, first, second, spheroid_seams,
			seam, parameter_tolerance, isect_tol, root))
		{
		    return false;
		}
		seam_samples.push_back(root);
		split_parameters.push_back(root.parameter);
	    }
	}
	return true;
    };
    if (!find_seams(true,
	    spheroid.full_domains[spheroid.rotation_direction],
	    spheroid.rotation_direction, spheroid_parameter_tolerance) ||
	!find_seams(false, profile_geometry.full_domains[
		profile_geometry.rotation_direction],
	    profile_geometry.rotation_direction,
	    profile_parameter_tolerance))
    {
	if (DEBUG_BREP_INTERSECT)
	    bu_log("parallel spheroid/revolution SSI rejected seam construction\n");
	return false;
    }

    samples.insert(samples.end(), seam_samples.begin(), seam_samples.end());
    std::sort(samples.begin(), samples.end(),
	[](const ParallelRevolutionSample &first,
	    const ParallelRevolutionSample &second) {
	    return first.parameter < second.parameter;
	});
    std::vector<ParallelRevolutionSample> unique_samples;
    unique_samples.reserve(samples.size());
    for (const ParallelRevolutionSample &sample : samples) {
	if (!unique_samples.empty() &&
	    std::fabs(sample.parameter - unique_samples.back().parameter) <=
		128.0 * ON_EPSILON)
	{
	    unique_samples.back() = sample;
	} else {
	    unique_samples.push_back(sample);
	}
    }
    samples.swap(unique_samples);

    std::sort(split_parameters.begin(), split_parameters.end());
    split_parameters.erase(std::unique(split_parameters.begin(),
	split_parameters.end(), [](double first, double second) {
	    return std::fabs(first - second) <= 128.0 * ON_EPSILON;
	}), split_parameters.end());
    if (split_parameters.size() < 2)
	return false;
    if (DEBUG_BREP_INTERSECT) {
	bu_log("parallel spheroid/revolution SSI split parameters:");
	for (double parameter : split_parameters)
	    bu_log(" %.17g", parameter);
	bu_log("\n");
    }

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (size_t split_index = 1; split_index < split_parameters.size();
	 ++split_index)
    {
	const double first_parameter = split_parameters[split_index - 1];
	const double second_parameter = split_parameters[split_index];
	std::vector<ParallelRevolutionSample> segment_samples;
	for (const ParallelRevolutionSample &sample : samples) {
	    if (std::fabs(sample.parameter - first_parameter) <=
		    128.0 * ON_EPSILON ||
		std::fabs(sample.parameter - second_parameter) <=
		    128.0 * ON_EPSILON)
	    {
		segment_samples.push_back(sample);
	    }
	}
	std::sort(segment_samples.begin(), segment_samples.end(),
	    [](const ParallelRevolutionSample &first,
		const ParallelRevolutionSample &second) {
		return first.parameter < second.parameter;
	    });
	if (segment_samples.size() != 2 ||
	    std::fabs(segment_samples.front().parameter - first_parameter) >
		128.0 * ON_EPSILON ||
	    std::fabs(segment_samples.back().parameter - second_parameter) >
		128.0 * ON_EPSILON ||
	    !normalize_parallel_revolution_parameters(segment_samples, true,
		spheroid.rotation_direction,
		spheroid.full_domains[spheroid.rotation_direction],
		spheroid_parameter_tolerance) ||
	    !normalize_parallel_revolution_parameters(segment_samples, false,
		profile_geometry.rotation_direction,
		profile_geometry.full_domains[
		    profile_geometry.rotation_direction],
		profile_parameter_tolerance))
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel spheroid/revolution SSI rejected segment %zu normalization\n",
		    split_index);
	    return false;
	}

	std::vector<ParallelRevolutionCurveSegment> cubic_segments;
	size_t cubic_segment_count = 0;
	if (!append_parallel_revolution_cubic_segments(spheroid,
		profile_geometry, axis_separation, line_direction,
		transverse_direction, tangent_parameter, boundary_parameter,
		segment_samples.front(), segment_samples.back(), isect_tol, 0,
		cubic_segment_count, cubic_segments))
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel spheroid/revolution SSI rejected segment %zu cubic fit\n",
		    split_index);
	    return false;
	}
	std::unique_ptr<ON_Curve> model_curve;
	std::unique_ptr<ON_Curve> spheroid_curve;
	std::unique_ptr<ON_Curve> profile_curve;
	if (!join_parallel_revolution_cubic_segments(cubic_segments,
		model_curve, spheroid_curve, profile_curve) ||
	    !planar_ruled_event_is_coherent(*model_curve, *spheroid_curve,
		*profile_curve, *spheroid_surface, *profile_surface,
		isect_tol))
	{
	    if (DEBUG_BREP_INTERSECT)
		bu_log("parallel spheroid/revolution SSI rejected segment %zu coherence\n",
		    split_index);
	    return false;
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = spheroid_is_surfaceA ?
	    static_cast<ON_Curve *>(spheroid_curve.release()) :
	    static_cast<ON_Curve *>(profile_curve.release());
	ON_Curve *curveB = spheroid_is_surfaceA ?
	    static_cast<ON_Curve *>(profile_curve.release()) :
	    static_cast<ON_Curve *>(spheroid_curve.release());
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    spheroid_is_surfaceA ? spheroid_surface : profile_surface,
	    spheroid_is_surfaceA ? profile_surface : spheroid_surface);
	generated_events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
    }

    if (generated_events.Count() == 0)
	return false;
    if (DEBUG_BREP_INTERSECT)
	bu_log("parallel spheroid/revolution SSI generated %d events from %zu samples\n",
	    generated_events.Count(), samples.size());
    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    return true;
}


static bool
append_parallel_spheroid_revolution_ssi_events(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_parallel_spheroid_revolution_ssi_events_ordered(
	surfA, surfB, true, events, isect_tol, isect_tolA, isect_tolB,
	surfaceA_udomain, surfaceA_vdomain, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_parallel_spheroid_revolution_ssi_events_ordered(
	    surfB, surfA, false, events, isect_tol, isect_tolB, isect_tolA,
	    surfaceB_udomain, surfaceB_vdomain, surfaceA_udomain,
	    surfaceA_vdomain);
}


/* A plane parallel to, but offset from, the axis of a spheroid intersects it
 * in an exact ellipse.  At large model coordinates generic surface/surface
 * refinement can return a plausible plane curve paired with a parameter
 * curve that is kilometers off the spheroid.  Recognize the certified
 * elliptical meridian, construct the model-space ellipse exactly, and fit
 * only its synchronized revolution parameters. */
static bool
append_parallel_planar_spheroid_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Plane plane;
    AffinePlanarSurface plane_mapping;
    SpheroidRevolutionGeometry geometry;
    if (plane_surface == NULL || revolution_surface == NULL ||
	!plane_surface->IsPlanar(&plane) || !plane.IsValid() ||
	!affine_planar_surface_mapping(plane_surface, plane_mapping) ||
	!spheroid_revolution_geometry(revolution_surface, isect_tol,
	    revolution_parameter_tolerance, revolution_udomain,
	    revolution_vdomain, geometry))
    {
	return false;
    }

    ON_3dVector normal = plane.Normal();
    if (!normal.Unitize())
	return false;
    const double radius_scale = std::max(geometry.radial_radius,
	geometry.axial_radius);
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	geometry.identity_tolerance / std::max(1.0, radius_scale));
    const double axis_dot = ON_DotProduct(normal,
	geometry.axis_direction);
    const double center_distance = plane.DistanceTo(geometry.center);
    if (std::fabs(axis_dot) > angular_tolerance ||
	std::fabs(center_distance) <= geometry.identity_tolerance ||
	std::fabs(center_distance) >= geometry.radial_radius -
	    geometry.identity_tolerance)
    {
	return false;
    }

    ON_3dVector horizontal_direction = ON_CrossProduct(
	geometry.axis_direction, normal);
    if (!horizontal_direction.Unitize())
	return false;
    const double normalized_distance = center_distance /
	geometry.radial_radius;
    const double section_fraction_squared = 1.0 -
	normalized_distance * normalized_distance;
    if (!(section_fraction_squared > 0.0))
	return false;
    const double section_fraction = std::sqrt(section_fraction_squared);
    const double horizontal_radius = geometry.radial_radius *
	section_fraction;
    const double axial_radius = geometry.axial_radius * section_fraction;
    const ON_3dPoint section_center = geometry.center -
	center_distance * normal;
    const ON_Plane section_plane(section_center, horizontal_direction,
	geometry.axis_direction);
    const ON_Ellipse section_ellipse(section_plane, horizontal_radius,
	axial_radius);
    ON_NurbsCurve complete_model_curve;
    if (!section_plane.IsValid() || !section_ellipse.IsValid() ||
	section_ellipse.GetNurbForm(complete_model_curve) <= 0 ||
	!complete_model_curve.IsValid() ||
	!complete_model_curve.IsClosed())
    {
	return false;
    }

    const int span_count = complete_model_curve.SpanCount();
    if (span_count <= 0)
	return false;
    std::vector<double> span_parameters(span_count + 1, 0.0);
    if (!complete_model_curve.GetSpanVector(span_parameters.data()))
	return false;

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (int span_index = 0; span_index < span_count; ++span_index) {
	const ON_Interval span_domain(span_parameters[span_index],
	    span_parameters[span_index + 1]);
	std::unique_ptr<ON_Curve> model_curve(
	    complete_model_curve.DuplicateCurve());
	if (!span_domain.IsIncreasing() || !model_curve ||
	    !model_curve->Trim(span_domain) || !model_curve->IsValid() ||
	    model_curve->Dimension() != 3)
	{
	    return false;
	}

	std::unique_ptr<ON_Curve> plane_curve(
	    affine_planar_parameter_curve(*model_curve, plane,
		plane_mapping, geometry.identity_tolerance, false));
	std::unique_ptr<ON_Curve> revolution_curve(
	    spheroid_parameter_curve(geometry, *model_curve, isect_tol));
	if (!plane_curve || !revolution_curve ||
	    !planar_ruled_event_is_coherent(*model_curve, *plane_curve,
		*revolution_curve, *plane_surface, *revolution_surface,
		isect_tol))
	{
	    return false;
	}
	if (DEBUG_BREP_INTERSECT) {
	    const ON_3dPoint uv_start = revolution_curve->PointAtStart();
	    const ON_3dPoint uv_middle = revolution_curve->PointAt(
		revolution_curve->Domain().Mid());
	    const ON_3dPoint uv_end = revolution_curve->PointAtEnd();
	    bu_log("parallel-plane spheroid SSI span %d UV %.17g %.17g -> "
		"%.17g %.17g -> %.17g %.17g\n", span_index,
		uv_start.x, uv_start.y, uv_middle.x, uv_middle.y,
		uv_end.x, uv_end.y);
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
	    revolution_curve.release();
	ON_Curve *curveB = plane_is_surfaceA ? revolution_curve.release() :
	    plane_curve.release();
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    plane_is_surfaceA ? plane_surface : revolution_surface,
	    plane_is_surfaceA ? revolution_surface : plane_surface);
	generated_events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    if (DEBUG_BREP_INTERSECT) {
	bu_log("parallel-plane spheroid SSI generated %d ellipse arcs\n",
	    generated_events.Count());
    }
    return generated_events.Count() > 0;
}


/* A plane containing the axis of a complete surface of revolution intersects
 * it in two opposite meridians.  Asking general SSI to discover those curves
 * is particularly ill-conditioned when the model coordinates are large: the
 * surface is singular at both shared pole endpoints and the two branches are
 * coincident in parameter space only at those boundaries.  Construct the two
 * exact isocurves and their synchronized parameter curves directly. */
static bool
append_axis_planar_revolution_ssi_events_ordered(
	const ON_Surface *plane_surface, const ON_Surface *revolution_surface,
	bool plane_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double revolution_parameter_tolerance,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Plane plane;
    AffinePlanarSurface plane_mapping;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(revolution_surface);
    if (revolution == NULL || revolution->m_curve == NULL ||
	!plane_surface->IsPlanar(&plane) ||
	!affine_planar_surface_mapping(plane_surface, plane_mapping))
    {
	return false;
    }

    const int profile_direction = revolution->m_bTransposed ? 0 : 1;
    const int rotation_direction = 1 - profile_direction;
    if (!revolution_surface->IsClosed(rotation_direction)) {
	bu_log("axis-plane SSI rejected nonclosed revolution\n");
	return false;
    }

    ON_Interval domains[2] = {
	revolution_udomain ? *revolution_udomain :
	    revolution_surface->Domain(0),
	revolution_vdomain ? *revolution_vdomain :
	    revolution_surface->Domain(1)
    };
    ON_Interval full_domains[2] = {
	revolution_surface->Domain(0), revolution_surface->Domain(1)
    };
    for (int direction = 0; direction < 2; ++direction) {
	domains[direction].MakeIncreasing();
	full_domains[direction].MakeIncreasing();
	if (!domains[direction].IsIncreasing() ||
	    !full_domains[direction].IsIncreasing() ||
	    std::fabs(domains[direction].Min() -
		full_domains[direction].Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(domains[direction].Max() -
		full_domains[direction].Max()) >
		revolution_parameter_tolerance)
	{
	    bu_log("axis-plane SSI rejected domain %d\n", direction);
	    return false;
	}
    }

    const ON_BoundingBox bounds = revolution_surface->BoundingBox();
    if (!bounds.IsValid()) {
	bu_log("axis-plane SSI rejected bounds\n");
	return false;
    }
    double coordinate_scale = 1.0;
    for (int coordinate = 0; coordinate < 3; ++coordinate) {
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_min[coordinate]));
	coordinate_scale = std::max(coordinate_scale,
	    std::fabs(bounds.m_max[coordinate]));
    }
    const double profile_scale = revolution->m_curve->BoundingBox().
	Diagonal().Length();
    if (!(profile_scale > ON_ZERO_TOLERANCE) ||
	!std::isfinite(profile_scale))
    {
	bu_log("axis-plane SSI rejected profile scale\n");
	return false;
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	identity_tolerance / std::max(1.0, profile_scale));

    ON_3dVector axis_direction(0.0, 0.0, 0.0);
    ON_3dVector base_radial(0.0, 0.0, 0.0);
    ON_3dVector plane_normal = plane.Normal();
    if (!plane_normal.Unitize() ||
	!revolution_meridian_frame(*revolution, identity_tolerance,
	    axis_direction, base_radial) ||
	std::fabs(axis_direction * plane_normal) > angular_tolerance ||
	std::fabs(plane.DistanceTo(revolution->m_axis.from)) >
	    identity_tolerance)
    {
	bu_log("axis-plane SSI rejected frame: ndot %.17g distance %.17g tol %.17g angular %.17g\n",
	    axis_direction * plane_normal,
	    plane.DistanceTo(revolution->m_axis.from), identity_tolerance,
	    angular_tolerance);
	return false;
    }

    ON_3dVector plane_radial = ON_CrossProduct(plane_normal,
	axis_direction);
    if (!plane_radial.Unitize()) {
	bu_log("axis-plane SSI rejected radial\n");
	return false;
    }

    ON_Interval angle_domain(revolution->m_angle);
    angle_domain.MakeIncreasing();
    if (!angle_domain.IsIncreasing() ||
	std::fabs(angle_domain.Length() - 2.0 * ON_PI) > angular_tolerance)
    {
	bu_log("axis-plane SSI rejected angle domain %.17g %.17g\n",
	    angle_domain.Min(), angle_domain.Max());
	return false;
    }

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (int branch = 0; branch < 2; ++branch) {
	const ON_3dVector target = branch == 0 ? plane_radial : -plane_radial;
	const double cosine = std::max(-1.0, std::min(1.0,
	    base_radial * target));
	const double sine = axis_direction * ON_CrossProduct(base_radial,
	    target);
	double angle = std::atan2(sine, cosine);
	while (angle < angle_domain.Min())
	    angle += 2.0 * ON_PI;
	while (angle >= angle_domain.Max())
	    angle -= 2.0 * ON_PI;
	if (angle_domain.Max() - angle <= angular_tolerance)
	    angle = angle_domain.Min();
	const double rotation_parameter = revolution->m_t.ParameterAt(
	    angle_domain.NormalizedParameterAt(angle));
	if (!parameter_in_domain(rotation_parameter,
		domains[rotation_direction], revolution_parameter_tolerance))
	{
	    bu_log("axis-plane SSI rejected rotation parameter %.17g\n",
		rotation_parameter);
	    return false;
	}

	std::unique_ptr<ON_Curve> model_curve(revolution_surface->IsoCurve(
	    profile_direction, rotation_parameter));
	if (!model_curve || !model_curve->IsValid() ||
	    model_curve->IsClosed() || model_curve->Dimension() != 3)
	{
	    bu_log("axis-plane SSI rejected branch %d model curve\n", branch);
	    return false;
	}
	ON_Interval model_domain = model_curve->Domain();
	model_domain.MakeIncreasing();
	if (!model_domain.IsIncreasing() ||
	    std::fabs(model_domain.Min() -
		full_domains[profile_direction].Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(model_domain.Max() -
		full_domains[profile_direction].Max()) >
		revolution_parameter_tolerance)
	{
	    bu_log("axis-plane SSI rejected branch %d model domain %.17g %.17g\n",
		branch, model_domain.Min(), model_domain.Max());
	    return false;
	}
	for (int sample_index = 0; sample_index <= 16; ++sample_index) {
	    const ON_3dPoint point = model_curve->PointAt(
		model_domain.ParameterAt((double)sample_index / 16.0));
	    if (!point.IsValid() ||
		std::fabs(plane.DistanceTo(point)) > identity_tolerance)
	    {
		bu_log("axis-plane SSI rejected branch %d sample %d distance %.17g tolerance %.17g\n",
		    branch, sample_index, plane.DistanceTo(point),
		    identity_tolerance);
		return false;
	    }
	}

	std::unique_ptr<ON_Curve> plane_curve(
	    affine_planar_parameter_curve(*model_curve, plane, plane_mapping,
		identity_tolerance, false));
	if (!plane_curve) {
	    bu_log("axis-plane SSI rejected branch %d plane curve\n", branch);
	    return false;
	}

	const ON_3dPoint revolution_start = rotation_direction == 0 ?
	    ON_3dPoint(rotation_parameter, model_domain.Min(), 0.0) :
	    ON_3dPoint(model_domain.Min(), rotation_parameter, 0.0);
	const ON_3dPoint revolution_end = rotation_direction == 0 ?
	    ON_3dPoint(rotation_parameter, model_domain.Max(), 0.0) :
	    ON_3dPoint(model_domain.Max(), rotation_parameter, 0.0);
	std::unique_ptr<ON_LineCurve> revolution_curve(
	    new ON_LineCurve(revolution_start, revolution_end));
	if (!revolution_curve || !revolution_curve->ChangeDimension(2) ||
	    !revolution_curve->SetDomain(model_domain.Min(), model_domain.Max()) ||
	    !revolution_curve->IsValid() ||
	    !planar_ruled_event_is_coherent(*model_curve, *plane_curve,
		*revolution_curve, *plane_surface, *revolution_surface,
		isect_tol))
	{
	    bu_log("axis-plane SSI rejected branch %d coherence\n", branch);
	    return false;
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = plane_is_surfaceA ? plane_curve.release() :
	    revolution_curve.release();
	ON_Curve *curveB = plane_is_surfaceA ? revolution_curve.release() :
	    plane_curve.release();
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    plane_is_surfaceA ? plane_surface : revolution_surface,
	    plane_is_surfaceA ? revolution_surface : plane_surface);
	generated_events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
    }

    if (generated_events.Count() != 2)
	return false;
    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    bu_log("axis-plane revolution SSI generated two meridians\n");
    return true;
}


static bool
parameter_curve_profile_is_in_domain(const ON_Curve &parameter_curve,
	int profile_direction, const ON_Interval &profile_domain,
	double parameter_tolerance)
{
    if (parameter_curve.Dimension() != 2 ||
	!parameter_curve.Domain().IsIncreasing() ||
	!profile_domain.IsIncreasing())
    {
	return false;
    }
    const int sample_count = std::max(16, 2 * parameter_curve.SpanCount());
    for (int sample_index = 0; sample_index <= sample_count; ++sample_index) {
	const ON_3dPoint parameter = parameter_curve.PointAt(
	    parameter_curve.Domain().ParameterAt(
		(double)sample_index / sample_count));
	const double profile_parameter = parameter[profile_direction];
	if (!parameter.IsValid() ||
	    !parameter_in_domain(profile_parameter, profile_domain,
		parameter_tolerance))
	{
	    return false;
	}
    }
    return true;
}


struct CoaxialParameterSample {
    double model_parameter;
    ON_2dPoint surface_parameter;
};


static bool
coaxial_project_surface_parameter(brlcad::PullbackContext &context,
	const ON_Surface *surface, const ON_3dPoint &model_point,
	const ON_2dPoint &seed, const bool closed[2],
	const ON_Interval domains[2], double tolerance,
	ON_2dPoint &surface_parameter)
{
    ON_2dPoint native_seed(seed);
    for (int direction = 0; direction < 2; ++direction) {
	if (!closed[direction])
	    continue;
	const double period = domains[direction].Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    return false;
	while (native_seed[direction] < domains[direction].Min())
	    native_seed[direction] += period;
	while (native_seed[direction] > domains[direction].Max())
	    native_seed[direction] -= period;
    }

    ON_3dPoint lifted_point;
    double distance = DBL_MAX;
    bool projected = context.SurfaceClosestPointFromSeed(surface,
	model_point, native_seed, surface_parameter, lifted_point, distance,
	tolerance, closed, domains,
	std::max(ON_ZERO_TOLERANCE, 0.01 * tolerance));
    if (!projected || !surface_parameter.IsValid() ||
	!lifted_point.IsValid() || !std::isfinite(distance) ||
	distance > tolerance)
    {
	distance = DBL_MAX;
	projected = context.SurfaceClosestPoint(surface, model_point,
	    surface_parameter, lifted_point, distance, 0,
	    ON_ZERO_TOLERANCE, tolerance);
    }
    if (!projected || !surface_parameter.IsValid() ||
	!lifted_point.IsValid() || !std::isfinite(distance) ||
	distance > tolerance)
    {
	return false;
    }

    for (int direction = 0; direction < 2; ++direction) {
	if (!closed[direction])
	    continue;
	const double period = domains[direction].Length();
	surface_parameter[direction] += std::round(
	    (seed[direction] - surface_parameter[direction]) / period) *
	    period;
    }
    return true;
}


static double
coaxial_parameter_interval_error(const ON_Surface *surface,
	const ON_Curve &model_curve, const CoaxialParameterSample &first,
	const CoaxialParameterSample &second)
{
    static const double fractions[7] = {
	0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875
    };
    double maximum_error = 0.0;
    for (int sample_index = 0; sample_index < 7; ++sample_index) {
	const double fraction = fractions[sample_index];
	const double parameter = (1.0 - fraction) * first.model_parameter +
	    fraction * second.model_parameter;
	const ON_2dPoint uv(
	    (1.0 - fraction) * first.surface_parameter.x +
		fraction * second.surface_parameter.x,
	    (1.0 - fraction) * first.surface_parameter.y +
		fraction * second.surface_parameter.y);
	const ON_3dPoint lifted = surface->PointAt(uv.x, uv.y);
	const ON_3dPoint model = model_curve.PointAt(parameter);
	if (!lifted.IsValid() || !model.IsValid())
	    return DBL_MAX;
	maximum_error = std::max(maximum_error, lifted.DistanceTo(model));
    }
    return maximum_error;
}


static ON_Curve *
coaxial_synchronized_parameter_curve(const ON_Surface *surface,
	const ON_Curve &model_curve, const ON_Curve &seed_curve,
	double tolerance)
{
    if (surface == NULL || !(tolerance > 0.0) ||
	!model_curve.Domain().IsIncreasing() ||
	seed_curve.Domain() != model_curve.Domain())
    {
	return NULL;
    }

    const ON_Interval domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    const bool closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    if (!domains[0].IsIncreasing() || !domains[1].IsIncreasing())
	return NULL;

    static const int initial_segment_count = 16;
    static const int maximum_refinement_passes = 12;
    static const size_t maximum_sample_count = 8193;
    const double refinement_tolerance = 0.5 * tolerance;
    const ON_Interval model_domain = model_curve.Domain();
    brlcad::PullbackContext context;
    std::vector<CoaxialParameterSample> samples;
    samples.reserve(initial_segment_count + 1);
    for (int sample_index = 0; sample_index <= initial_segment_count;
	 ++sample_index)
    {
	CoaxialParameterSample sample;
	sample.model_parameter = model_domain.ParameterAt(
	    (double)sample_index / initial_segment_count);
	const ON_3dPoint seed = seed_curve.PointAt(sample.model_parameter);
	if (!coaxial_project_surface_parameter(context, surface,
		model_curve.PointAt(sample.model_parameter),
		ON_2dPoint(seed.x, seed.y), closed, domains, tolerance,
		sample.surface_parameter))
	{
	    return NULL;
	}
	samples.push_back(sample);
    }

    bool converged = false;
    for (int refinement = 0; refinement < maximum_refinement_passes;
	 ++refinement)
    {
	std::vector<CoaxialParameterSample> refined;
	refined.reserve(std::min(maximum_sample_count,
	    2 * samples.size() - 1));
	refined.push_back(samples.front());
	bool changed = false;
	for (size_t sample_index = 1; sample_index < samples.size();
	     ++sample_index)
	{
	    const CoaxialParameterSample &first = samples[sample_index - 1];
	    const CoaxialParameterSample &second = samples[sample_index];
	    if (coaxial_parameter_interval_error(surface, model_curve, first,
		    second) > refinement_tolerance)
	    {
		if (refined.size() + samples.size() - sample_index + 1 >
			maximum_sample_count)
		{
		    return NULL;
		}
		CoaxialParameterSample middle;
		middle.model_parameter = 0.5 * (first.model_parameter +
		    second.model_parameter);
		const ON_2dPoint seed(
		    0.5 * (first.surface_parameter.x +
			second.surface_parameter.x),
		    0.5 * (first.surface_parameter.y +
			second.surface_parameter.y));
		if (!coaxial_project_surface_parameter(context, surface,
			model_curve.PointAt(middle.model_parameter), seed,
			closed, domains, tolerance, middle.surface_parameter))
		{
		    return NULL;
		}
		refined.push_back(middle);
		changed = true;
	    }
	    refined.push_back(second);
	}
	samples.swap(refined);
	if (!changed) {
	    converged = true;
	    break;
	}
    }
    if (!converged)
	return NULL;

    ON_3dPointArray parameter_points;
    ON_SimpleArray<double> parameters;
    parameter_points.Reserve(static_cast<int>(samples.size()));
    parameters.Reserve(static_cast<int>(samples.size()));
    for (const CoaxialParameterSample &sample : samples) {
	parameter_points.Append(ON_3dPoint(sample.surface_parameter.x,
	    sample.surface_parameter.y, 0.0));
	parameters.Append(sample.model_parameter);
    }
    std::unique_ptr<ON_PolylineCurve> result(
	new ON_PolylineCurve(parameter_points, parameters));
    if (!result || !result->ChangeDimension(2) || !result->IsValid())
	return NULL;
    return result.release();
}


/* A cylinder coaxial with a surface of revolution intersects it wherever the
 * revolution profile reaches the cylinder radius.  Each isolated profile
 * point sweeps out one complete circle.  Construct those circles directly;
 * general two-dimensional SSI refinement otherwise tends to fragment a
 * tangent circle into short, mutually disconnected curves. */
static bool
append_coaxial_cylinder_revolution_ssi_events_ordered(
	const ON_Surface *cylinder_surface,
	const ON_Surface *revolution_surface, bool cylinder_is_surfaceA,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double cylinder_parameter_tolerance,
	double revolution_parameter_tolerance,
	const ON_Interval *cylinder_udomain,
	const ON_Interval *cylinder_vdomain,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Cylinder cylinder;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(revolution_surface);
    if (cylinder_surface == NULL || revolution == NULL ||
	revolution->m_curve == NULL ||
	!cylinder_surface->IsCylinder(&cylinder,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)) ||
	!cylinder.IsValid())
    {
	return false;
    }
    /* Coincident or disjoint cylinder/cylinder pairs are overlap problems,
     * not isolated profile roots. */
    ON_Cylinder revolution_cylinder;
    if (revolution_surface->IsCylinder(&revolution_cylinder,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)))
    {
	return false;
    }

    RevolutionRefinementGeometry cylinder_geometry;
    RevolutionRefinementGeometry revolution_geometry;
    if (!revolution_refinement_geometry(cylinder_surface, isect_tol,
	    cylinder_geometry) ||
	!revolution_refinement_geometry(revolution_surface, isect_tol,
	    revolution_geometry))
    {
	return false;
    }
    ON_3dVector cylinder_axis = cylinder_geometry.axis.Direction();
    ON_3dVector revolution_axis = revolution_geometry.axis.Direction();
    if (!cylinder_axis.Unitize() || !revolution_axis.Unitize())
	return false;

    const ON_BoundingBox cylinder_bounds = cylinder_surface->BoundingBox();
    const ON_BoundingBox revolution_bounds =
	revolution_surface->BoundingBox();
    double coordinate_scale = 1.0;
    const ON_BoundingBox bounds[2] = {cylinder_bounds, revolution_bounds};
    for (int bounds_index = 0; bounds_index < 2; ++bounds_index) {
	if (!bounds[bounds_index].IsValid())
	    return false;
	for (int axis = 0; axis < 3; ++axis) {
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(bounds[bounds_index].m_min[axis]));
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(bounds[bounds_index].m_max[axis]));
	}
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	isect_tol / std::max(1.0, revolution_bounds.Diagonal().Length()));
    if (ON_CrossProduct(cylinder_axis, revolution_axis).Length() >
	angular_tolerance)
    {
	return false;
    }
    const ON_3dVector axis_offset =
	revolution_geometry.axis.from - cylinder_geometry.axis.from;
    if (ON_CrossProduct(axis_offset, cylinder_axis).Length() > isect_tol)
	return false;
    const int cylinder_profile_direction =
	cylinder_geometry.profile_direction;
    const int cylinder_rotation_direction =
	1 - cylinder_profile_direction;
    const int revolution_profile_direction =
	revolution_geometry.profile_direction;
    const int revolution_rotation_direction =
	1 - revolution_profile_direction;
    if (!cylinder_surface->IsClosed(cylinder_rotation_direction) ||
	!revolution_surface->IsClosed(revolution_rotation_direction))
    {
	return false;
    }

    ON_Interval cylinder_domains[2] = {
	cylinder_udomain ? *cylinder_udomain : cylinder_surface->Domain(0),
	cylinder_vdomain ? *cylinder_vdomain : cylinder_surface->Domain(1)
    };
    ON_Interval revolution_domains[2] = {
	revolution_udomain ? *revolution_udomain :
	    revolution_surface->Domain(0),
	revolution_vdomain ? *revolution_vdomain :
	    revolution_surface->Domain(1)
    };
    for (int direction = 0; direction < 2; ++direction) {
	cylinder_domains[direction].MakeIncreasing();
	revolution_domains[direction].MakeIncreasing();
    }
    ON_Interval cylinder_rotation_domain = cylinder_surface->Domain(
	cylinder_rotation_direction);
    ON_Interval revolution_rotation_domain = revolution_surface->Domain(
	revolution_rotation_direction);
    ON_Interval revolution_profile_domain = revolution->m_curve->Domain();
    cylinder_rotation_domain.MakeIncreasing();
    revolution_rotation_domain.MakeIncreasing();
    revolution_profile_domain.MakeIncreasing();
    if (!cylinder_domains[0].IsIncreasing() ||
	!cylinder_domains[1].IsIncreasing() ||
	!revolution_domains[0].IsIncreasing() ||
	!revolution_domains[1].IsIncreasing() ||
	!cylinder_rotation_domain.IsIncreasing() ||
	!revolution_rotation_domain.IsIncreasing() ||
	!revolution_profile_domain.IsIncreasing() ||
	std::fabs(cylinder_domains[cylinder_rotation_direction].Min() -
	    cylinder_rotation_domain.Min()) > cylinder_parameter_tolerance ||
	std::fabs(cylinder_domains[cylinder_rotation_direction].Max() -
	    cylinder_rotation_domain.Max()) > cylinder_parameter_tolerance ||
	std::fabs(revolution_domains[revolution_rotation_direction].Min() -
	    revolution_rotation_domain.Min()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_domains[revolution_rotation_direction].Max() -
	    revolution_rotation_domain.Max()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_surface->Domain(
	    revolution_profile_direction).Min() -
	    revolution_profile_domain.Min()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_surface->Domain(
	    revolution_profile_direction).Max() -
	    revolution_profile_domain.Max()) >
		revolution_parameter_tolerance)
    {
	return false;
    }

    ON_3dVector meridian_axis;
    ON_3dVector radial_direction;
    if (!revolution_meridian_frame(*revolution, identity_tolerance,
	    meridian_axis, radial_direction))
    {
	return false;
    }
    if (ON_DotProduct(meridian_axis, cylinder_axis) < 0.0)
	cylinder_axis.Reverse();

    const double cylinder_radius = std::fabs(cylinder.circle.Radius());
    if (!(cylinder_radius > ON_ZERO_TOLERANCE) ||
	!std::isfinite(cylinder_radius))
    {
	return false;
    }

    struct ProfileRoot {
	double parameter;
	double radius;
	double axis_parameter;
    };
    std::vector<ProfileRoot> roots;
    const double plane_extent = 2.0 * coordinate_scale;
    for (int side = -1; side <= 1; side += 2) {
	const ON_3dPoint plane_origin = revolution->m_axis.from +
	    (side * cylinder_radius) * radial_direction;
	const ON_Plane radial_plane(plane_origin, radial_direction);
	if (!radial_plane.IsValid())
	    return false;
	ON_PlaneSurface radial_plane_surface(radial_plane);
	if (!radial_plane_surface.SetExtents(0,
		ON_Interval(-plane_extent, plane_extent)) ||
	    !radial_plane_surface.SetExtents(1,
		ON_Interval(-plane_extent, plane_extent)))
	{
	    return false;
	}

	ON_SimpleArray<ON_X_EVENT> profile_events;
	const int event_count = ON_Intersect(revolution->m_curve,
	    &radial_plane_surface, profile_events, isect_tol,
	    2.0 * isect_tol,
	    &revolution_domains[revolution_profile_direction], NULL, NULL);
	if (event_count < 0)
	    return false;
	for (int event_index = 0; event_index < profile_events.Count();
	     ++event_index)
	{
	    const ON_X_EVENT &profile_event = profile_events[event_index];
	    if (profile_event.m_type != ON_X_EVENT::csx_point)
		return false;
	    double parameter = profile_event.m_a[0];
	    if (!parameter_in_domain(parameter,
		    revolution_domains[revolution_profile_direction],
		    revolution_parameter_tolerance))
	    {
		continue;
	    }
	    parameter = std::max(
		revolution_domains[revolution_profile_direction].Min(),
		std::min(
		    revolution_domains[revolution_profile_direction].Max(),
		    parameter));
	    const ON_3dPoint point = revolution->m_curve->PointAt(parameter);
	    if (!point.IsValid())
		return false;
	    const ON_3dVector offset = point - cylinder_geometry.axis.from;
	    const double axis_parameter = ON_DotProduct(offset,
		cylinder_axis);
	    const ON_3dVector radial = offset - cylinder_axis *
		axis_parameter;
	    const double radius = radial.Length();
	    if (!std::isfinite(radius) ||
		std::fabs(radius - cylinder_radius) > isect_tol)
	    {
		continue;
	    }

	    bool duplicate = false;
	    for (const ProfileRoot &root : roots) {
		if (std::fabs(root.radius - radius) <= identity_tolerance &&
		    std::fabs(root.axis_parameter - axis_parameter) <=
			identity_tolerance)
		{
		    duplicate = true;
		    break;
		}
	    }
	    if (!duplicate)
		roots.push_back({parameter, radius, axis_parameter});
	}
    }
    if (roots.empty())
	return false;
    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (const ProfileRoot &root : roots) {
	std::unique_ptr<ON_Curve> model_curve(
	    revolution_surface->IsoCurve(revolution_rotation_direction,
		root.parameter));
	if (!model_curve || !model_curve->IsValid() ||
	    model_curve->Dimension() != 3 || !model_curve->IsClosed())
	{
	    return false;
	}
	const ON_Interval model_domain = model_curve->Domain();
	if (!model_domain.IsIncreasing() ||
	    std::fabs(model_domain.Min() - revolution_rotation_domain.Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(model_domain.Max() - revolution_rotation_domain.Max()) >
		revolution_parameter_tolerance)
	{
	    return false;
	}

	std::string pullback_failure_reason;
	PullbackFailureReason pullback_failure = PullbackFailureReason::None;
	std::unique_ptr<ON_Curve> cylinder_curve(brlcad::pullback_curve(
	    cylinder_surface, model_curve.get(), isect_tol,
	    std::max(isect_tol, ON_ZERO_TOLERANCE),
	    &pullback_failure_reason, &pullback_failure));
	if (!cylinder_curve || cylinder_curve->Dimension() != 2 ||
	    !cylinder_curve->IsValid() ||
	    !parameter_curve_profile_is_in_domain(*cylinder_curve,
		cylinder_profile_direction,
		cylinder_domains[cylinder_profile_direction],
		cylinder_parameter_tolerance))
	{
	    return false;
	}

	const ON_3dPoint projected_start = cylinder_curve->PointAtStart();
	const ON_3dPoint projected_middle = cylinder_curve->PointAt(
	    cylinder_curve->Domain().Mid());
	const ON_3dPoint projected_end = cylinder_curve->PointAtEnd();
	const double rotation_period = cylinder_rotation_domain.Length();
	double cylinder_start =
	    projected_start[cylinder_rotation_direction];
	while (cylinder_start < cylinder_rotation_domain.Min())
	    cylinder_start += rotation_period;
	while (cylinder_start > cylinder_rotation_domain.Max())
	    cylinder_start -= rotation_period;
	if (std::fabs(cylinder_start - cylinder_rotation_domain.Min()) <=
		cylinder_parameter_tolerance)
	{
	    cylinder_start = cylinder_rotation_domain.Min();
	} else if (std::fabs(cylinder_start -
		cylinder_rotation_domain.Max()) <= cylinder_parameter_tolerance)
	{
	    cylinder_start = cylinder_rotation_domain.Max();
	}

	double rotation_delta =
	    projected_end[cylinder_rotation_direction] -
	    projected_start[cylinder_rotation_direction];
	if (std::fabs(rotation_delta) < 0.5 * rotation_period)
	    rotation_delta =
		projected_middle[cylinder_rotation_direction] -
		projected_start[cylinder_rotation_direction];
	const double rotation_sign = rotation_delta < 0.0 ? -1.0 : 1.0;
	std::vector<double> split_fractions;
	split_fractions.push_back(0.0);
	const double seam_fraction = rotation_sign > 0.0 ?
	    (cylinder_rotation_domain.Max() - cylinder_start) /
		rotation_period :
	    (cylinder_start - cylinder_rotation_domain.Min()) /
		rotation_period;
	const double split_fraction_tolerance = std::max(64.0 * ON_EPSILON,
	    cylinder_parameter_tolerance / rotation_period);
	if (seam_fraction > split_fraction_tolerance &&
	    seam_fraction < 1.0 - split_fraction_tolerance)
	{
	    split_fractions.push_back(seam_fraction);
	}
	split_fractions.push_back(1.0);

	ON_ClassArray<ON_SSX_EVENT> root_events;
	for (size_t split_index = 1;
	     split_index < split_fractions.size(); ++split_index)
	{
	    const double first_fraction = split_fractions[split_index - 1];
	    const double second_fraction = split_fractions[split_index];
	    const double first_parameter = model_domain.ParameterAt(
		first_fraction);
	    const double second_parameter = model_domain.ParameterAt(
		second_fraction);
	    std::unique_ptr<ON_Curve> model_segment(
		model_curve->DuplicateCurve());
	    if (!model_segment ||
		((first_fraction > 0.0 || second_fraction < 1.0) &&
		 !model_segment->Trim(ON_Interval(first_parameter,
		     second_parameter))) ||
		!model_segment->IsValid())
	    {
		return false;
	    }

	    double cylinder_first = cylinder_start + rotation_sign *
		rotation_period * first_fraction;
	    double cylinder_second = cylinder_start + rotation_sign *
		rotation_period * second_fraction;
	    double cylinder_middle = 0.5 * (cylinder_first +
		cylinder_second);
	    while (cylinder_middle < cylinder_rotation_domain.Min()) {
		cylinder_first += rotation_period;
		cylinder_second += rotation_period;
		cylinder_middle += rotation_period;
	    }
	    while (cylinder_middle > cylinder_rotation_domain.Max()) {
		cylinder_first -= rotation_period;
		cylinder_second -= rotation_period;
		cylinder_middle -= rotation_period;
	    }
	    if (std::fabs(cylinder_first - cylinder_rotation_domain.Min()) <=
		    cylinder_parameter_tolerance)
		cylinder_first = cylinder_rotation_domain.Min();
	    if (std::fabs(cylinder_first - cylinder_rotation_domain.Max()) <=
		    cylinder_parameter_tolerance)
		cylinder_first = cylinder_rotation_domain.Max();
	    if (std::fabs(cylinder_second - cylinder_rotation_domain.Min()) <=
		    cylinder_parameter_tolerance)
		cylinder_second = cylinder_rotation_domain.Min();
	    if (std::fabs(cylinder_second - cylinder_rotation_domain.Max()) <=
		    cylinder_parameter_tolerance)
		cylinder_second = cylinder_rotation_domain.Max();

	    const double cylinder_profile =
		projected_middle[cylinder_profile_direction];
	    const ON_3dPoint cylinder_uv_start =
		cylinder_rotation_direction == 0 ?
		ON_3dPoint(cylinder_first, cylinder_profile, 0.0) :
		ON_3dPoint(cylinder_profile, cylinder_first, 0.0);
	    const ON_3dPoint cylinder_uv_end =
		cylinder_rotation_direction == 0 ?
		ON_3dPoint(cylinder_second, cylinder_profile, 0.0) :
		ON_3dPoint(cylinder_profile, cylinder_second, 0.0);
	    std::unique_ptr<ON_LineCurve> cylinder_segment(
		new ON_LineCurve(cylinder_uv_start, cylinder_uv_end));

	    const ON_3dPoint revolution_uv_start =
		revolution_rotation_direction == 0 ?
		ON_3dPoint(first_parameter, root.parameter, 0.0) :
		ON_3dPoint(root.parameter, first_parameter, 0.0);
	    const ON_3dPoint revolution_uv_end =
		revolution_rotation_direction == 0 ?
		ON_3dPoint(second_parameter, root.parameter, 0.0) :
		ON_3dPoint(root.parameter, second_parameter, 0.0);
	    std::unique_ptr<ON_LineCurve> revolution_segment(
		new ON_LineCurve(revolution_uv_start, revolution_uv_end));
	    const ON_Interval segment_domain = model_segment->Domain();
	    if (!cylinder_segment || !revolution_segment ||
		!cylinder_segment->ChangeDimension(2) ||
		!revolution_segment->ChangeDimension(2) ||
		!cylinder_segment->SetDomain(segment_domain.Min(),
		    segment_domain.Max()) ||
		!revolution_segment->SetDomain(segment_domain.Min(),
		    segment_domain.Max()) ||
		!cylinder_segment->IsValid() ||
		!revolution_segment->IsValid())
	    {
		return false;
	    }
	    std::unique_ptr<ON_Curve> synchronized_cylinder(
		coaxial_synchronized_parameter_curve(cylinder_surface,
		    *model_segment, *cylinder_segment, isect_tol));
	    if (!synchronized_cylinder ||
		!parameter_curve_profile_is_in_domain(*synchronized_cylinder,
		    cylinder_profile_direction,
		    cylinder_domains[cylinder_profile_direction],
		    cylinder_parameter_tolerance) ||
		!planar_ruled_event_is_coherent(*model_segment,
		    *synchronized_cylinder, *revolution_segment,
		    *cylinder_surface, *revolution_surface, isect_tol))
	    {
		return false;
	    }

	    ON_SSX_EVENT event;
	    ON_Curve *curveA = cylinder_is_surfaceA ?
		synchronized_cylinder.release() : revolution_segment.release();
	    ON_Curve *curveB = cylinder_is_surfaceA ?
		revolution_segment.release() : synchronized_cylinder.release();
	    set_ssx_event_from_curves(event, model_segment.release(), curveA,
		curveB, cylinder_is_surfaceA ? cylinder_surface :
		revolution_surface, cylinder_is_surfaceA ?
		revolution_surface : cylinder_surface);
	    root_events.Append(event);
	    event.m_curve3d = NULL;
	    event.m_curveA = NULL;
	    event.m_curveB = NULL;
	}

	for (int event_index = 0; event_index < root_events.Count();
	     ++event_index)
	{
	    generated_events.Append(root_events[event_index]);
	    root_events[event_index].m_curve3d = NULL;
	    root_events[event_index].m_curveA = NULL;
	    root_events[event_index].m_curveB = NULL;
	}
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    return generated_events.Count() > 0;
}


static bool
append_coaxial_cylinder_revolution_ssi_events(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_coaxial_cylinder_revolution_ssi_events_ordered(
	surfA, surfB, true, events, isect_tol, isect_tolA, isect_tolB,
	surfaceA_udomain, surfaceA_vdomain, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_coaxial_cylinder_revolution_ssi_events_ordered(
	    surfB, surfA, false, events, isect_tol, isect_tolB, isect_tolA,
	    surfaceB_udomain, surfaceB_vdomain, surfaceA_udomain,
	    surfaceA_vdomain);
}


struct ParallelConeCylinderGeometry {
    ON_Cone cone;
    ON_Cylinder cylinder;
    ON_3dVector cone_axis;
    ON_3dVector cylinder_axis;
    double axis_alignment;
    double cylinder_axis_at_cone_apex;
    double cone_height_per_radius;
};


static ON_3dPoint
parallel_cone_cylinder_point(const ParallelConeCylinderGeometry &geometry,
	double angle)
{
    const ON_3dPoint circle_point = geometry.cylinder.PointAt(angle, 0.0);
    const ON_3dVector cone_offset = circle_point - geometry.cone.ApexPoint();
    const ON_3dVector radial_offset = cone_offset - geometry.cone_axis *
	ON_DotProduct(cone_offset, geometry.cone_axis);
    const double cone_height = geometry.cone_height_per_radius *
	radial_offset.Length();
    const double cylinder_height = (cone_height -
	geometry.cylinder_axis_at_cone_apex) / geometry.axis_alignment;
    return geometry.cylinder.PointAt(angle, cylinder_height);
}


static double
parallel_cone_cylinder_chord_error(
	const ParallelConeCylinderGeometry &geometry, double first_angle,
	double second_angle, const ON_3dPoint &first_point,
	const ON_3dPoint &second_point)
{
    static const double fractions[3] = {0.25, 0.5, 0.75};
    double maximum_error = 0.0;
    for (int sample_index = 0; sample_index < 3; ++sample_index) {
	const double fraction = fractions[sample_index];
	const ON_3dPoint point = parallel_cone_cylinder_point(geometry,
	    (1.0 - fraction) * first_angle + fraction * second_angle);
	const ON_3dPoint chord = (1.0 - fraction) * first_point +
	    fraction * second_point;
	if (!point.IsValid() || !chord.IsValid())
	    return DBL_MAX;
	maximum_error = std::max(maximum_error, point.DistanceTo(chord));
    }
    return maximum_error;
}


static bool
append_parallel_cone_cylinder_samples(
	const ParallelConeCylinderGeometry &geometry, double first_angle,
	double second_angle, const ON_3dPoint &first_point,
	const ON_3dPoint &second_point, double tolerance, int depth,
	ON_3dPointArray &points, ON_SimpleArray<double> &parameters)
{
    static const int maximum_depth = 20;
    static const int maximum_sample_count = 8193;
    if (parallel_cone_cylinder_chord_error(geometry, first_angle,
	    second_angle, first_point, second_point) <= tolerance)
    {
	points.Append(second_point);
	parameters.Append(second_angle);
	return true;
    }
    if (depth >= maximum_depth || points.Count() >= maximum_sample_count)
	return false;

    const double middle_angle = 0.5 * (first_angle + second_angle);
    const ON_3dPoint middle_point = parallel_cone_cylinder_point(geometry,
	middle_angle);
    return middle_point.IsValid() &&
	append_parallel_cone_cylinder_samples(geometry, first_angle,
	    middle_angle, first_point, middle_point, tolerance, depth + 1,
	    points, parameters) &&
	append_parallel_cone_cylinder_samples(geometry, middle_angle,
	    second_angle, middle_point, second_point, tolerance, depth + 1,
	    points, parameters);
}


static ON_Curve *
parallel_cone_cylinder_model_curve(
	const ParallelConeCylinderGeometry &geometry, double first_angle,
	double second_angle, double tolerance)
{
    if (!(second_angle > first_angle) || !(tolerance > 0.0))
	return NULL;

    const ON_3dPoint first_point = parallel_cone_cylinder_point(geometry,
	first_angle);
    const ON_3dPoint second_point = parallel_cone_cylinder_point(geometry,
	second_angle);
    ON_3dPointArray points;
    ON_SimpleArray<double> parameters;
    points.Append(first_point);
    parameters.Append(first_angle);
    if (!first_point.IsValid() || !second_point.IsValid() ||
	!append_parallel_cone_cylinder_samples(geometry, first_angle,
	    second_angle, first_point, second_point, 0.25 * tolerance, 0,
	    points, parameters))
    {
	return NULL;
    }

    std::unique_ptr<ON_PolylineCurve> curve(
	new ON_PolylineCurve(points, parameters));
    if (!curve || !curve->IsValid())
	return NULL;
    return curve.release();
}


static ON_Curve *
parallel_cone_cylinder_parameter_seed(const ON_Surface *surface,
	const ON_Curve &model_curve, double tolerance)
{
    if (surface == NULL || !model_curve.Domain().IsIncreasing())
	return NULL;

    const ON_Interval domain = model_curve.Domain();
    ON_3dPointArray points;
    ON_SimpleArray<double> parameters;
    brlcad::PullbackContext context;
    ON_2dPoint previous;
    for (int sample_index = 0; sample_index < 3; ++sample_index) {
	const double parameter = domain.ParameterAt(0.5 * sample_index);
	ON_2dPoint surface_parameter;
	ON_3dPoint lifted_point;
	double distance = DBL_MAX;
	if (!context.SurfaceClosestPoint(surface, model_curve.PointAt(parameter),
		surface_parameter, lifted_point, distance, 0,
		ON_ZERO_TOLERANCE, tolerance) ||
	    !surface_parameter.IsValid() || !lifted_point.IsValid() ||
	    !std::isfinite(distance) || distance > tolerance)
	{
	    return NULL;
	}
	if (sample_index > 0) {
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		if (!(period > ON_ZERO_TOLERANCE))
		    return NULL;
		surface_parameter[direction] += std::round(
		    (previous[direction] - surface_parameter[direction]) /
		    period) * period;
	    }
	}
	points.Append(ON_3dPoint(surface_parameter.x,
	    surface_parameter.y, 0.0));
	parameters.Append(parameter);
	previous = surface_parameter;
    }

    std::unique_ptr<ON_PolylineCurve> seed(
	new ON_PolylineCurve(points, parameters));
    if (!seed || !seed->ChangeDimension(2) || !seed->IsValid())
	return NULL;
    return seed.release();
}


static bool
parallel_cone_cylinder_profile_range(const ON_Surface *surface,
	int profile_direction, const ON_Interval &profile_domain,
	const ON_3dPoint &axis_origin, const ON_3dVector &axis_direction,
	double &minimum_height, double &maximum_height)
{
    if (surface == NULL || !profile_domain.IsIncreasing())
	return false;
    const int rotation_direction = 1 - profile_direction;
    const double rotation_parameter = surface->Domain(rotation_direction).Mid();
    ON_3dPoint parameters[2];
    for (int end = 0; end < 2; ++end) {
	parameters[end][profile_direction] = end ? profile_domain.Max() :
	    profile_domain.Min();
	parameters[end][rotation_direction] = rotation_parameter;
    }
    const ON_3dPoint first = surface->PointAt(parameters[0].x,
	parameters[0].y);
    const ON_3dPoint second = surface->PointAt(parameters[1].x,
	parameters[1].y);
    if (!first.IsValid() || !second.IsValid())
	return false;
    const double first_height = ON_DotProduct(first - axis_origin,
	axis_direction);
    const double second_height = ON_DotProduct(second - axis_origin,
	axis_direction);
    minimum_height = std::min(first_height, second_height);
    maximum_height = std::max(first_height, second_height);
    return std::isfinite(minimum_height) && std::isfinite(maximum_height);
}


static double
normalized_radians(double angle)
{
    angle = std::fmod(angle, 2.0 * ON_PI);
    return angle < 0.0 ? angle + 2.0 * ON_PI : angle;
}


static bool
append_parallel_axis_cone_cylinder_ssi_events_ordered(
	const ON_Surface *cone_surface, const ON_Surface *cylinder_surface,
	bool cone_is_surfaceA, ON_ClassArray<ON_SSX_EVENT> &events,
	double isect_tol, double cone_parameter_tolerance,
	double cylinder_parameter_tolerance,
	const ON_Interval *cone_udomain, const ON_Interval *cone_vdomain,
	const ON_Interval *cylinder_udomain,
	const ON_Interval *cylinder_vdomain)
{
    ParallelConeCylinderGeometry geometry;
    if (cone_surface == NULL || cylinder_surface == NULL ||
	!cone_surface->IsCone(&geometry.cone,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)) ||
	!cylinder_surface->IsCylinder(&geometry.cylinder,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)) ||
	!geometry.cone.IsValid() || !geometry.cylinder.IsValid())
    {
	return false;
    }
    ON_Cylinder cone_cylinder;
    ON_Cone cylinder_cone;
    if (cone_surface->IsCylinder(&cone_cylinder,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)) ||
	cylinder_surface->IsCone(&cylinder_cone,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)))
    {
	return false;
    }

    geometry.cone_axis = geometry.cone.Axis();
    geometry.cylinder_axis = geometry.cylinder.Axis();
    if (!geometry.cone_axis.Unitize() ||
	!geometry.cylinder_axis.Unitize())
    {
	return false;
    }
    geometry.axis_alignment = ON_DotProduct(geometry.cylinder_axis,
	geometry.cone_axis);
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	isect_tol / std::max(1.0,
	    std::max(cone_surface->BoundingBox().Diagonal().Length(),
		cylinder_surface->BoundingBox().Diagonal().Length())));
    if (std::fabs(std::fabs(geometry.axis_alignment) - 1.0) >
	angular_tolerance)
    {
	return false;
    }

    RevolutionRefinementGeometry cylinder_geometry;
    if (!revolution_refinement_geometry(cylinder_surface, isect_tol,
	cylinder_geometry))
    {
	return false;
    }
    const ON_Interval cone_native_domains[2] = {
	cone_surface->Domain(0), cone_surface->Domain(1)
    };
    const ON_Interval cylinder_native_domains[2] = {
	cylinder_surface->Domain(0), cylinder_surface->Domain(1)
    };
    const ON_Interval cone_domains[2] = {
	cone_udomain ? *cone_udomain : cone_native_domains[0],
	cone_vdomain ? *cone_vdomain : cone_native_domains[1]
    };
    const ON_Interval cylinder_domains[2] = {
	cylinder_udomain ? *cylinder_udomain : cylinder_native_domains[0],
	cylinder_vdomain ? *cylinder_vdomain : cylinder_native_domains[1]
    };

    const ON_Interval cone_u_domain = cone_surface->Domain(0);
    const ON_Interval cone_v_domain = cone_surface->Domain(1);
    ON_3dPoint cone_point;
    ON_3dVector cone_derivative_u;
    ON_3dVector cone_derivative_v;
    if (!cone_u_domain.IsIncreasing() || !cone_v_domain.IsIncreasing() ||
	!cone_surface->Ev1Der(cone_u_domain.Mid(), cone_v_domain.Mid(),
	    cone_point, cone_derivative_u, cone_derivative_v) ||
	!cone_derivative_u.Unitize() || !cone_derivative_v.Unitize())
    {
	return false;
    }
    const double cone_axial_u = std::fabs(ON_DotProduct(
	cone_derivative_u, geometry.cone_axis));
    const double cone_axial_v = std::fabs(ON_DotProduct(
	cone_derivative_v, geometry.cone_axis));
    if (std::max(cone_axial_u, cone_axial_v) < 0.5 ||
	std::fabs(cone_axial_u - cone_axial_v) < 0.25)
    {
	return false;
    }
    const int cone_profile_direction =
	cone_axial_u > cone_axial_v ? 0 : 1;
    const int cone_rotation_direction = 1 - cone_profile_direction;
    const int cylinder_profile_direction = cylinder_geometry.profile_direction;
    const int cylinder_rotation_direction = 1 - cylinder_profile_direction;
    if (!cone_surface->IsClosed(cone_rotation_direction) ||
	!cylinder_surface->IsClosed(cylinder_rotation_direction) ||
	std::fabs(cone_domains[cone_rotation_direction].Min() -
	    cone_native_domains[cone_rotation_direction].Min()) >
	    cone_parameter_tolerance ||
	std::fabs(cone_domains[cone_rotation_direction].Max() -
	    cone_native_domains[cone_rotation_direction].Max()) >
	    cone_parameter_tolerance ||
	std::fabs(cylinder_domains[cylinder_rotation_direction].Min() -
	    cylinder_native_domains[cylinder_rotation_direction].Min()) >
	    cylinder_parameter_tolerance ||
	std::fabs(cylinder_domains[cylinder_rotation_direction].Max() -
	    cylinder_native_domains[cylinder_rotation_direction].Max()) >
	    cylinder_parameter_tolerance)
    {
	return false;
    }

    double minimum_cone_height;
    double maximum_cone_height;
    double minimum_cylinder_height;
    double maximum_cylinder_height;
    if (!parallel_cone_cylinder_profile_range(cone_surface,
	    cone_profile_direction, cone_domains[cone_profile_direction],
	    geometry.cone.ApexPoint(), geometry.cone_axis,
	    minimum_cone_height, maximum_cone_height) ||
	!parallel_cone_cylinder_profile_range(cylinder_surface,
	    cylinder_profile_direction,
	    cylinder_domains[cylinder_profile_direction],
	    geometry.cylinder.Center(), geometry.cylinder_axis,
	    minimum_cylinder_height, maximum_cylinder_height))
    {
	return false;
    }

    geometry.cylinder_axis_at_cone_apex = ON_DotProduct(
	geometry.cylinder.Center() - geometry.cone.ApexPoint(),
	geometry.cone_axis);
    geometry.cone_height_per_radius = geometry.cone.height /
	geometry.cone.radius;
    if (!std::isfinite(geometry.cone_height_per_radius) ||
	ZERO(geometry.cone_height_per_radius))
    {
	return false;
    }
    double cylinder_cone_height[2] = {
	geometry.cylinder_axis_at_cone_apex + geometry.axis_alignment *
	    minimum_cylinder_height,
	geometry.cylinder_axis_at_cone_apex + geometry.axis_alignment *
	    maximum_cylinder_height
    };
    if (cylinder_cone_height[0] > cylinder_cone_height[1])
	std::swap(cylinder_cone_height[0], cylinder_cone_height[1]);
    const double minimum_height = std::max(minimum_cone_height,
	cylinder_cone_height[0]);
    const double maximum_height = std::min(maximum_cone_height,
	cylinder_cone_height[1]);
    if (!(maximum_height > minimum_height))
	return false;

    double radial_limits[2] = {
	minimum_height / geometry.cone_height_per_radius,
	maximum_height / geometry.cone_height_per_radius
    };
    if (radial_limits[0] > radial_limits[1])
	std::swap(radial_limits[0], radial_limits[1]);
    if (radial_limits[1] < 0.0)
	return false;
    radial_limits[0] = std::max(0.0, radial_limits[0]);

    const ON_3dVector center_offset = geometry.cylinder.Center() -
	geometry.cone.ApexPoint();
    const ON_3dVector axis_offset = center_offset - geometry.cone_axis *
	ON_DotProduct(center_offset, geometry.cone_axis);
    const double axis_distance = axis_offset.Length();
    const double cylinder_radius = std::fabs(geometry.cylinder.circle.radius);
    if (!(axis_distance > isect_tol) ||
	!(cylinder_radius > ON_ZERO_TOLERANCE))
    {
	return false;
    }
    const double possible_minimum_radius = std::fabs(axis_distance -
	cylinder_radius);
    const double possible_maximum_radius = axis_distance + cylinder_radius;
    radial_limits[0] = std::max(radial_limits[0], possible_minimum_radius);
    radial_limits[1] = std::min(radial_limits[1], possible_maximum_radius);
    if (!(radial_limits[1] > radial_limits[0]))
	return false;

    const ON_3dVector radial_x = geometry.cylinder.circle.plane.xaxis;
    const ON_3dVector radial_y = geometry.cylinder.circle.plane.yaxis;
    const double phase = std::atan2(ON_DotProduct(axis_offset, radial_y),
	ON_DotProduct(axis_offset, radial_x));
    std::vector<double> boundaries;
    boundaries.push_back(0.0);
    boundaries.push_back(2.0 * ON_PI);
    const double radial_denominator = 2.0 * axis_distance * cylinder_radius;
    for (int limit_index = 0; limit_index < 2; ++limit_index) {
	const double radius = radial_limits[limit_index];
	double cosine = (radius * radius - axis_distance * axis_distance -
	    cylinder_radius * cylinder_radius) / radial_denominator;
	if (cosine <= -1.0 || cosine >= 1.0)
	    continue;
	cosine = std::max(-1.0, std::min(1.0, cosine));
	const double offset = std::acos(cosine);
	boundaries.push_back(normalized_radians(phase - offset));
	boundaries.push_back(normalized_radians(phase + offset));
    }
    std::sort(boundaries.begin(), boundaries.end());
    const double angular_merge_tolerance = 1024.0 * ON_EPSILON;
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end(),
	[angular_merge_tolerance](double first, double second) {
	    return std::fabs(first - second) <= angular_merge_tolerance;
	}), boundaries.end());

    std::vector<ON_Interval> accepted;
    for (size_t boundary_index = 1; boundary_index < boundaries.size();
	 ++boundary_index)
    {
	const double first = boundaries[boundary_index - 1];
	const double second = boundaries[boundary_index];
	const ON_3dPoint middle = parallel_cone_cylinder_point(geometry,
	    0.5 * (first + second));
	const double height = ON_DotProduct(middle - geometry.cone.ApexPoint(),
	    geometry.cone_axis);
	if (middle.IsValid() && height >= minimum_height - isect_tol &&
	    height <= maximum_height + isect_tol)
	{
	    accepted.push_back(ON_Interval(first, second));
	}
    }
    if (accepted.empty())
	return false;
    if (accepted.size() > 1 &&
	ZERO(accepted.front().Min()) &&
	EQUAL(accepted.back().Max(), 2.0 * ON_PI))
    {
	accepted.front().Set(accepted.back().Min(),
	    accepted.front().Max() + 2.0 * ON_PI);
	accepted.pop_back();
    }

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (const ON_Interval &angle_domain : accepted) {
	std::unique_ptr<ON_Curve> model_curve(
	    parallel_cone_cylinder_model_curve(geometry,
		angle_domain.Min(), angle_domain.Max(), isect_tol));
	if (!model_curve)
	    return false;
	std::unique_ptr<ON_Curve> cone_seed(
	    parallel_cone_cylinder_parameter_seed(cone_surface,
		*model_curve, isect_tol));
	std::unique_ptr<ON_Curve> cylinder_seed(
	    parallel_cone_cylinder_parameter_seed(cylinder_surface,
		*model_curve, isect_tol));
	if (!cone_seed || !cylinder_seed)
	    return false;
	std::unique_ptr<ON_Curve> cone_curve(
	    coaxial_synchronized_parameter_curve(cone_surface, *model_curve,
		*cone_seed, isect_tol));
	std::unique_ptr<ON_Curve> cylinder_curve(
	    coaxial_synchronized_parameter_curve(cylinder_surface,
		*model_curve, *cylinder_seed, isect_tol));
	if (!cone_curve || !cylinder_curve ||
	    !parameter_curve_profile_is_in_domain(*cone_curve,
		cone_profile_direction,
		cone_domains[cone_profile_direction],
		cone_parameter_tolerance) ||
	    !parameter_curve_profile_is_in_domain(*cylinder_curve,
		cylinder_profile_direction,
		cylinder_domains[cylinder_profile_direction],
		cylinder_parameter_tolerance) ||
	    !planar_ruled_event_is_coherent(*model_curve, *cone_curve,
		*cylinder_curve, *cone_surface, *cylinder_surface, isect_tol))
	{
	    return false;
	}

	ON_SSX_EVENT event;
	ON_Curve *curveA = cone_is_surfaceA ? cone_curve.release() :
	    cylinder_curve.release();
	ON_Curve *curveB = cone_is_surfaceA ? cylinder_curve.release() :
	    cone_curve.release();
	set_ssx_event_from_curves(event, model_curve.release(), curveA, curveB,
	    cone_is_surfaceA ? cone_surface : cylinder_surface,
	    cone_is_surfaceA ? cylinder_surface : cone_surface);
	generated_events.Append(event);
	event.m_curve3d = NULL;
	event.m_curveA = NULL;
	event.m_curveB = NULL;
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    return generated_events.Count() > 0;
}


static bool
append_parallel_axis_cone_cylinder_ssi_events(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_parallel_axis_cone_cylinder_ssi_events_ordered(
	surfA, surfB, true, events, isect_tol, isect_tolA, isect_tolB,
	surfaceA_udomain, surfaceA_vdomain, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_parallel_axis_cone_cylinder_ssi_events_ordered(
	    surfB, surfA, false, events, isect_tol, isect_tolB, isect_tolA,
	    surfaceB_udomain, surfaceB_vdomain, surfaceA_udomain,
	    surfaceA_vdomain);
}


/* A coaxial cone and surface of revolution intersect at profile points on
 * either cone generator.  Each isolated point away from the apex sweeps one
 * circle.  This exact reduction avoids the artificial fixed-parameter drift
 * produced when general SSI refinement follows a full-period circle. */
static bool
append_coaxial_cone_revolution_ssi_events_ordered(
	const ON_Surface *cone_surface,
	const ON_Surface *revolution_surface, bool cone_is_surfaceA,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double cone_parameter_tolerance,
	double revolution_parameter_tolerance,
	const ON_Interval *cone_udomain,
	const ON_Interval *cone_vdomain,
	const ON_Interval *revolution_udomain,
	const ON_Interval *revolution_vdomain)
{
    ON_Cone cone;
    const ON_RevSurface *revolution =
	ON_RevSurface::Cast(revolution_surface);
    if (cone_surface == NULL || revolution == NULL ||
	revolution->m_curve == NULL ||
	!cone_surface->IsCone(&cone,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)) || !cone.IsValid())
    {
	return false;
    }
    ON_Cone revolution_cone;
    if (revolution_surface->IsCone(&revolution_cone,
	    std::max(isect_tol, ON_ZERO_TOLERANCE)))
    {
	return false;
    }

    RevolutionRefinementGeometry revolution_geometry;

    if (!revolution_refinement_geometry(revolution_surface, isect_tol,
	    revolution_geometry))
    {
	return false;
    }
    ON_3dVector cone_axis = cone.Axis();
    ON_3dVector revolution_axis = revolution_geometry.axis.Direction();
    if (!cone_axis.Unitize() || !revolution_axis.Unitize())
	return false;

    const ON_BoundingBox cone_bounds = cone_surface->BoundingBox();
    const ON_BoundingBox revolution_bounds =
	revolution_surface->BoundingBox();
    double coordinate_scale = 1.0;
    const ON_BoundingBox bounds[2] = {cone_bounds, revolution_bounds};
    for (int bounds_index = 0; bounds_index < 2; ++bounds_index) {
	if (!bounds[bounds_index].IsValid())
	    return false;
	for (int axis = 0; axis < 3; ++axis) {
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(bounds[bounds_index].m_min[axis]));
	    coordinate_scale = std::max(coordinate_scale,
		std::fabs(bounds[bounds_index].m_max[axis]));
	}
    }
    const double identity_tolerance = std::min(isect_tol,
	std::max(ON_ZERO_TOLERANCE,
	    1024.0 * ON_EPSILON * coordinate_scale));
    const double angular_tolerance = std::max(1024.0 * ON_EPSILON,
	isect_tol / std::max(1.0, revolution_bounds.Diagonal().Length()));
    if (ON_CrossProduct(cone_axis, revolution_axis).Length() >
	angular_tolerance ||
	ON_CrossProduct(revolution_geometry.axis.from - cone.ApexPoint(),
	    cone_axis).Length() > isect_tol)
    {
	return false;
    }

    const ON_Interval cone_u_domain = cone_surface->Domain(0);
    const ON_Interval cone_v_domain = cone_surface->Domain(1);
    ON_3dPoint cone_point;
    ON_3dVector cone_derivative_u;
    ON_3dVector cone_derivative_v;
    if (!cone_u_domain.IsIncreasing() || !cone_v_domain.IsIncreasing() ||
	!cone_surface->Ev1Der(cone_u_domain.Mid(), cone_v_domain.Mid(),
	    cone_point, cone_derivative_u, cone_derivative_v) ||
	!cone_derivative_u.Unitize() || !cone_derivative_v.Unitize())
    {
	return false;
    }
    const double cone_axial_u = std::fabs(ON_DotProduct(
	cone_derivative_u, cone_axis));
    const double cone_axial_v = std::fabs(ON_DotProduct(
	cone_derivative_v, cone_axis));
    if (std::max(cone_axial_u, cone_axial_v) < 0.5 ||
	std::fabs(cone_axial_u - cone_axial_v) < 0.25)
    {
	return false;
    }
    const int cone_profile_direction =
	cone_axial_u > cone_axial_v ? 0 : 1;
    const int cone_rotation_direction = 1 - cone_profile_direction;
    const int revolution_profile_direction =
	revolution_geometry.profile_direction;
    const int revolution_rotation_direction =
	1 - revolution_profile_direction;
    if (!cone_surface->IsClosed(cone_rotation_direction) ||
	!revolution_surface->IsClosed(revolution_rotation_direction))
    {
	return false;
    }

    ON_Interval cone_domains[2] = {
	cone_udomain ? *cone_udomain : cone_surface->Domain(0),
	cone_vdomain ? *cone_vdomain : cone_surface->Domain(1)
    };
    ON_Interval revolution_domains[2] = {
	revolution_udomain ? *revolution_udomain :
	    revolution_surface->Domain(0),
	revolution_vdomain ? *revolution_vdomain :
	    revolution_surface->Domain(1)
    };
    for (int direction = 0; direction < 2; ++direction) {
	cone_domains[direction].MakeIncreasing();
	revolution_domains[direction].MakeIncreasing();
    }
    ON_Interval cone_rotation_domain = cone_surface->Domain(
	cone_rotation_direction);
    ON_Interval revolution_rotation_domain = revolution_surface->Domain(
	revolution_rotation_direction);
    ON_Interval revolution_profile_domain = revolution->m_curve->Domain();
    cone_rotation_domain.MakeIncreasing();
    revolution_rotation_domain.MakeIncreasing();
    revolution_profile_domain.MakeIncreasing();
    if (!cone_domains[0].IsIncreasing() ||
	!cone_domains[1].IsIncreasing() ||
	!revolution_domains[0].IsIncreasing() ||
	!revolution_domains[1].IsIncreasing() ||
	!cone_rotation_domain.IsIncreasing() ||
	!revolution_rotation_domain.IsIncreasing() ||
	!revolution_profile_domain.IsIncreasing() ||
	std::fabs(cone_domains[cone_rotation_direction].Min() -
	    cone_rotation_domain.Min()) > cone_parameter_tolerance ||
	std::fabs(cone_domains[cone_rotation_direction].Max() -
	    cone_rotation_domain.Max()) > cone_parameter_tolerance ||
	std::fabs(revolution_domains[revolution_rotation_direction].Min() -
	    revolution_rotation_domain.Min()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_domains[revolution_rotation_direction].Max() -
	    revolution_rotation_domain.Max()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_surface->Domain(
	    revolution_profile_direction).Min() -
	    revolution_profile_domain.Min()) >
		revolution_parameter_tolerance ||
	std::fabs(revolution_surface->Domain(
	    revolution_profile_direction).Max() -
	    revolution_profile_domain.Max()) >
		revolution_parameter_tolerance)
    {
	return false;
    }

    ON_3dVector meridian_axis;
    ON_3dVector radial_direction;
    if (!revolution_meridian_frame(*revolution, identity_tolerance,
	    meridian_axis, radial_direction))
    {
	return false;
    }
    double cone_slope = cone.radius / cone.height;
    if (ON_DotProduct(meridian_axis, cone_axis) < 0.0) {
	cone_axis.Reverse();
	cone_slope = -cone_slope;
    }
    if (!std::isfinite(cone_slope) || ZERO(cone_slope))
	return false;

    struct ProfileRoot {
	double parameter;
	double radius;
	double axis_parameter;
    };
    std::vector<ProfileRoot> roots;
    const double plane_extent = 2.0 * coordinate_scale;
    for (int side = -1; side <= 1; side += 2) {
	ON_3dVector generator_plane_normal = radial_direction -
	    (side * cone_slope) * cone_axis;
	if (!generator_plane_normal.Unitize())
	    return false;
	const ON_Plane generator_plane(cone.ApexPoint(),
	    generator_plane_normal);
	if (!generator_plane.IsValid())
	    return false;
	ON_PlaneSurface generator_plane_surface(generator_plane);
	if (!generator_plane_surface.SetExtents(0,
		ON_Interval(-plane_extent, plane_extent)) ||
	    !generator_plane_surface.SetExtents(1,
		ON_Interval(-plane_extent, plane_extent)))
	{
	    return false;
	}

	ON_SimpleArray<ON_X_EVENT> profile_events;
	const int event_count = ON_Intersect(revolution->m_curve,
	    &generator_plane_surface, profile_events, isect_tol,
	    2.0 * isect_tol,
	    &revolution_domains[revolution_profile_direction], NULL, NULL);
	if (event_count < 0)
	    return false;
	for (int event_index = 0; event_index < profile_events.Count();
	     ++event_index)
	{
	    const ON_X_EVENT &profile_event = profile_events[event_index];
	    if (profile_event.m_type != ON_X_EVENT::csx_point)
		return false;
	    double parameter = profile_event.m_a[0];
	    if (!parameter_in_domain(parameter,
		    revolution_domains[revolution_profile_direction],
		    revolution_parameter_tolerance))
	    {
		continue;
	    }
	    parameter = std::max(
		revolution_domains[revolution_profile_direction].Min(),
		std::min(
		    revolution_domains[revolution_profile_direction].Max(),
		    parameter));
	    const ON_3dPoint point = revolution->m_curve->PointAt(parameter);
	    if (!point.IsValid())
		return false;
	    const ON_3dVector offset = point - cone.ApexPoint();
	    const double axis_parameter = ON_DotProduct(offset, cone_axis);
	    const ON_3dVector radial = offset - cone_axis * axis_parameter;
	    const double radius = radial.Length();
	    if (!std::isfinite(radius) || radius <= identity_tolerance ||
		std::fabs(radius - std::fabs(cone_slope * axis_parameter)) >
		    isect_tol)
	    {
		continue;
	    }

	    bool duplicate = false;
	    for (const ProfileRoot &root : roots) {
		if (std::fabs(root.radius - radius) <= identity_tolerance &&
		    std::fabs(root.axis_parameter - axis_parameter) <=
			identity_tolerance)
		{
		    duplicate = true;
		    break;
		}
	    }
	    if (!duplicate)
		roots.push_back({parameter, radius, axis_parameter});
	}
    }
    if (roots.empty())
	return false;

    ON_ClassArray<ON_SSX_EVENT> generated_events;
    for (const ProfileRoot &root : roots) {
	std::unique_ptr<ON_Curve> model_curve(
	    revolution_surface->IsoCurve(revolution_rotation_direction,
		root.parameter));
	if (!model_curve || !model_curve->IsValid() ||
	    model_curve->Dimension() != 3 || !model_curve->IsClosed())
	{
	    return false;
	}
	const ON_Interval model_domain = model_curve->Domain();
	if (!model_domain.IsIncreasing() ||
	    std::fabs(model_domain.Min() - revolution_rotation_domain.Min()) >
		revolution_parameter_tolerance ||
	    std::fabs(model_domain.Max() - revolution_rotation_domain.Max()) >
		revolution_parameter_tolerance)
	{
	    return false;
	}

	std::string pullback_failure_reason;
	PullbackFailureReason pullback_failure = PullbackFailureReason::None;
	std::unique_ptr<ON_Curve> cone_curve(brlcad::pullback_curve(
	    cone_surface, model_curve.get(), isect_tol,
	    std::max(isect_tol, ON_ZERO_TOLERANCE),
	    &pullback_failure_reason, &pullback_failure));
	if (!cone_curve || cone_curve->Dimension() != 2 ||
	    !cone_curve->IsValid() ||
	    !parameter_curve_profile_is_in_domain(*cone_curve,
		cone_profile_direction, cone_domains[cone_profile_direction],
		cone_parameter_tolerance))
	{
	    return false;
	}

	const ON_3dPoint projected_start = cone_curve->PointAtStart();
	const ON_3dPoint projected_middle = cone_curve->PointAt(
	    cone_curve->Domain().Mid());
	const ON_3dPoint projected_end = cone_curve->PointAtEnd();
	const double rotation_period = cone_rotation_domain.Length();
	double cone_start = projected_start[cone_rotation_direction];
	while (cone_start < cone_rotation_domain.Min())
	    cone_start += rotation_period;
	while (cone_start > cone_rotation_domain.Max())
	    cone_start -= rotation_period;
	if (std::fabs(cone_start - cone_rotation_domain.Min()) <=
		cone_parameter_tolerance)
	{
	    cone_start = cone_rotation_domain.Min();
	} else if (std::fabs(cone_start - cone_rotation_domain.Max()) <=
		cone_parameter_tolerance)
	{
	    cone_start = cone_rotation_domain.Max();
	}

	double rotation_delta = projected_end[cone_rotation_direction] -
	    projected_start[cone_rotation_direction];
	if (std::fabs(rotation_delta) < 0.5 * rotation_period)
	    rotation_delta = projected_middle[cone_rotation_direction] -
		projected_start[cone_rotation_direction];
	const double rotation_sign = rotation_delta < 0.0 ? -1.0 : 1.0;
	std::vector<double> split_fractions;
	split_fractions.push_back(0.0);
	const double seam_fraction = rotation_sign > 0.0 ?
	    (cone_rotation_domain.Max() - cone_start) / rotation_period :
	    (cone_start - cone_rotation_domain.Min()) / rotation_period;
	const double split_fraction_tolerance = std::max(64.0 * ON_EPSILON,
	    cone_parameter_tolerance / rotation_period);
	if (seam_fraction > split_fraction_tolerance &&
	    seam_fraction < 1.0 - split_fraction_tolerance)
	{
	    split_fractions.push_back(seam_fraction);
	}
	split_fractions.push_back(1.0);

	ON_ClassArray<ON_SSX_EVENT> root_events;
	for (size_t split_index = 1;
	     split_index < split_fractions.size(); ++split_index)
	{
	    const double first_fraction = split_fractions[split_index - 1];
	    const double second_fraction = split_fractions[split_index];
	    const double first_parameter = model_domain.ParameterAt(
		first_fraction);
	    const double second_parameter = model_domain.ParameterAt(
		second_fraction);
	    std::unique_ptr<ON_Curve> model_segment(
		model_curve->DuplicateCurve());
	    if (!model_segment ||
		((first_fraction > 0.0 || second_fraction < 1.0) &&
		 !model_segment->Trim(ON_Interval(first_parameter,
		     second_parameter))) || !model_segment->IsValid())
	    {
		return false;
	    }

	    double cone_first = cone_start + rotation_sign *
		rotation_period * first_fraction;
	    double cone_second = cone_start + rotation_sign *
		rotation_period * second_fraction;
	    double cone_middle = 0.5 * (cone_first + cone_second);
	    while (cone_middle < cone_rotation_domain.Min()) {
		cone_first += rotation_period;
		cone_second += rotation_period;
		cone_middle += rotation_period;
	    }
	    while (cone_middle > cone_rotation_domain.Max()) {
		cone_first -= rotation_period;
		cone_second -= rotation_period;
		cone_middle -= rotation_period;
	    }
	    if (std::fabs(cone_first - cone_rotation_domain.Min()) <=
		    cone_parameter_tolerance)
		cone_first = cone_rotation_domain.Min();
	    if (std::fabs(cone_first - cone_rotation_domain.Max()) <=
		    cone_parameter_tolerance)
		cone_first = cone_rotation_domain.Max();
	    if (std::fabs(cone_second - cone_rotation_domain.Min()) <=
		    cone_parameter_tolerance)
		cone_second = cone_rotation_domain.Min();
	    if (std::fabs(cone_second - cone_rotation_domain.Max()) <=
		    cone_parameter_tolerance)
		cone_second = cone_rotation_domain.Max();

	    const double cone_profile =
		projected_middle[cone_profile_direction];
	    const ON_3dPoint cone_uv_start =
		cone_rotation_direction == 0 ?
		ON_3dPoint(cone_first, cone_profile, 0.0) :
		ON_3dPoint(cone_profile, cone_first, 0.0);
	    const ON_3dPoint cone_uv_end =
		cone_rotation_direction == 0 ?
		ON_3dPoint(cone_second, cone_profile, 0.0) :
		ON_3dPoint(cone_profile, cone_second, 0.0);
	    std::unique_ptr<ON_LineCurve> cone_seed(
		new ON_LineCurve(cone_uv_start, cone_uv_end));

	    const ON_3dPoint revolution_uv_start =
		revolution_rotation_direction == 0 ?
		ON_3dPoint(first_parameter, root.parameter, 0.0) :
		ON_3dPoint(root.parameter, first_parameter, 0.0);
	    const ON_3dPoint revolution_uv_end =
		revolution_rotation_direction == 0 ?
		ON_3dPoint(second_parameter, root.parameter, 0.0) :
		ON_3dPoint(root.parameter, second_parameter, 0.0);
	    std::unique_ptr<ON_LineCurve> revolution_curve(
		new ON_LineCurve(revolution_uv_start, revolution_uv_end));
	    const ON_Interval segment_domain = model_segment->Domain();
	    if (!cone_seed || !revolution_curve ||
		!cone_seed->ChangeDimension(2) ||
		!revolution_curve->ChangeDimension(2) ||
		!cone_seed->SetDomain(segment_domain.Min(),
		    segment_domain.Max()) ||
		!revolution_curve->SetDomain(segment_domain.Min(),
		    segment_domain.Max()) ||
		!cone_seed->IsValid() || !revolution_curve->IsValid())
	    {
		return false;
	    }
	    std::unique_ptr<ON_Curve> synchronized_cone(
		coaxial_synchronized_parameter_curve(cone_surface,
		    *model_segment, *cone_seed, isect_tol));
	    if (!synchronized_cone ||
		!parameter_curve_profile_is_in_domain(*synchronized_cone,
		    cone_profile_direction,
		    cone_domains[cone_profile_direction],
		    cone_parameter_tolerance) ||
		!parameter_curve_profile_is_in_domain(*synchronized_cone,
		    cone_rotation_direction,
		    cone_domains[cone_rotation_direction],
		    cone_parameter_tolerance) ||
		!planar_ruled_event_is_coherent(*model_segment,
		    *synchronized_cone, *revolution_curve, *cone_surface,
		    *revolution_surface, isect_tol))
	    {
		return false;
	    }

	    ON_SSX_EVENT event;
	    ON_Curve *curveA = cone_is_surfaceA ?
		synchronized_cone.release() : revolution_curve.release();
	    ON_Curve *curveB = cone_is_surfaceA ?
		revolution_curve.release() : synchronized_cone.release();
	    set_ssx_event_from_curves(event, model_segment.release(), curveA,
		curveB, cone_is_surfaceA ? cone_surface : revolution_surface,
		cone_is_surfaceA ? revolution_surface : cone_surface);
	    root_events.Append(event);
	    event.m_curve3d = NULL;
	    event.m_curveA = NULL;
	    event.m_curveB = NULL;
	}

	for (int event_index = 0; event_index < root_events.Count();
	     ++event_index)
	{
	    generated_events.Append(root_events[event_index]);
	    root_events[event_index].m_curve3d = NULL;
	    root_events[event_index].m_curveA = NULL;
	    root_events[event_index].m_curveB = NULL;
	}
    }

    for (int event_index = 0; event_index < generated_events.Count();
	 ++event_index)
    {
	events.Append(generated_events[event_index]);
	generated_events[event_index].m_curve3d = NULL;
	generated_events[event_index].m_curveA = NULL;
	generated_events[event_index].m_curveB = NULL;
    }
    return generated_events.Count() > 0;
}


static bool
append_coaxial_cone_revolution_ssi_events(
	const ON_Surface *surfA, const ON_Surface *surfB,
	ON_ClassArray<ON_SSX_EVENT> &events, double isect_tol,
	double isect_tolA, double isect_tolB,
	const ON_Interval *surfaceA_udomain,
	const ON_Interval *surfaceA_vdomain,
	const ON_Interval *surfaceB_udomain,
	const ON_Interval *surfaceB_vdomain)
{
    return append_coaxial_cone_revolution_ssi_events_ordered(
	surfA, surfB, true, events, isect_tol, isect_tolA, isect_tolB,
	surfaceA_udomain, surfaceA_vdomain, surfaceB_udomain,
	surfaceB_vdomain) ||
	append_coaxial_cone_revolution_ssi_events_ordered(
	    surfB, surfA, false, events, isect_tol, isect_tolB, isect_tolA,
	    surfaceB_udomain, surfaceB_vdomain, surfaceA_udomain,
	    surfaceA_vdomain);
}


// Find curves that contain the boundaries of overlap regions.
//
// The portion of a NURBS surface inside a span defined by two pairs
// of strictly increasing knots (one in each of the u and v
// directions) is a rational Bezier surface [1].
//
// Thus a NURBS surface can be decomposed into Bezier patches whose
// boundaries coincide with the isocurves at the surface's strictly
// increasing (Bezier) knots.
//
// When two Bezier patches overlap, the boundary of the overlap region
// is formed completely from the boundaries of the patches [2].
//
// Therefore, the boundaries of the overlap regions between two NURBS
// surfaces are formed completely from the isocurves at the surfaces'
// Bezier knots. More narrowly, the subcurves of said isocurves which
// overlap the opposing surface contain all subcurves needed to bound
// the overlap regions.
//
// 1. See ON_NurbsSurface::ConvertSpanToBezier().
// 2. Theorem 3 in
// http://libgen.org/scimag1/10.1016/S0010-4485%252896%252900099-1.pdf
static void
find_overlap_boundary_curves(
    ON_SimpleArray<OverlapSegment *> &overlaps,
    ON_ClassArray<ON_SSX_EVENT> &csx_events,
    ON_3dPointArray &isocurve_3d,
    ON_2dPointArray &isocurveA_2d,
    ON_2dPointArray &isocurveB_2d,
    const ON_Surface *surfA,
    const ON_Surface *surfB,
    Subsurface *treeA,
    Subsurface *treeB,
    double isect_tol,
    double isect_tolA,
    double isect_tolB,
    double overlap_tol)
{
    for (int i = 0; i < 4; i++) {
	bool is_surfA_iso = i < 2;
	int knot_dir = i % 2;
	int surf_dir = 1 - knot_dir;

	const ON_Surface *surf1 = is_surfA_iso ? surfA : surfB;
	const ON_Surface *surf2 = is_surfA_iso ? surfB : surfA;
	double isect_tol1 = is_surfA_iso ? isect_tolA : isect_tolB;
	Subsurface *tree2 = is_surfA_iso ? treeB : treeA;

	ON_2dPointArray &isocurve1_2d = is_surfA_iso ? isocurveA_2d :
	    isocurveB_2d;

	ON_2dPointArray &isocurve2_2d = is_surfA_iso ? isocurveB_2d :
	    isocurveA_2d;

	std::vector<double> surf1_bezier_knots;
	surf1_bezier_knots = get_bezier_knots(surf1, surf_dir);

	for (size_t j = 0; j < surf1_bezier_knots.size(); ++j) {
	    SurfaceKnot surf1_knot(knot_dir, surf1_bezier_knots, j);
	    ON_Curve *surf1_isocurve = surf1->IsoCurve(surf1_knot.dir,
						       surf1_knot.c);

	    ON_SimpleArray<ON_X_EVENT> events;
	    ON_CurveArray overlap2d;
	    ON_Intersect(surf1_isocurve, surf2, events, isect_tol,
			 overlap_tol, 0, 0, 0, &overlap2d);

	    //dplot->IsoCSX(events, surf1_isocurve, is_surfA_iso);
	    //dplot->WriteLog();

	    append_csx_event_points(isocurve_3d, isocurve2_2d, events);

	    for (int k = 0; k < events.Count(); k++) {
		ON_X_EVENT &event = events[k];

		KnotIsocurveSource iso_src(is_surfA_iso, surf1, surf1_knot);
		OverlapIsocurve iso(iso_src, surf1_isocurve, event.m_a[0],
				    event.m_a[1]);

		isocurve1_2d.Append(iso.overlap_uv[0]);

		if (event.m_type == ON_X_EVENT::csx_overlap) {
		    isocurve1_2d.Append(iso.overlap_uv[1]);
		    if (k >= overlap2d.Count() || overlap2d[k] == NULL) {
			throw GeometryGenerationError(
			    "curve-surface overlap is missing its parameter curve\n");
		    }

		    bool curve_on_overlap_boundary =
			is_curve_on_overlap_boundary(iso, surf1, surf2, tree2,
						     isect_tol, isect_tol1);

		    if (curve_on_overlap_boundary) {
			append_overlap_segments(overlaps, iso, overlap2d[k],
						surf1);
		    } else {
			// the intersection isn't part of a
			// surface-surface overlap, but we want it all
			// the same
			ON_SimpleArray<OverlapSegment *> tmp_overlaps;

			append_overlap_segments(tmp_overlaps, iso, overlap2d[k], surf1);

			for (int l = 0; l < tmp_overlaps.Count(); ++l) {
			    ON_SSX_EVENT ssx_event;

			    int ret = set_ssx_event_from_curves(ssx_event,
				    tmp_overlaps[l]->m_curve3d,
				    tmp_overlaps[l]->m_curveA,
				    tmp_overlaps[l]->m_curveB, surfA, surfB);
			    if (ret != 0) {
				bu_log("warning: reverse failed.");
			    }
			    csx_events.Append(ssx_event);

			    // set the curves to NULL so they aren't
			    // deleted by destructors
			    tmp_overlaps[l]->m_curve3d = NULL;
			    tmp_overlaps[l]->m_curveA = NULL;
			    tmp_overlaps[l]->m_curveB = NULL;

			    ssx_event.m_curve3d = NULL;
			    ssx_event.m_curveA = NULL;
			    ssx_event.m_curveB = NULL;
			}
		    }
		}
	    }
	    delete surf1_isocurve;
	}
    }
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
    IntersectionBudgetScope budget_scope;
    if (budget_scope.IsBudgetExceeded()) {
	return budget_scope.BudgetExceededResult();
    }

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
    V_MAX(fitting_tol, isect_tol);

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

    if (append_affine_planar_ssi_event(surfA, surfB, x, isect_tol,
	    isect_tolA, isect_tolB))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_planar_ruled_ssi_event(surfA, surfB, x, isect_tol,
	    isect_tolA, isect_tolB, surfaceA_udomain, surfaceA_vdomain,
	    surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_planar_revolution_ssi_event(surfA, surfB, x, isect_tol,
	    isect_tolA, isect_tolB, surfaceA_udomain, surfaceA_vdomain,
	    surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_parallel_spheroid_revolution_ssi_events(surfA, surfB, x,
	    isect_tol, isect_tolA, isect_tolB, surfaceA_udomain,
	    surfaceA_vdomain, surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_coaxial_cylinder_revolution_ssi_events(surfA, surfB, x,
	    isect_tol, isect_tolA, isect_tolB, surfaceA_udomain,
	    surfaceA_vdomain, surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_parallel_axis_cone_cylinder_ssi_events(surfA, surfB, x,
	    isect_tol, isect_tolA, isect_tolB, surfaceA_udomain,
	    surfaceA_vdomain, surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    if (append_coaxial_cone_revolution_ssi_events(surfA, surfB, x,
	    isect_tol, isect_tolA, isect_tolB, surfaceA_udomain,
	    surfaceA_vdomain, surfaceB_udomain, surfaceB_vdomain))
    {
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return x.Count() - original_count;
    }

    /* An affine planar patch is represented exactly by its two corner
     * triangles at every scale.  Keeping that patch intact while the other
     * surface is refined avoids multiplying every candidate by four without
     * improving either its bounding box or the eventual Newton seed.  This
     * matters in particular for small planar pipe caps tested against long,
     * narrow ruled pipe surfaces. */
    AffinePlanarSurface affine_mapping;
    const bool affine_planar_A = affine_planar_surface_mapping(
	rootA->m_surf, affine_mapping);
    const bool affine_planar_B = affine_planar_surface_mapping(
	rootB->m_surf, affine_mapping);
    const int shallow_revolution_direction_A =
	nearly_linear_revolution_profile_direction(rootA->m_surf, isect_tol);
    const int shallow_revolution_direction_B =
	nearly_linear_revolution_profile_direction(rootB->m_surf, isect_tol);
    int coaxial_profile_direction_A = -1;
    int coaxial_profile_direction_B = -1;
    ON_Line coaxial_axis;
    const bool coaxial_revolution_pair =
	coaxial_revolution_refinement_directions(rootA->m_surf, rootB->m_surf,
	    isect_tol, coaxial_profile_direction_A,
	    coaxial_profile_direction_B, coaxial_axis);
    int parallel_profile_direction_A = -1;
    int parallel_profile_direction_B = -1;
    ON_Line parallel_axis_A;
    ON_Line parallel_axis_B;
    const bool parallel_revolution_pair = !coaxial_revolution_pair &&
	parallel_revolution_refinement_directions(rootA->m_surf,
	    rootB->m_surf, isect_tol, parallel_profile_direction_A,
	    parallel_profile_direction_B, parallel_axis_A, parallel_axis_B);

    ON_3dPointArray curvept, tmp_curvept;
    ON_2dPointArray curve_uvA, curve_uvB, tmp_curve_uvA, tmp_curve_uvB;

    ON_SimpleArray<OverlapSegment *> overlaps;
    ON_ClassArray<ON_SSX_EVENT> csx_events;
    const auto return_budget_exceeded = [&]() {
	for (int overlap_index = 0;
	     overlap_index < overlaps.Count(); ++overlap_index)
	{
	    delete overlaps[overlap_index];
	    overlaps[overlap_index] = NULL;
	}
	x.SetCount(original_count);
	if (treeA == NULL) {
	    delete rootA;
	}
	if (treeB == NULL) {
	    delete rootB;
	}
	return budget_scope.BudgetExceededResult();
    };

    if (coaxial_revolution_pair) {
	bool profiles_disjoint = false;
	const bool profiles_classified =
	    coaxial_revolution_profiles_are_disjoint(rootA->m_surf,
		rootB->m_surf, coaxial_axis, isect_tol,
		profiles_disjoint);
	if (budget_scope.IsBudgetExceeded()) {
	    return return_budget_exceeded();
	}
	if (profiles_classified && profiles_disjoint) {
	    if (treeA == NULL) {
		delete rootA;
	    }
	    if (treeB == NULL) {
		delete rootB;
	    }
	    return 0;
	}
    }

    find_overlap_boundary_curves(overlaps, csx_events, tmp_curvept, tmp_curve_uvA,
				 tmp_curve_uvB, surfA, surfB, treeA, treeB, isect_tol, isect_tolA,
				 isect_tolB, overlap_tol);

    split_overlaps_at_intersections(overlaps, surfA, surfB, treeA, treeB,
				    isect_tol, isect_tolA, isect_tolB);
    if (budget_scope.IsBudgetExceeded()) {
	return return_budget_exceeded();
    }

    // add csx_events
    for (int i = 0; i < csx_events.Count(); ++i) {
	x.Append(csx_events[i]);

	// set the curves to NULL so they aren't deleted by destructor
	csx_events[i].m_curve3d = NULL;
	csx_events[i].m_curveA = NULL;
	csx_events[i].m_curveB = NULL;
    }

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
	    if (surfA->IsPlanar() && surfB->IsPlanar()) {
		ON_SSX_EVENT event;

		int ret = set_ssx_event_from_curves(event,
						    overlaps[i]->m_curve3d, overlaps[i]->m_curveA,
						    overlaps[i]->m_curveB, surfA, surfB);

		if (ret != 0) {
		    bu_log("warning: reverse failed. The direction of %d "
			   "might be wrong.\n", x.Count() - original_count);
		}

		x.Append(event);

		// set the curves to NULL so they aren't deleted by
		// ~ON_SSX_EVENT()
		event.m_curve3d = event.m_curveA = event.m_curveB = NULL;
	    } else {
		delete overlaps[i];
	    }
	    overlaps[i] = NULL;
	}
    }

    // create overlap events
    ON_ClassArray<Overlapevent> overlap_events;
    for (int i = 0; i < overlaps.Count(); i++) {
	if (!is_valid_overlap(overlaps[i])) {
	    continue;
	}

	for (int j = 0; j <= overlaps.Count(); j++) {
	    if (overlaps[i]->m_curve3d->IsClosed() &&
		overlaps[i]->m_curveA->IsClosed()  &&
		overlaps[i]->m_curveB->IsClosed())
	    {
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
		overlap_events.Append(Overlapevent(&x, x.Count() - 1));

		// Set the curves to NULL so they aren't deleted by
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
	    fill_gap_if_closed(overlaps[i]->m_curve3d, isect_tol);
	    fill_gap_if_closed(overlaps[i]->m_curveA, isect_tolA);
	    fill_gap_if_closed(overlaps[i]->m_curveB, isect_tolB);
	}
    }

    for (int i = 0; i < overlap_events.Count(); ++i) {
	ON_SSX_EVENT &event = overlap_events[i].Event();
	// The overlap region should be to the LEFT of that *m_curveA*.
	// (see opennurbs/opennurbs_x.h)

	// find a point with a single tangent
	double start_t = event.m_curveA->Domain().Mid();
	if (!event.m_curveA->IsContinuous(ON::continuity::G1_continuous, start_t)) {
	    // if mid point doesn't work, try a couple other points
	    // TODO: what happens if none of them work?
	    start_t = event.m_curveA->Domain().NormalizedParameterAt(1.0 / 3.0);
	    if (!event.m_curveA->IsContinuous(ON::continuity::G1_continuous, start_t)) {
		start_t = event.m_curveA->Domain().NormalizedParameterAt(2.0 / 3.0);
	    }
	}

	ON_3dPoint start_ptA = event.m_curveA->PointAt(start_t);
	ON_3dVector normA = ON_CrossProduct(ON_3dVector::ZAxis,
					    event.m_curveA->TangentAt(start_t));
	double line_len = 1.0 + event.m_curveA->BoundingBox().Diagonal().Length();

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
	ON_Intersect(&inside_lineA, event.m_curveA, inside_events, isect_tol);
	ON_Intersect(&outside_lineA, event.m_curveA, outside_events, isect_tol);
	std::vector<double> line_t;
	if (inside_events.Count() != 2 && outside_events.Count() == 2) {
	    // the direction of the curve should be opposite
	    event.m_curve3d->Reverse();
	    event.m_curveA->Reverse();
	    event.m_curveB->Reverse();
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
	for (int j = 0; j < overlap_events.Count(); j++) {
	    // any curves that intersect the line crossing through the
	    // loop may be inside the loop
	    // FIXME: What about curves inside the loop that the line
	    //        happens to miss?
	    if (i == j) {
		continue;
	    }
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(&inside_lineA, overlap_events[j].Event().m_curveA, x_event, isect_tol);
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
	    event.m_curve3d->Reverse();
	    event.m_curveA->Reverse();
	    event.m_curveB->Reverse();
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
	    if (overlap_events[i].IsCurveCompletelyIn(overlap_events[j].Event().m_curve3d)) {
		overlap_events[i].m_inside.push_back(j);
	    }
	}
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%d overlaps.\n", overlaps.Count());
	bu_log("%d overlap events.\n", overlap_events.Count());
    }

    if (surfA->IsPlanar() && surfB->IsPlanar() && overlaps.Count()) {
	if (budget_scope.IsBudgetExceeded()) {
	    return return_budget_exceeded();
	}
	return x.Count() - original_count;
    }

    /* Second step: calculate the intersection of the bounding boxes.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     */
    typedef std::vector<SSIRefinementCandidate> NodePairs;
    NodePairs candidates, next_candidates;
    std::vector<std::unique_ptr<Subsurface> > directional_refinement_nodes;
    std::map<std::pair<Subsurface *, int>,
	std::pair<Subsurface *, Subsurface *> > directional_split_cache;
    if (!budget_scope.ConsumeCandidatePair()) {
	return return_budget_exceeded();
    }
    candidates.push_back(SSIRefinementCandidate(rootA, rootB));
    size_t candidate_pair_count = candidates.size();

    /* The previous simultaneous refinement performed MAX_SSI_DEPTH + 1
     * splits on each surface.  Preserve that per-surface resolution, but
     * split only one side of a candidate at a time.  Testing the resulting
     * children before splitting the other side prunes spatially irrelevant
     * combinations instead of eagerly multiplying every pair by sixteen. */
    const unsigned int standard_refinement_depth = MAX_SSI_DEPTH + 1;
    /* For coaxial surfaces of revolution, first localize intersections in
     * each meridian profile and then refine their sweep directions.  A
     * directional split at each stage reaches the same final resolution as
     * a quadtree split without multiplying two rotationally symmetric
     * parameter grids before their relevant profile bands are known. */
    const bool staged_coaxial_refinement =
	coaxial_revolution_pair && overlap_events.Count() == 0;
    /* Parallel but offset revolutions lose their most useful spatial
     * separation when both parameter directions are split at once.  First
     * localize their meridian profiles, then refine their angular sectors.
     * This reaches the same per-direction depth as quadtree refinement while
     * allowing profile separation to reject irrelevant radial bands early. */
    const bool staged_parallel_revolution_refinement =
	parallel_revolution_pair && overlap_events.Count() == 0;
    const bool staged_directional_refinement = staged_coaxial_refinement ||
	staged_parallel_revolution_refinement;
    const unsigned int staged_initial_refinement_depth =
	standard_refinement_depth;
    const unsigned int maximum_refinement_depth =
	staged_directional_refinement ?
	    staged_initial_refinement_depth + standard_refinement_depth :
	    standard_refinement_depth;
    const unsigned int maximum_refinement_rounds =
	2 * maximum_refinement_depth;
    /* Coplanar overlap loops need samples from both parameter domains.
     * Preserve an affine patch only for ordinary, non-overlap SSI. */
    const bool preserve_affine_planar_A =
	affine_planar_A && overlap_events.Count() == 0;
    const bool preserve_affine_planar_B =
	affine_planar_B && overlap_events.Count() == 0;
    const auto staged_split_direction = [&](bool refine_A,
	bool initial_stage) {
	if (staged_coaxial_refinement) {
	    const int profile_direction = refine_A ?
		coaxial_profile_direction_A : coaxial_profile_direction_B;
	    return initial_stage ? profile_direction : 1 - profile_direction;
	}
	if (staged_parallel_revolution_refinement) {
	    const int profile_direction = refine_A ?
		parallel_profile_direction_A : parallel_profile_direction_B;
	    return initial_stage ? profile_direction : 1 - profile_direction;
	}
	return refine_A ? shallow_revolution_direction_A :
	    shallow_revolution_direction_B;
    };
    for (unsigned int refinement_round = 0;
	 refinement_round < maximum_refinement_rounds && !candidates.empty();
	 ++refinement_round)
    {
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end();
	     ++i)
	{
	    // If the Subsurface from either surfA or surfB (we're
	    // checking surfA in this case) is completely inside an
	    // overlap region, then we know that there is no
	    // (non-overlap) intersection between this pair of
	    // Subsurfaces, and thus no point in subdividing them
	    // further.
	    if (is_subsurfaceA_completely_inside_overlap(overlap_events,
		    i->surfaceA, isect_tolA))
	    {
		continue;
	    }

	    const bool directional_initial_stage = staged_directional_refinement &&
		(i->depthA < staged_initial_refinement_depth ||
		 i->depthB < staged_initial_refinement_depth);
	    const unsigned int refinement_limit = directional_initial_stage ?
		staged_initial_refinement_depth : maximum_refinement_depth;
	    bool can_refine_A = !preserve_affine_planar_A &&
		i->depthA < refinement_limit;
	    bool can_refine_B = !preserve_affine_planar_B &&
		i->depthB < refinement_limit;
	    if (!can_refine_A && !can_refine_B) {
		next_candidates.push_back(*i);
		continue;
	    }

	    bool refine_A = refine_first_ssi_surface(*i, can_refine_A,
		can_refine_B);
	    const int first_direction = staged_split_direction(refine_A,
		directional_initial_stage);
	    std::vector<Subsurface *> parts = first_direction >= 0 ?
		subdivide_subsurface_direction(
		    refine_A ? i->surfaceA : i->surfaceB, first_direction,
		    directional_refinement_nodes, directional_split_cache) :
		subdivide_subsurface(refine_A ? i->surfaceA : i->surfaceB);
	    if (parts.size() == 1 &&
		parts[0] == (refine_A ? i->surfaceA : i->surfaceB))
	    {
		/* A failed surface split cannot gain resolution on later rounds.
		 * Retire that side and, when possible, refine the other one. */
		if (refine_A) {
		    i->depthA = refinement_limit;
		    can_refine_A = false;
		} else {
		    i->depthB = refinement_limit;
		    can_refine_B = false;
		}
		if (!can_refine_A && !can_refine_B) {
		    next_candidates.push_back(*i);
		    continue;
		}
		refine_A = can_refine_A;
		const int second_direction = staged_split_direction(refine_A,
		    directional_initial_stage);
		parts = second_direction >= 0 ?
		    subdivide_subsurface_direction(
			refine_A ? i->surfaceA : i->surfaceB,
			second_direction, directional_refinement_nodes,
			directional_split_cache) :
		    subdivide_subsurface(refine_A ? i->surfaceA : i->surfaceB);
		if (parts.size() == 1 &&
		    parts[0] == (refine_A ? i->surfaceA : i->surfaceB))
		{
		    if (refine_A)
			i->depthA = refinement_limit;
		    else
			i->depthB = refinement_limit;
		    next_candidates.push_back(*i);
		    continue;
		}
	    }
	    const unsigned int child_depthA = i->depthA + (refine_A ? 1 : 0);
	    const unsigned int child_depthB = i->depthB + (refine_A ? 0 : 1);
	    for (size_t part_index = 0; part_index < parts.size(); ++part_index) {
		Subsurface *partA = refine_A ? parts[part_index] : i->surfaceA;
		Subsurface *partB = refine_A ? i->surfaceB : parts[part_index];
	if (partB->Intersect(*partA, isect_tol) &&
	    (!staged_coaxial_refinement ||
	     coaxial_subsurface_profiles_may_intersect(partA, partB,
		 coaxial_axis, isect_tol)) &&
	    (!staged_parallel_revolution_refinement ||
	     parallel_revolution_subsurface_profiles_may_intersect(partA,
		 partB, parallel_axis_A, parallel_axis_B, isect_tol)) &&
	    subsurface_control_hulls_may_intersect(partA, partB,
		isect_tol))
		{
		    if (candidate_pair_count >= MAX_SSI_CANDIDATE_PAIRS ||
			!budget_scope.ConsumeCandidatePair())
		    {
			bu_log("SSI refinement work limit reached in round %u "
			    "with %zu active and %zu admitted candidates "
			    "(depths %u/%u, spans %.17g/%.17g).\n",
			    refinement_round, candidates.size(),
			    candidate_pair_count, i->depthA, i->depthB,
			    subsurface_physical_span(i->surfaceA),
			    subsurface_physical_span(i->surfaceB));
			return return_budget_exceeded();
		    }
		    ++candidate_pair_count;
		    next_candidates.push_back(SSIRefinementCandidate(partA, partB,
			child_depthA, child_depthB));
		}
	    }
	}
	candidates = next_candidates;
    }
    if (DEBUG_BREP_INTERSECT) {
	bu_log("We get %zu intersection bounding boxes.\n", candidates.size());
    }

    /* Third step: get the intersection points using triangular approximation,
     * and then Newton iterations.
     */
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end();
	 ++i)
    {
	// We have arrived at the bottom of the trees.
	// Get an estimate of the intersection point lying on the intersection curve.

	// Get the corners of each surface sub-patch inside the bounding-box.
	ON_BoundingBox box_intersect;
	i->surfaceA->Intersect(*(i->surfaceB), isect_tol, &box_intersect);

	/* We approximate each surface sub-patch inside the bounding-box with two
	 * triangles that share an edge.
	 * The intersection of the surface sub-patches is approximated as the
	 * intersection of triangles.
	 */
	std::pair<Triangle, Triangle> trianglesA =
	    get_subsurface_triangles(i->surfaceA, surfA);
	std::pair<Triangle, Triangle> trianglesB =
	    get_subsurface_triangles(i->surfaceB, surfB);

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
    if (budget_scope.IsBudgetExceeded()) {
	return return_budget_exceeded();
    }
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
    std::vector<ON_Curve *>intersect3d, intersect_uvA, intersect_uvB;
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

	// Curve in 3D space.
	ON_3dPointArray points3d;
	for (int j = 0; j < polyline.Count(); j++) {
	    points3d.Append(curvept[polyline[j]]);
	}
	// close curve if it forms a loop
	const double endpoint_distance =
	    curvept[startpoint].DistanceTo(curvept[endpoint]);
	if (!is_seam && endpoint_distance < max_dist &&
	    endpoint_distance > ON_ZERO_TOLERANCE) {
	    points3d.Append(curvept[startpoint]);
	}

	// Curve in UV space (surfA).
	ON_3dPointArray pointsA;
	for (int j = 0; j < polyline.Count(); j++) {
	    const ON_2dPoint &point2d = curve_uvA[polyline[j]];
	    pointsA.Append(ON_3dPoint(point2d.x, point2d.y, 0.0));
	}
	// close curve if it forms a loop (happens rarely compared to 3D)
	if (!is_seam &&
	    fabs(curve_uvA[startpoint].x - curve_uvA[endpoint].x) < max_dist_uA &&
	    fabs(curve_uvA[startpoint].y - curve_uvA[endpoint].y) < max_dist_vA)
	{
	    const ON_2dPoint &point2d = curve_uvA[startpoint];
	    pointsA.Append(ON_3dPoint(point2d.x, point2d.y, 0.0));
	}

	// Curve in UV space (surfB).
	ON_3dPointArray pointsB;
	for (int j = 0; j < polyline.Count(); j++) {
	    const ON_2dPoint &point2d = curve_uvB[polyline[j]];
	    pointsB.Append(ON_3dPoint(point2d.x, point2d.y, 0.0));
	}
	// close curve if it forms a loop (happens rarely compared to 3D)
	if (!is_seam &&
	    fabs(curve_uvB[startpoint].x - curve_uvB[endpoint].x) < max_dist_uB &&
	    fabs(curve_uvB[startpoint].y - curve_uvB[endpoint].y) < max_dist_vB)
	{
	    const ON_2dPoint &point2d = curve_uvB[startpoint];
	    pointsB.Append(ON_3dPoint(point2d.x, point2d.y, 0.0));
	}

	ON_PolylineCurve *curve3d = new ON_PolylineCurve(points3d);
	const bool has_terminal_spatial_closure = points3d.Count() > 2 &&
	    points3d[0].DistanceTo(points3d[points3d.Count() - 1]) <=
	    INTERSECTION_TOL;
	const bool use_shared_parameterization_A =
	    points3d.Count() == pointsA.Count() ||
	    (has_terminal_spatial_closure &&
	     points3d.Count() == pointsA.Count() + 1);
	const bool use_shared_parameterization_B =
	    points3d.Count() == pointsB.Count() ||
	    (has_terminal_spatial_closure &&
	     points3d.Count() == pointsB.Count() + 1);
	intersect3d.push_back(curve3d);

	// Keep each UV sequence paired to the 3D samples independently.  A
	// periodic surface can omit the terminal 3D closure sample.
	ON_PolylineCurve *curveA = new ON_PolylineCurve(pointsA);
	curveA->ChangeDimension(2);
	intersect_uvA.push_back(use_shared_parameterization_A ? curveA :
		curve_fitting(curveA, fitting_tolA));

	ON_PolylineCurve *curveB = new ON_PolylineCurve(pointsB);
	curveB->ChangeDimension(2);
	intersect_uvB.push_back(use_shared_parameterization_B ? curveB :
		curve_fitting(curveB, fitting_tolB));

	delete polylines[i];
    }

    if (DEBUG_BREP_INTERSECT) {
	bu_log("%zd curve segments and %d single points.\n", intersect3d.size(), single_pts.Count());
    }
    bu_free(polyline_of_terminal, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");

    // generate transverse and tangent ON_SSX_EVENTs (overlap events
    // handled above)
    if (intersect3d.size() == intersect_uvA.size() && intersect3d.size() == intersect_uvB.size()) {
	for (size_t i = 0; i < intersect3d.size(); i++) {
	    ON_SSX_EVENT event;

	    int ret = set_ssx_event_from_curves(event, intersect3d[i],
		    intersect_uvA[i], intersect_uvB[i], surfA, surfB);

	    if (ret != 0) {
		bu_log("warning: reverse failed. The direction of %d might be "
			"wrong.\n", x.Count() - original_count);
	    }

	    x.Append(event);

	    // set the curves to NULL so they aren't deleted by
	    // ~ON_SSX_EVENT()
	    event.m_curve3d = event.m_curveA = event.m_curveB = NULL;
	}
    }

    for (int i = 0; i < single_pts.Count(); i++) {
	bool unique_pt = true;
	for (size_t j = 0; j < intersect3d.size(); ++j) {
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

	if (budget_scope.IsBudgetExceeded()) {
	    return return_budget_exceeded();
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
