/*                  I N T E R S E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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


/* Sub-division support for a curve.
 * It's similar to generating the bounding box tree, when the Split()
 * method is called, the curve is splitted into two parts, whose bounding
 * boxes become the children of the original curve's bbox.
 */
class Subcurve {
    friend class Subsurface;
private:
    ON_BoundingBox m_node;
public:
    ON_Curve *m_curve;
    ON_Interval m_t;
    Subcurve *m_children[2];
    ON_BOOL32 m_islinear;

    Subcurve() : m_curve(NULL), m_islinear(false)
    {
	m_children[0] = m_children[1] = NULL;
    }
    Subcurve(const Subcurve &_scurve)
    {
	m_islinear = _scurve.m_islinear;
	m_curve = _scurve.m_curve->Duplicate();
	m_t = _scurve.m_t;
	m_children[0] = m_children[1] = NULL;
	SetBBox(_scurve.m_node);
    }
    ~Subcurve()
    {
	for (int i = 0; i < 2; i++) {
	    if (m_children[i])
		delete m_children[i];
	}
	delete m_curve;
    }
    int Split()
    {
	if (m_children[0] && m_children[1])
	    return 0;

	for (int i = 0; i < 2; i++)
	    m_children[i] = new Subcurve();
	ON_SimpleArray<double> pline_t;
	double split_t = m_t.Mid();
	if (m_curve->IsPolyline(NULL, &pline_t) && pline_t.Count() > 2) {
	    split_t = pline_t[pline_t.Count()/2];
	}
	if (m_curve->Split(split_t, m_children[0]->m_curve, m_children[1]->m_curve) == false)
	    return -1;

	m_children[0]->m_t = m_children[0]->m_curve->Domain();
	m_children[0]->SetBBox(m_children[0]->m_curve->BoundingBox());
	m_children[0]->m_islinear = m_children[0]->m_curve->IsLinear();
	m_children[1]->m_t = m_children[1]->m_curve->Domain();
	m_children[1]->SetBBox(m_children[1]->m_curve->BoundingBox());
	m_children[1]->m_islinear = m_children[1]->m_curve->IsLinear();

	return 0;
    }
    void GetBBox(ON_3dPoint &min, ON_3dPoint &max)
    {
	min = m_node.m_min;
	max = m_node.m_max;
    }
    void SetBBox(const ON_BoundingBox &bbox)
    {
	m_node = bbox;
    }
    bool IsPointIn(const ON_3dPoint &pt, double tolerance = 0.0)
    {
	ON_3dVector vtol(tolerance, tolerance, tolerance);
	ON_BoundingBox new_bbox(m_node.m_min-vtol, m_node.m_max+vtol);
	return new_bbox.IsPointIn(pt);
    }
    bool Intersect(const Subcurve& other, double tolerance = 0.0, ON_BoundingBox* intersection = NULL) const
    {
	ON_3dVector vtol(tolerance, tolerance, tolerance);
	ON_BoundingBox new_bbox(m_node.m_min-vtol, m_node.m_max+vtol);
	ON_BoundingBox box;
	bool ret = box.Intersection(new_bbox, other.m_node);
	if (intersection != NULL)
	    *intersection = box;
	return ret;
    }
};


/* Sub-division support for a surface.
 * It's similar to generating the bounding box tree, when the Split()
 * method is called, the surface is splitted into two parts, whose bounding
 * boxes become the children of the original surface's bbox.
 */
class Subsurface {
private:
    ON_BoundingBox m_node;
public:
    ON_Surface *m_surf;
    ON_Interval m_u, m_v;
    Subsurface *m_children[4];
    ON_BOOL32 m_isplanar;

    Subsurface() : m_surf(NULL), m_isplanar(false)
    {
	m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
    }
    Subsurface(const Subsurface &_ssurf)
    {
	m_surf = _ssurf.m_surf->Duplicate();
	m_u = _ssurf.m_u;
	m_v = _ssurf.m_v;
	m_isplanar = _ssurf.m_isplanar;
	m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
	SetBBox(_ssurf.m_node);
    }
    ~Subsurface()
    {
	for (int i = 0; i < 4; i++) {
	    if (m_children[i])
		delete m_children[i];
	}
	delete m_surf;
    }
    int Split()
    {
	if (m_children[0] && m_children[1] && m_children[2] && m_children[3])
	    return 0;

	for (int i = 0; i < 4; i++)
	    m_children[i] = new Subsurface();
	ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL;
	ON_BOOL32 ret = true;
	ret = m_surf->Split(0, m_surf->Domain(0).Mid(), temp_surf1, temp_surf2);
	if (!ret) {
	    delete temp_surf1;
	    delete temp_surf2;
	    return -1;
	}

	ret = temp_surf1->Split(1, m_surf->Domain(1).Mid(), m_children[0]->m_surf, m_children[1]->m_surf);
	delete temp_surf1;
	if (!ret) {
	    delete temp_surf2;
	    return -1;
	}
	m_children[0]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[0]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[0]->SetBBox(m_children[0]->m_surf->BoundingBox());
	m_children[0]->m_isplanar = m_children[0]->m_surf->IsPlanar();
	m_children[1]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[1]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[1]->SetBBox(m_children[1]->m_surf->BoundingBox());
	m_children[1]->m_isplanar = m_children[1]->m_surf->IsPlanar();

	ret = temp_surf2->Split(1, m_v.Mid(), m_children[2]->m_surf, m_children[3]->m_surf);
	delete temp_surf2;
	if (!ret)
	    return -1;
	m_children[2]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[2]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[2]->SetBBox(m_children[2]->m_surf->BoundingBox());
	m_children[2]->m_isplanar = m_children[2]->m_surf->IsPlanar();
	m_children[3]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[3]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[3]->SetBBox(m_children[3]->m_surf->BoundingBox());
	m_children[3]->m_isplanar = m_children[3]->m_surf->IsPlanar();

	return 0;
    }
    void GetBBox(ON_3dPoint &min, ON_3dPoint &max)
    {
	min = m_node.m_min;
	max = m_node.m_max;
    }
    void SetBBox(const ON_BoundingBox &bbox)
    {
	m_node = bbox;
	/* Make sure that each dimension of the bounding box is greater than
	 * ON_ZERO_TOLERANCE.
	 * It does the same work as building the surface tree when ray tracing
	 */
	for (int i = 0; i < 3; i++) {
	    double d = m_node.m_max[i] - m_node.m_min[i];
	    if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		m_node.m_min[i] -= 0.001;
		m_node.m_max[i] += 0.001;
	    }
	}
    }
    bool Intersect(const Subcurve& curve, double tolerance = 0.0, ON_BoundingBox* intersection = NULL) const
    {
	ON_3dVector vtol(tolerance, tolerance, tolerance);
	ON_BoundingBox new_bbox(m_node.m_min-vtol, m_node.m_max+vtol);
	ON_BoundingBox box;
	bool ret = box.Intersection(new_bbox, curve.m_node);
	if (intersection != NULL)
	    *intersection = box;
	return ret;
    }
    bool Intersect(const Subsurface& surf, double tolerance = 0.0, ON_BoundingBox* intersection = NULL) const
    {
	ON_3dVector vtol(tolerance, tolerance, tolerance);
	ON_BoundingBox new_bbox(m_node.m_min-vtol, m_node.m_max+vtol);
	ON_BoundingBox box;
	bool ret = box.Intersection(new_bbox, surf.m_node);
	if (intersection != NULL)
	    *intersection = box;
	return ret;
    }
};


/**
 * Point-point intersections (PPI)
 *
 * approach:
 *
 * - Calculate the distance of the two points.
 *
 * - If the distance is within the tolerance, the two points
 *   intersect. Thd mid-point of them is the intersection point,
 *   and half of the distance is the uncertainty radius.
 */


bool
ON_Intersect(const ON_3dPoint& pointA,
	     const ON_3dPoint& pointB,
	     ON_ClassArray<ON_PX_EVENT>& x,
	     double tolerance)
{
    if (tolerance <= 0.0)
	tolerance = ON_ZERO_TOLERANCE;

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


// The maximal depth for subdivision - trade-off between accurancy and
// performance
#define MAX_PCI_DEPTH 8


// We make the default tolerance for PSI the same as that of curve-curve
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define PCI_DEFAULT_TOLERANCE 0.001


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define PCI_MAX_ITERATIONS 100


bool
ON_Intersect(const ON_3dPoint& pointA,
	     const ON_Curve& curveB,
	     ON_ClassArray<ON_PX_EVENT>& x,
	     double tolerance,
	     const ON_Interval* curveB_domain)
{
    if (tolerance <= 0.0)
	tolerance = PCI_DEFAULT_TOLERANCE;

    Subcurve root;
    if (curveB_domain == NULL || *curveB_domain == curveB.Domain()) {
	root.m_curve = curveB.Duplicate();
	root.m_t = curveB.Domain();
    }
    else {
	// Use ON_Curve::Split() to get the sub-curve inside curveB_domain
	ON_Curve *temp_curve1 = NULL, *temp_curve2 = NULL, *temp_curve3 = NULL;
	if (!curveB.Split(curveB_domain->Min(), temp_curve1, temp_curve2))
	    return false;
	delete temp_curve1;
	temp_curve1 = NULL;
	if (!temp_curve2->Split(curveB_domain->Max(), temp_curve1, temp_curve3))
	    return false;
	delete temp_curve1;
	delete temp_curve2;
	root.m_curve = temp_curve3;
	root.m_t = *curveB_domain;
    }

    root.SetBBox(root.m_curve->BoundingBox());
    root.m_islinear = root.m_curve->IsLinear();

    if (!root.IsPointIn(pointA, tolerance))
	return false;

    std::vector<Subcurve*> candidates, next_candidates;
    candidates.push_back(&root);

    // Find the sub-curves that are possibly intersected with the point.
    for (int i = 0; i < MAX_PCI_DEPTH; i++) {
	next_candidates.clear();
	for (unsigned int j = 0; j < candidates.size(); j++) {
	    if (candidates[j]->m_islinear) {
		next_candidates.push_back(candidates[j]);
	    } else {
		if (candidates[j]->Split() == 0) {
		    if (candidates[j]->m_children[0]->IsPointIn(pointA, tolerance))
			next_candidates.push_back(candidates[j]->m_children[0]);
		    if (candidates[j]->m_children[1]->IsPointIn(pointA, tolerance))
			next_candidates.push_back(candidates[j]->m_children[1]);
		} else
		    next_candidates.push_back(candidates[j]);
	    }
	}
	candidates = next_candidates;
    }

    for (unsigned int i = 0; i < candidates.size(); i++) {
	// Use linear approximation to get an estimated intersection point
	ON_Line line(candidates[i]->m_curve->PointAtStart(), candidates[i]->m_curve->PointAtEnd());
	double t, dis;
	line.ClosestPointTo(pointA, &t);

	// Make sure line_t belongs to [0, 1]
	double line_t;
	if (t < 0)
	    line_t = 0;
	else if (t > 1)
	    line_t = 1;
	else
	    line_t = t;

	double closest_point_t = candidates[i]->m_t.Min() + candidates[i]->m_t.Length()*line_t;
	ON_3dPoint closest_point = curveB.PointAt(closest_point_t);
	dis = closest_point.DistanceTo(pointA);

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
	while (!ON_NearZero(last_t-closest_point_t)
	       && dis > tolerance
	       && iterations++ < PCI_MAX_ITERATIONS) {
	    ON_3dVector tangent, s_deriv;
	    last_t = closest_point_t;
	    curveB.Ev2Der(closest_point_t, closest_point, tangent, s_deriv);
	    double F = ON_DotProduct(closest_point-pointA, tangent);
	    double derivF = ON_DotProduct(closest_point-pointA, s_deriv) + ON_DotProduct(tangent, tangent);
	    if (!ON_NearZero(derivF))
		closest_point_t -= F/derivF;
	    dis = closest_point.DistanceTo(pointA);
	}
	closest_point = curveB.PointAt(closest_point_t);

	if (dis <= tolerance) {
	    ON_PX_EVENT *Event = new ON_PX_EVENT;
	    Event->m_type = ON_PX_EVENT::pcx_point;
	    Event->m_A = pointA;
	    Event->m_B = closest_point;
	    Event->m_b[0] = closest_point_t;
	    Event->m_Mid = (pointA + Event->m_B) * 0.5;
	    Event->m_radius = closest_point.DistanceTo(pointA) * 0.5;
	    x.Append(*Event);
	    return true;
	}
    }
    // All candidates have no intersection
    return false;
}


/**
 * Point-surface intersections (PSI)
 *
 * approach:
 *
 * - Use brlcad::get_closest_point() to compute the closest point on
 *   the surface to that point.
 *
 * - If the closest point is within the given tolerance, there is an
 *   intersection.
 */


// We make the default tolerance for PSI the same as that of curve-surface
// intersections defined by openNURBS (see other/openNURBS/opennurbs_curve.h)
#define PSI_DEFAULT_TOLERANCE 0.001


// The default maximal depth for creating a surfacee tree (8) is way too
// much - killing performance. Since it's only used for getting an
// estimation, and we use Newton iterations afterwards, it's reasonable
// to use a smaller depth in this step.
#define MAX_PSI_DEPTH 4


bool
ON_Intersect(const ON_3dPoint& pointA,
	     const ON_Surface& surfaceB,
	     ON_ClassArray<ON_PX_EVENT>& x,
	     double tolerance,
	     const ON_Interval* surfaceB_udomain,
	     const ON_Interval* surfaceB_vdomain,
	     brlcad::SurfaceTree* tree)
{
    if (tolerance <= 0.0)
	tolerance = PSI_DEFAULT_TOLERANCE;

    ON_Interval u_domain, v_domain;
    if (surfaceB_udomain == 0)
	u_domain = surfaceB.Domain(0);
    else
	u_domain = *surfaceB_udomain;
    if (surfaceB_vdomain == 0)
	v_domain = surfaceB.Domain(1);
    else
	v_domain = *surfaceB_vdomain;

    // We need ON_BrepFace for get_closest_point().
    // TODO: Use routines like SubSurface in SSI to reduce computation.
    ON_Brep *brep = surfaceB.BrepForm();
    if (!brep) return false;
    ON_2dPoint closest_point_uv;
    bool delete_tree = false;
    if (!tree) {
	tree = new brlcad::SurfaceTree(brep->Face(0), false, MAX_PSI_DEPTH);
	delete_tree = true;
    }
    if (brlcad::get_closest_point(closest_point_uv, brep->Face(0), pointA, tree) == false) {
	delete brep;
	if (delete_tree) delete tree;
	return false;
    }

    delete brep;
    if (delete_tree) delete tree;

    ON_3dPoint closest_point_3d = surfaceB.PointAt(closest_point_uv.x, closest_point_uv.y);
    if (pointA.DistanceTo(closest_point_3d) <= tolerance
	&& u_domain.Includes(closest_point_uv.x)
	&& v_domain.Includes(closest_point_uv.y)) {
	ON_PX_EVENT Event;
	Event.m_type = ON_PX_EVENT::psx_point;
	Event.m_A = pointA;
	Event.m_b = closest_point_uv;
	Event.m_B = closest_point_3d;
	Event.m_Mid = (pointA + Event.m_B) * 0.5;
	Event.m_radius = pointA.DistanceTo(Event.m_B) * 0.5;
	x.Append(Event);
	return true;
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


// The maximal depth for subdivision - trade-off between accurancy and
// performance
#define MAX_CCI_DEPTH 8


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define CCI_MAX_ITERATIONS 100


// We can only test a finite number of points to determine overlap.
// Here we test 16 points uniformly distributed.
#define CCI_OVERLAP_TEST_POINTS 16


// Build the curve tree root within a given domain
bool
build_curve_root(const ON_Curve* curve, const ON_Interval* domain, Subcurve& root)
{
    if (domain == NULL || *domain == curve->Domain()) {
	root.m_curve = curve->Duplicate();
	root.m_t = curve->Domain();
    } else {
	// Use ON_Curve::Split() to get the sub-curve inside the domain
	ON_Curve *temp_curve1 = NULL, *temp_curve2 = NULL, *temp_curve3 = NULL;
	if (!curve->Split(domain->Min(), temp_curve1, temp_curve2))
	    return false;
	delete temp_curve1;
	temp_curve1 = NULL;
	if (!temp_curve2->Split(domain->Max(), temp_curve1, temp_curve3))
	    return false;
	delete temp_curve1;
	delete temp_curve2;
	root.m_curve = temp_curve3;
	root.m_t = *domain;
    }

    root.SetBBox(root.m_curve->BoundingBox());
    root.m_islinear = root.m_curve->IsLinear();
    return true;
}


void
newton_cci(double& t_a, double& t_b, const ON_Curve* curveA, const ON_Curve* curveB, double intersection_tolerance)
{
    // Equations:
    //   x_a(t_a) - x_b(t_b) = 0
    //   y_a(t_a) - y_b(t_b) = 0
    //   z_a(t_a) - z_b(t_b) = 0
    // It's an over-determined system.
    // We use Newton-Raphson iterations to solve two equations of the three,
    // and use the third for checking.
    double last_t_a = DBL_MAX*.5, last_t_b = DBL_MAX*.5;
    ON_3dPoint pointA = curveA->PointAt(t_a);
    ON_3dPoint pointB = curveB->PointAt(t_b);
    if (pointA.DistanceTo(pointB) < intersection_tolerance)
	return;

    int iteration = 0;
    while (fabs(last_t_a - t_a) + fabs(last_t_b - t_b) > ON_ZERO_TOLERANCE
	   && iteration++ < CCI_MAX_ITERATIONS) {
	last_t_a = t_a, last_t_b = t_b;
	ON_3dVector derivA, derivB;
	curveA->Ev1Der(t_a, pointA, derivA);
	curveB->Ev1Der(t_b, pointB, derivB);
	ON_Matrix J(2, 2), F(2, 1);
	J[0][0] = derivA.x;
	J[0][1] = -derivB.x;
	J[1][0] = derivA.y;
	J[1][1] = -derivB.y;
	F[0][0] = pointA.x - pointB.x;
	F[1][0] = pointA.y - pointB.y;
	if (!J.Invert(0.0)) {
	    // bu_log("Inverse failed.\n");
	    J[0][0] = derivA.x;
	    J[0][1] = -derivB.x;
	    J[1][0] = derivA.z;
	    J[1][1] = -derivB.z;
	    F[0][0] = pointA.x - pointB.x;
	    F[1][0] = pointA.z - pointB.z;
	    if (!J.Invert(0.0)) {
		// bu_log("Inverse failed again.\n");
		J[0][0] = derivA.y;
		J[0][1] = -derivB.y;
		J[1][0] = derivA.z;
		J[1][1] = -derivB.z;
		F[0][0] = pointA.y - pointB.y;
		F[1][0] = pointA.z - pointB.z;
		if (!J.Invert(0.0)) {
		    // bu_log("Inverse failed and never try again.\n");
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


int
compare_by_m_a0(const ON_X_EVENT* a, const ON_X_EVENT* b)
{
    // We don't care whether they are equal
    return a->m_a[0] < b->m_a[0] ? -1 : 1;
}


int
ON_Intersect(const ON_Curve* curveA,
	     const ON_Curve* curveB,
	     ON_SimpleArray<ON_X_EVENT>& x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     const ON_Interval* curveA_domain,
	     const ON_Interval* curveB_domain)
{
    if (curveA == NULL || curveB == NULL)
	return 0;

    if (intersection_tolerance <= 0.0)
	intersection_tolerance = CCI_DEFAULT_TOLERANCE;
    if (overlap_tolerance < intersection_tolerance)
	overlap_tolerance = 2*intersection_tolerance;
    double t1_tolerance = intersection_tolerance;
    double t2_tolerance = intersection_tolerance;

    Subcurve rootA, rootB;
    if (!build_curve_root(curveA, curveA_domain, rootA))
	return 0;
    if (!build_curve_root(curveB, curveB_domain, rootB))
	return 0;

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double l = rootA.m_curve->BoundingBox().Diagonal().Length();
    double dl = curveA_domain ? curveA_domain->Length() : curveA->Domain().Length();
    if (!ON_NearZero(l))
	t1_tolerance = intersection_tolerance/l*dl;
    l = rootB.m_curve->BoundingBox().Diagonal().Length();
    dl = curveB_domain ? curveB_domain->Length() : curveB->Domain().Length();
    if (!ON_NearZero(l))
	t2_tolerance = intersection_tolerance/l*dl;

    typedef std::vector<std::pair<Subcurve*, Subcurve*> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootA.Intersect(rootB, intersection_tolerance))
	candidates.push_back(std::make_pair(&rootA, &rootB));

    // Use sub-division and bounding box intersections first.
    for (int h = 0; h <= MAX_CCI_DEPTH; h++) {
	if (candidates.empty())
	    break;
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve*> splittedA, splittedB;
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
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedA[j]->Intersect(*splittedB[k], intersection_tolerance))
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
	}
	candidates = next_candidates;
    }

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    // For intersected bounding boxes, we calculate an accurate intersection
    // point.
    for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
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

		    if (startB_on_A > 1+t1_tolerance || endB_on_A < -t1_tolerance
			|| startA_on_B > 1+t2_tolerance || endA_on_B < -t2_tolerance)
			continue;

		    double t_a1, t_a2, t_b1, t_b2;
		    if (startB_on_A > 0.0)
			t_a1 = startB_on_A, t_b1 = 0.0;
		    else
			t_a1 = 0.0, t_b1 = startA_on_B;
		    if (endB_on_A < 1.0)
			t_a2 = endB_on_A, t_b2 = 1.0;
		    else
			t_a2 = 1.0, t_b2 = endA_on_B;

		    ON_X_EVENT* Event = new ON_X_EVENT;
		    Event->m_A[0] = lineA.PointAt(t_a1);
		    Event->m_A[1] = lineA.PointAt(t_a2);
		    Event->m_B[0] = lineB.PointAt(t_b1);
		    Event->m_B[1] = lineB.PointAt(t_b2);
		    Event->m_a[0] = i->first->m_t.ParameterAt(t_a1);
		    Event->m_a[1] = i->first->m_t.ParameterAt(t_a2);
		    Event->m_b[0] = i->second->m_t.ParameterAt(t_b1);
		    Event->m_b[1] = i->second->m_t.ParameterAt(t_b2);
		    Event->m_type = ON_X_EVENT::ccx_overlap;
		    tmp_x.Append(*Event);
		}
	    } else {
		// They are not parallel, check intersection point
		double t_lineA, t_lineB;
		double t_a, t_b;
		if (ON_IntersectLineLine(lineA, lineB, &t_lineA, &t_lineB, ON_ZERO_TOLERANCE, true)) {
		    // The line segments intersect
		    t_a = i->first->m_t.ParameterAt(t_lineA);
		    t_b = i->second->m_t.ParameterAt(t_lineB);

		    ON_X_EVENT* Event = new ON_X_EVENT;
		    Event->m_A[0] = Event->m_A[1] = lineA.PointAt(t_lineA);
		    Event->m_B[0] = Event->m_B[1] = lineB.PointAt(t_lineB);
		    Event->m_a[0] = Event->m_a[1] = t_a;
		    Event->m_b[0] = Event->m_b[1] = t_b;
		    Event->m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(*Event);
		}
	    }
	} else {
	    // They are not both linear.

	    // Use two different start points - the two end-points of the interval
	    // If they converge to one point, it's considered an intersection
	    // point, otherwise it's considered an overlap event.
	    // FIXME: Find a better machanism to check overlapping, because this method
	    // may miss some overlap cases. (Overlap events can also converge to one
	    // point)
	    double t_a1 = i->first->m_t.Min(), t_b1 = i->second->m_t.Min();
	    newton_cci(t_a1, t_b1, curveA, curveB, intersection_tolerance);
	    double t_a2 = i->first->m_t.Max(), t_b2 = i->second->m_t.Max();
	    newton_cci(t_a2, t_b2, curveA, curveB, intersection_tolerance);

	    ON_3dPoint pointA1 = curveA->PointAt(t_a1);
	    ON_3dPoint pointB1 = curveB->PointAt(t_b1);
	    ON_3dPoint pointA2 = curveA->PointAt(t_a2);
	    ON_3dPoint pointB2 = curveB->PointAt(t_b2);
	    if (pointA1.DistanceTo(pointA2) < intersection_tolerance
		&& pointB1.DistanceTo(pointB2) < intersection_tolerance) {
		// it's considered the same point
		ON_3dPoint pointA = curveA->PointAt(t_a1);
		ON_3dPoint pointB = curveB->PointAt(t_b1);
		double distance = pointA.DistanceTo(pointB);
		// Check the validity of the solution
		if (distance < intersection_tolerance) {
		    ON_X_EVENT *Event = new ON_X_EVENT;
		    Event->m_A[0] = Event->m_A[1] = pointA;
		    Event->m_B[0] = Event->m_B[1] = pointB;
		    Event->m_a[0] = Event->m_a[1] = t_a1;
		    Event->m_b[0] = Event->m_b[1] = t_b1;
		    Event->m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(*Event);
		}
	    } else {
		// Check overlap
		// bu_log("Maybe overlap.\n");
		double distance1 = pointA1.DistanceTo(pointB1);
		double distance2 = pointA2.DistanceTo(pointB2);

		// Check the validity of the solution
		if (distance1 < intersection_tolerance && distance2 < intersection_tolerance) {
		    ON_X_EVENT *Event = new ON_X_EVENT;
		    // We make sure that m_a[0] <= m_a[1]
		    if (t_a1 <= t_a2) {
			Event->m_A[0] = pointA1;
			Event->m_A[1] = pointA2;
			Event->m_B[0] = pointB1;
			Event->m_B[1] = pointB2;
			Event->m_a[0] = t_a1;
			Event->m_a[1] = t_a2;
			Event->m_b[0] = t_b1;
			Event->m_b[1] = t_b2;
		    } else {
			Event->m_A[0] = pointA2;
			Event->m_A[1] = pointA1;
			Event->m_B[0] = pointB2;
			Event->m_B[1] = pointB1;
			Event->m_a[0] = t_a2;
			Event->m_a[1] = t_a1;
			Event->m_b[0] = t_b2;
			Event->m_b[1] = t_b1;
		    }
		    int j;
		    for (j = 1; j < CCI_OVERLAP_TEST_POINTS; j++) {
			double strike = 1.0/CCI_OVERLAP_TEST_POINTS;
			ON_3dPoint test_point = curveA->PointAt(t_a1 + (t_a2-t_a1)*j*strike);
			ON_ClassArray<ON_PX_EVENT> pci_x;
			// Use point-curve intersection
			if (!ON_Intersect(test_point, *curveB, pci_x, overlap_tolerance))
			    break;
		    }
		    if (j == CCI_OVERLAP_TEST_POINTS) {
			Event->m_type = ON_X_EVENT::ccx_overlap;
			tmp_x.Append(*Event);
			continue;
		    }
		    // if j != CCI_OVERLAP_TEST_POINTS, two ccx_point events should
		    // be generated. Fall through.
		}
		if (distance1 < intersection_tolerance) {
		    // in case that the second one was not correct
		    ON_X_EVENT *Event = new ON_X_EVENT;
		    Event->m_A[0] = Event->m_A[1] = pointA1;
		    Event->m_B[0] = Event->m_B[1] = pointB1;
		    Event->m_a[0] = Event->m_a[1] = t_a1;
		    Event->m_b[0] = Event->m_b[1] = t_b1;
		    Event->m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(*Event);
		}
		if (distance2 < intersection_tolerance) {
		    // in case that the first one was not correct
		    ON_X_EVENT *Event = new ON_X_EVENT;
		    Event->m_A[0] = Event->m_A[1] = pointA2;
		    Event->m_B[0] = Event->m_B[1] = pointB2;
		    Event->m_a[0] = Event->m_a[1] = t_a2;
		    Event->m_b[0] = Event->m_b[1] = t_b2;
		    Event->m_type = ON_X_EVENT::ccx_point;
		    tmp_x.Append(*Event);
		}
	    }
	}
    }

    ON_SimpleArray<ON_X_EVENT> points, overlap, pending;
    // Use an O(n^2) naive approach to eliminate duplications
    for (int i = 0; i < tmp_x.Count(); i++) {
	int j;
	if (tmp_x[i].m_type == ON_X_EVENT::ccx_overlap) {
	    overlap.Append(tmp_x[i]);
	    continue;
	}
	// ccx_point
	for (j = 0; j < points.Count(); j++) {
	    if (tmp_x[i].m_A[0].DistanceTo(points[j].m_A[0]) < intersection_tolerance
		&& tmp_x[i].m_A[1].DistanceTo(points[j].m_A[1]) < intersection_tolerance
		&& tmp_x[i].m_B[0].DistanceTo(points[j].m_B[0]) < intersection_tolerance
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < intersection_tolerance)
		break;
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // Merge the overlap events that are continuous
    overlap.QuickSort(compare_by_m_a0);
    for (int i = 0; i < overlap.Count(); i++) {
	bool merged = false;
	for (int j = 0; j < pending.Count(); j++) {
	    if (pending[j].m_a[1] < overlap[i].m_a[0] - t1_tolerance) {
		pending[j].m_A[0] = curveA->PointAt(pending[j].m_a[0]);
		pending[j].m_A[1] = curveA->PointAt(pending[j].m_a[1]);
		pending[j].m_B[0] = curveB->PointAt(pending[j].m_b[0]);
		pending[j].m_B[1] = curveB->PointAt(pending[j].m_b[1]);
		x.Append(pending[j]);
		pending.Remove(j);
		j--;
		continue;
	    }
	    if (overlap[i].m_a[0] < pending[j].m_a[1] + t1_tolerance) {
		ON_Interval interval_1(overlap[i].m_b[0], overlap[i].m_b[1]);
		ON_Interval interval_2(pending[j].m_b[0], pending[j].m_b[1]);
		interval_1.MakeIncreasing();
		interval_1.m_t[0] -= t2_tolerance;
		interval_1.m_t[1] += t2_tolerance;
		if (interval_1.Intersection(interval_2)) {
		    // Need to merge
		    merged = true;
		    pending[j].m_a[1] = std::max(overlap[i].m_a[1], pending[j].m_a[1]);
		    pending[j].m_b[0] = std::min(overlap[i].m_b[0], pending[j].m_b[0]);
		    pending[j].m_b[1] = std::max(overlap[i].m_b[1], pending[j].m_b[1]);
		    break;
		}
	    }
	}
	if (merged == false)
	    pending.Append(overlap[i]);
    }
    for (int i = 0; i < pending.Count(); i++) {
	pending[i].m_A[0] = curveA->PointAt(pending[i].m_a[0]);
	pending[i].m_A[1] = curveA->PointAt(pending[i].m_a[1]);
	pending[i].m_B[0] = curveB->PointAt(pending[i].m_b[0]);
	pending[i].m_B[1] = curveB->PointAt(pending[i].m_b[1]);
	x.Append(pending[i]);
    }

    // The intersection points shouldn't be inside the overlapped parts.
    int overlap_events = x.Count();
    for (int i = 0; i < points.Count(); i++) {
	int j;
	for (j = 0; j < overlap_events; j++) {
	    if (points[i].m_a[0] > x[j].m_a[0] - t1_tolerance
		&& points[i].m_a[0] < x[j].m_a[1] + t1_tolerance)
		break;
	}
	if (j == overlap_events)
	    x.Append(points[i]);
    }

    return x.Count();
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


// The maximal depth for subdivision - trade-off between accurancy and
// performance
#define MAX_CSI_DEPTH 8


// Newton-Raphson iteration needs a good start. Although we can almost get
// a good starting point every time, but in case of some unfortunate cases,
// we define a maximum iteration, preventing from infinite loop.
#define CSI_MAX_ITERATIONS 100


// We can only test a finite number of points to determine overlap.
// Here we test 2 points uniformly distributed.
#define CSI_OVERLAP_TEST_POINTS 2


// Build the surface tree root within a given domain
bool
build_surface_root(const ON_Surface* surf, const ON_Interval* u_domain, const ON_Interval* v_domain, Subsurface& root)
{
    // First, do u split
    const ON_Surface* u_splitted_surf; // the surface after u-split
    if (u_domain == NULL || *u_domain == surf->Domain(0)) {
	u_splitted_surf = surf;
	root.m_u = surf->Domain(0);
    } else {
	// Use ON_Surface::Split() to get the sub-surface inside the domain
	ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL, *temp_surf3 = NULL;
	if (!surf->Split(0, u_domain->Min(), temp_surf1, temp_surf2))
	    return false;
	delete temp_surf1;
	temp_surf1 = NULL;
	if (!temp_surf2->Split(0, u_domain->Max(), temp_surf1, temp_surf3))
	    return false;
	delete temp_surf1;
	delete temp_surf2;
	u_splitted_surf = temp_surf3;
	root.m_u = *u_domain;
    }

    // Then v split
    if (v_domain == NULL || *v_domain == surf->Domain(1)) {
	root.m_surf = u_splitted_surf->Duplicate();
	root.m_v = surf->Domain(1);
    } else {
	// Use ON_Surface::Split() to get the sub-surface inside the domain
	ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL, *temp_surf3 = NULL;
	if (!surf->Split(1, v_domain->Min(), temp_surf1, temp_surf2))
	    return false;
	delete temp_surf1;
	temp_surf1 = NULL;
	if (!temp_surf2->Split(1, v_domain->Max(), temp_surf1, temp_surf3))
	    return false;
	delete temp_surf1;
	delete temp_surf2;
	root.m_surf = temp_surf3;
	root.m_v = *v_domain;
    }

    root.SetBBox(root.m_surf->BoundingBox());
    root.m_isplanar = root.m_surf->IsPlanar();
    return true;
}


void
newton_csi(double& t, double& u, double& v, const ON_Curve* curveA, const ON_Surface* surfB, double intersection_tolerance, brlcad::SurfaceTree* tree)
{
    // Equations:
    //   x_a(t) - x_b(u,v) = 0
    //   y_a(t) - y_b(u,v) = 0
    //   z_a(t) - z_b(u,v) = 0
    // We use Newton-Raphson iterations to solve these equations.
    double last_t = DBL_MAX/3, last_u = DBL_MAX/3, last_v = DBL_MAX/3;
    ON_3dPoint pointA = curveA->PointAt(t);
    ON_3dPoint pointB = surfB->PointAt(u, v);
    if (pointA.DistanceTo(pointB) < intersection_tolerance)
	return;

    ON_ClassArray<ON_PX_EVENT> px_event;
    if (ON_Intersect(pointA, *surfB, px_event, intersection_tolerance, 0, 0, tree)) {
	u = px_event[0].m_b.x;
	v = px_event[0].m_b.y;
	return;
    }

    int iteration = 0;
    while (fabs(last_t - t) + fabs(last_u - u) + fabs(last_v - v) > ON_ZERO_TOLERANCE
	   && iteration++ < CCI_MAX_ITERATIONS) {
	last_t = t, last_u = u, last_v = v;
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
ON_Intersect(const ON_Curve* curveA,
	     const ON_Surface* surfaceB,
	     ON_SimpleArray<ON_X_EVENT>& x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     const ON_Interval* curveA_domain,
	     const ON_Interval* surfaceB_udomain,
	     const ON_Interval* surfaceB_vdomain)
{
    if (curveA == NULL || surfaceB == NULL)
	return 0;

    if (intersection_tolerance <= 0.0)
	intersection_tolerance = CSI_DEFAULT_TOLERANCE;
    if (overlap_tolerance < intersection_tolerance)
	overlap_tolerance = 2*intersection_tolerance;
    double t_tolerance = intersection_tolerance;
    double u_tolerance = intersection_tolerance;
    double v_tolerance = intersection_tolerance;

    // We need ON_BrepFace for get_closest_point().
    // This is used in point-surface intersections, in case we build the
    // tree again and again.
    ON_Brep *brep = surfaceB->BrepForm();
    if (!brep) return false;
    brlcad::SurfaceTree *tree = new brlcad::SurfaceTree(brep->Face(0), false, MAX_PSI_DEPTH);

    Subcurve rootA;
    Subsurface rootB;
    if (!build_curve_root(curveA, curveA_domain, rootA))
	return 0;
    if (!build_surface_root(surfaceB, surfaceB_udomain, surfaceB_vdomain, rootB))
	return 0;

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double l = rootA.m_curve->BoundingBox().Diagonal().Length();
    double dl = curveA_domain ? curveA_domain->Length() : curveA->Domain().Length();
    if (!ON_NearZero(l))
	t_tolerance = intersection_tolerance/l*dl;
    l = rootB.m_surf->BoundingBox().Diagonal().Length();
    dl = surfaceB_udomain ? surfaceB_udomain->Length() : surfaceB->Domain(0).Length();
    if (!ON_NearZero(l))
	u_tolerance = intersection_tolerance/l*dl;
    dl = surfaceB_vdomain ? surfaceB_vdomain->Length() : surfaceB->Domain(1).Length();
    if (!ON_NearZero(l))
	v_tolerance = intersection_tolerance/l*dl;

    typedef std::vector<std::pair<Subcurve*, Subsurface*> > NodePairs;
    NodePairs candidates, next_candidates;
    if (rootB.Intersect(rootA, intersection_tolerance))
	candidates.push_back(std::make_pair(&rootA, &rootB));

    // Use sub-division and bounding box intersections first.
    for (int h = 0; h <= MAX_CSI_DEPTH; h++) {
	if (candidates.empty())
	    break;
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subcurve*> splittedA;
	    std::vector<Subsurface*> splittedB;
	    if ((*i).first->m_islinear || (*i).first->Split() != 0) {
		splittedA.push_back((*i).first);
	    } else {
		splittedA.push_back((*i).first->m_children[0]);
		splittedA.push_back((*i).first->m_children[1]);
	    }
	    if ((*i).second->m_isplanar || (*i).second->Split() != 0) {
		splittedB.push_back((*i).second);
	    } else {
		for (int j = 0; j < 4; j++)
		    splittedB.push_back((*i).second->m_children[j]);
	    }
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedB[k]->Intersect(*splittedA[j], intersection_tolerance))
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
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
			if (!plane.CreateFromPoints(point2, point3, point4))
			    success = false;

	    if (success && !ON_NearZero(line.Length())) {
		if (line.Direction().IsPerpendicularTo(plane.Normal())) {
		    // They are parallel
		    if (line.InPlane(plane, intersection_tolerance)) {
			// we report a csx_overlap event
			ON_X_EVENT *Event = new ON_X_EVENT;

			// First, we check the endpoints of the line segment
			ON_ClassArray<ON_PX_EVENT> px_event1, px_event2;
			int intersections = 0;
			if (ON_Intersect(line.from, *surfaceB, px_event1, intersection_tolerance, 0, 0, tree)) {
			    Event->m_A[intersections] = line.from;
			    Event->m_B[intersections] = px_event1[0].m_B;
			    Event->m_a[intersections] = i->first->m_t.Min();
			    Event->m_b[2*intersections] = px_event1[0].m_b[0];
			    Event->m_b[2*intersections+1] = px_event1[0].m_b[1];
			    intersections++;
			}
			if (ON_Intersect(line.to, *surfaceB, px_event2, intersection_tolerance, 0, 0, tree)) {
			    Event->m_A[intersections] = line.to;
			    Event->m_B[intersections] = px_event2[0].m_B;
			    Event->m_a[intersections] = i->first->m_t.Max();
			    Event->m_b[2*intersections] = px_event2[0].m_b[0];
			    Event->m_b[2*intersections+1] = px_event2[0].m_b[1];
			    intersections++;
			}

			// Then, we check the intersection of the line segment
			// and the boundaries of the surface
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
				    if (ON_NearZero(t1 - line_t[k]))
					break;
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
			    if (intersections >= 2) break;
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
			    Event->m_A[intersections] = line.PointAt(line_t[j]);
			    Event->m_B[intersections] = surfaceB->PointAt(surf_u, surf_v);
			    Event->m_a[intersections] = i->first->m_t.ParameterAt(line_t[j]);
			    Event->m_b[2*intersections] = surf_u;
			    Event->m_b[2*intersections+1] = surf_v;
			    intersections++;
			}

			// Generate an ON_X_EVENT
			if (intersections == 1) {
			    Event->m_type = ON_X_EVENT::csx_point;
			    Event->m_A[1] = Event->m_A[0];
			    Event->m_B[1] = Event->m_B[0];
			    Event->m_a[1] = Event->m_a[0];
			    Event->m_b[2] = Event->m_b[0];
			    Event->m_b[3] = Event->m_b[1];
			} else {
			    Event->m_type = ON_X_EVENT::csx_overlap;
			    if (Event->m_a[0] > Event->m_a[1]) {
				std::swap(Event->m_A[0], Event->m_A[1]);
				std::swap(Event->m_B[0], Event->m_B[1]);
				std::swap(Event->m_a[0], Event->m_a[1]);
				std::swap(Event->m_b[0], Event->m_b[2]);
				std::swap(Event->m_b[1], Event->m_b[3]);
			    }
			}
			tmp_x.Append(*Event);
			continue;
		    } else
			continue;
		} else {
		    // They are not parallel, check intersection point
		    double cos_angle = fabs(ON_DotProduct(plane.Normal(), line.Direction()))/(plane.Normal().Length()*line.Direction().Length());
		    // cos_angle != 0, otherwise the curve and the plane are parallel.
		    double distance = fabs(plane.DistanceTo(line.from));
		    double line_t = distance/cos_angle/line.Length();
		    if (line_t > 1.0 + t_tolerance)
			continue;
		    ON_3dPoint intersection = line.from + line.Direction()*line_t;
		    ON_ClassArray<ON_PX_EVENT> px_event;
		    if (!ON_Intersect(intersection, *(i->second->m_surf), px_event, intersection_tolerance, 0, 0, tree))
			continue;

		    ON_X_EVENT* Event = new ON_X_EVENT;
		    Event->m_A[0] = Event->m_A[1] = intersection;
		    Event->m_B[0] = Event->m_B[1] = px_event[0].m_B;
		    Event->m_a[0] = Event->m_a[1] = i->first->m_t.ParameterAt(line_t);
		    Event->m_b[0] = Event->m_b[2] = px_event[0].m_b.x;
		    Event->m_b[1] = Event->m_b[3] = px_event[0].m_b.y;
		    Event->m_type = ON_X_EVENT::csx_point;
		    tmp_x.Append(*Event);
		}
	    }
	}

	// They are not both linear or planar, or the above try failed.

	// Use two different start points - the two end-points of the interval
	// of the Subcurve's m_t.
	// If they converge to one point, it's considered an intersection
	// point, otherwise it's considered an overlap event.
	// FIXME: Find a better machanism to check overlapping, because this method
	// may miss some overlap cases. (Overlap events can also converge to one
	// point)
	double u1 = i->second->m_u.Mid(), v1 = i->second->m_v.Mid();
	double t1 = i->first->m_t.Min();
	newton_csi(t1, u1, v1, curveA, surfaceB, intersection_tolerance, tree);
	double u2 = i->second->m_u.Mid(), v2 = i->second->m_v.Mid();
	double t2 = i->first->m_t.Max();
	newton_csi(t2, u2, v2, curveA, surfaceB, intersection_tolerance, tree);

	if (isnan(u1) || isnan(v1) || isnan(t1)) {
	    u1 = u2; v1 = v2; t1 = t2;
	}

	if (isnan(u1) || isnan(v1) || isnan(t1))
	    continue;

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
		ON_X_EVENT *Event = new ON_X_EVENT;
		Event->m_A[0] = Event->m_A[1] = pointA1;
		Event->m_B[0] = Event->m_B[1] = pointB1;
		Event->m_a[0] = Event->m_a[1] = t1;
		Event->m_b[0] = Event->m_b[2] = u1;
		Event->m_b[1] = Event->m_b[3] = v1;
		Event->m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(*Event);
	    }
	} else {
	    // Check overlap
	    // bu_log("Maybe overlap.\n");
	    double distance1 = pointA1.DistanceTo(pointB1);
	    double distance2 = pointA2.DistanceTo(pointB2);

	    // Check the validity of the solution
	    if (distance1 < intersection_tolerance && distance2 < intersection_tolerance) {
		ON_X_EVENT *Event = new ON_X_EVENT;
		// We make sure that m_a[0] <= m_a[1]
		if (t1 <= t2) {
		    Event->m_A[0] = pointA1;
		    Event->m_A[1] = pointA2;
		    Event->m_B[0] = pointB1;
		    Event->m_B[1] = pointB2;
		    Event->m_a[0] = t1;
		    Event->m_a[1] = t2;
		    Event->m_b[0] = u1;
		    Event->m_b[1] = v1;
		    Event->m_b[2] = u2;
		    Event->m_b[3] = v2;
		} else {
		    Event->m_A[0] = pointA2;
		    Event->m_A[1] = pointA1;
		    Event->m_B[0] = pointB2;
		    Event->m_B[1] = pointB1;
		    Event->m_a[0] = t2;
		    Event->m_a[1] = t1;
		    Event->m_b[0] = u2;
		    Event->m_b[1] = v2;
		    Event->m_b[2] = u1;
		    Event->m_b[3] = v1;
		}
		int j;
		for (j = 1; j < CSI_OVERLAP_TEST_POINTS; j++) {
		    double strike = 1.0/CSI_OVERLAP_TEST_POINTS;
		    ON_3dPoint test_point = curveA->PointAt(t1 + (t2-t1)*j*strike);
		    ON_ClassArray<ON_PX_EVENT> psi_x;
		    // Use point-surface intersection
		    if (!ON_Intersect(test_point, *surfaceB, psi_x, overlap_tolerance, 0, 0, tree))
			break;
		}
		if (j == CSI_OVERLAP_TEST_POINTS) {
		    Event->m_type = ON_X_EVENT::csx_overlap;
		    tmp_x.Append(*Event);
		    continue;
		}
		// if j != CSI_OVERLAP_TEST_POINTS, two csx_point events may
		// be generated. Fall through.
	    }
	    if (distance1 < intersection_tolerance) {
		// in case that the second one was not correct
		ON_X_EVENT *Event = new ON_X_EVENT;
		Event->m_A[0] = Event->m_A[1] = pointA1;
		Event->m_B[0] = Event->m_B[1] = pointB1;
		Event->m_a[0] = Event->m_a[1] = t1;
		Event->m_b[0] = Event->m_b[2] = u1;
		Event->m_b[1] = Event->m_b[3] = v1;
		Event->m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(*Event);
	    }
	    if (distance2 < intersection_tolerance) {
		// in case that the first one was not correct
		ON_X_EVENT *Event = new ON_X_EVENT;
		Event->m_A[0] = Event->m_A[1] = pointA2;
		Event->m_B[0] = Event->m_B[1] = pointB2;
		Event->m_a[0] = Event->m_a[1] = t2;
		Event->m_b[0] = Event->m_b[2] = u2;
		Event->m_b[1] = Event->m_b[3] = v2;
		Event->m_type = ON_X_EVENT::csx_point;
		tmp_x.Append(*Event);
	    }
	}
    }

    ON_SimpleArray<ON_X_EVENT> points, overlap, pending;
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
		&& tmp_x[i].m_B[1].DistanceTo(points[j].m_B[1]) < intersection_tolerance)
		break;
	}
	if (j == points.Count()) {
	    points.Append(tmp_x[i]);
	}
    }

    // Merge the overlap events that are continuous
    overlap.QuickSort(compare_by_m_a0);
    for (int i = 0; i < overlap.Count(); i++) {
	bool merged = false;
	for (int j = 0; j < pending.Count(); j++) {
	    if (pending[j].m_a[1] < overlap[i].m_a[0] - t_tolerance) {
		x.Append(pending[j]);
		pending.Remove(j);
		j--;
		continue;
	    }
	    if (overlap[i].m_a[0] < pending[j].m_a[1] + t_tolerance) {
		ON_Interval interval_u1(overlap[i].m_b[0], overlap[i].m_b[2]);
		ON_Interval interval_u2(pending[j].m_b[0], pending[j].m_b[2]);
		ON_Interval interval_v1(overlap[i].m_b[1], overlap[i].m_b[3]);
		ON_Interval interval_v2(pending[j].m_b[1], pending[j].m_b[3]);
		interval_u1.MakeIncreasing();
		interval_u1.m_t[0] -= u_tolerance;
		interval_u1.m_t[1] += u_tolerance;
		interval_v1.MakeIncreasing();
		interval_v1.m_t[0] -= v_tolerance;
		interval_v1.m_t[1] += v_tolerance;
		if (interval_u1.Intersection(interval_u2) && interval_v1.Intersection(interval_v2)) {
		    // if the uv rectangle of them intersects, it's consider overlap.
		    merged = true;
		    if (overlap[i].m_a[1] > pending[j].m_a[1]) {
			pending[j].m_a[1] = overlap[i].m_a[1];
			pending[j].m_b[2] = overlap[i].m_b[2];
			pending[j].m_b[3] = overlap[i].m_b[3];
			pending[j].m_A[1] = overlap[i].m_A[1];
			pending[j].m_B[1] = overlap[i].m_B[1];
		    }
		    break;
		}
	    }
	}
	if (merged == false)
	    pending.Append(overlap[i]);
    }
    for (int i = 0; i < pending.Count(); i++) {
	x.Append(pending[i]);
    }

    // The intersection points shouldn't be inside the overlapped parts.
    int overlap_events = x.Count();
    for (int i = 0; i < points.Count(); i++) {
	int j;
	for (j = 0; j < overlap_events; j++) {
	    if (points[i].m_a[0] > x[j].m_a[0] - t_tolerance
		&& points[i].m_a[0] < x[j].m_a[1] + t_tolerance)
		break;
	}
	if (j == overlap_events)
	    x.Append(points[i]);
    }

    return x.Count();
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
 * - Fit the intersection points into polyline curves, and then to NURBS
 *   curves. Points with distance less than max_dis are considered in
 *   one curve.
 *
 * See: Adarsh Krishnamurthy, Rahul Khardekar, Sara McMains, Kirk Haller,
 * and Gershon Elber. 2008.
 * Performing efficient NURBS modeling operations on the GPU.
 * In Proceedings of the 2008 ACM symposium on Solid and physical modeling
 * (SPM '08). ACM, New York, NY, USA, 257-268. DOI=10.1145/1364901.1364937
 * http://doi.acm.org/10.1145/1364901.1364937
 */


struct Triangle {
    ON_3dPoint A, B, C;
    inline void CreateFromPoints(ON_3dPoint &p_A, ON_3dPoint &p_B, ON_3dPoint &p_C) {
	A = p_A;
	B = p_B;
	C = p_C;
    }
    ON_3dPoint BarycentricCoordinate(ON_3dPoint &pt)
    {
	double x, y, z, pivot_ratio;
	if (ON_Solve2x2(A[0]-C[0], B[0]-C[0], A[1]-C[1], B[1]-C[1], pt[0]-C[0], pt[1]-C[1], &x, &y, &pivot_ratio) == 2)
	    return ON_3dPoint(x, y, 1-x-y);
	if (ON_Solve2x2(A[0]-C[0], B[0]-C[0], A[2]-C[2], B[2]-C[2], pt[0]-C[0], pt[2]-C[2], &x, &z, &pivot_ratio) == 2)
	    return ON_3dPoint(x, 1-x-z, z);
	if (ON_Solve2x2(A[1]-C[1], B[1]-C[1], A[2]-C[2], B[2]-C[2], pt[1]-C[1], pt[2]-C[2], &y, &z, &pivot_ratio) == 2)
	    return ON_3dPoint(1-y-z, y, z);
	return ON_3dPoint::UnsetPoint;
    }
    void GetLineSegments(ON_Line line[3]) const
    {
	line[0] = ON_Line(A, B);
	line[1] = ON_Line(A, C);
	line[2] = ON_Line(B, C);
    }
};


bool
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
	if (!count)
	    return false;
	center = pt/count;
	return true;
    }

    if (!ON_Intersect(plane_a, plane_b, intersect))
	return false;
    ON_3dVector line_normal = ON_CrossProduct(plane_a.Normal(), intersect.Direction());

    // dpi - >0: one side of the line, <0: another side, ==0: on the line
    double dp1 = ON_DotProduct(TriA.A - intersect.from, line_normal);
    double dp2 = ON_DotProduct(TriA.B - intersect.from, line_normal);
    double dp3 = ON_DotProduct(TriA.C - intersect.from, line_normal);

    int points_on_line = ON_NearZero(dp1) + ON_NearZero(dp2) + ON_NearZero(dp3);
    int points_on_one_side = (dp1 >= ON_ZERO_TOLERANCE) + (dp2 >= ON_ZERO_TOLERANCE) + (dp3 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3-points_on_line)
	return false;

    line_normal = ON_CrossProduct(plane_b.Normal(), intersect.Direction());
    double dp4 = ON_DotProduct(TriB.A - intersect.from, line_normal);
    double dp5 = ON_DotProduct(TriB.B - intersect.from, line_normal);
    double dp6 = ON_DotProduct(TriB.C - intersect.from, line_normal);
    points_on_line = ON_NearZero(dp4) + ON_NearZero(dp5) + ON_NearZero(dp6);
    points_on_one_side = (dp4 >= ON_ZERO_TOLERANCE) + (dp5 >= ON_ZERO_TOLERANCE) + (dp6 >= ON_ZERO_TOLERANCE);
    if (points_on_one_side == 0 || points_on_one_side == 3-points_on_line)
	return false;

    double t[4];
    int count = 0;
    double t1, t2;
    // dpi*dpj < 0 - the corresponding points are on different sides
    // - the line segment between them are intersected by the plane-plane
    // intersection line
    if (dp1*dp2 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.B, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp1*dp3 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp2*dp3 < ON_ZERO_TOLERANCE && count != 2) {
	intersect.ClosestPointTo(TriA.B, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (count != 2)
	return false;
    if (dp4*dp5 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.B, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp4*dp6 < ON_ZERO_TOLERANCE) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp5*dp6 < ON_ZERO_TOLERANCE && count != 4) {
	intersect.ClosestPointTo(TriB.B, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (count != 4)
	return false;
    if (t[0] > t[1])
	std::swap(t[1], t[0]);
    if (t[2] > t[3])
	std::swap(t[3], t[2]);
    double left = std::max(t[0], t[2]);
    double right = std::min(t[1], t[3]);
    if (left > right)
	return false;
    center = intersect.PointAt((left+right)/2);
    return true;
}


struct PointPair {
    int indexA, indexB;
    double distance3d, distanceA, distanceB;
    bool operator < (const PointPair &_pp) const {
	return distance3d < _pp.distance3d;
    }
};


void
closed_domain(int type, const ON_Surface* surfA, const ON_Surface* surfB, ON_3dPointArray& curvept, ON_2dPointArray& curveuv, ON_2dPointArray& curvest)
{
    // type =
    // 0: uv.x, 1: uv.y, 2: st.x, 3: st.y

    ON_BOOL32 is_closed = (type < 2 ? surfA : surfB)->IsClosed(type%2);
    const ON_Interval domain = (type < 2 ? surfA : surfB)->Domain(type%2);
    if (!is_closed)
	return;

    int count = curvept.Count();
    for (int i = 0; i < count; i++) {
	ON_3dPoint pt3d = ON_3dPoint::UnsetPoint;
	ON_2dPoint ptuv, ptst;
	ptuv = curveuv[i];
	ptst = curvest[i];
	double& to_compare = (type < 2 ? ptuv : ptst)[type%2];
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


// The maximal depth for subdivision - trade-off between accurancy and
// performance
#define MAX_SSI_DEPTH 8


// We make the default tolerance for SSI the same as that of surface-surface
// intersections defined by openNURBS (see other/openNURBS/opennurbs_surface.h)
#define SSI_DEFAULT_TOLERANCE 0.001


int
ON_Intersect(const ON_Surface* surfA,
	     const ON_Surface* surfB,
	     ON_ClassArray<ON_SSX_EVENT>& x,
	     double intersection_tolerance,
	     double overlap_tolerance,
	     double fitting_tolerance,
	     const ON_Interval* surfaceA_udomain,
	     const ON_Interval* surfaceA_vdomain,
	     const ON_Interval* surfaceB_udomain,
	     const ON_Interval* surfaceB_vdomain)
{
    if (surfA == NULL || surfB == NULL) {
	return -1;
    }

    if (intersection_tolerance <= 0.0)
	intersection_tolerance = SSI_DEFAULT_TOLERANCE;
    if (overlap_tolerance < intersection_tolerance)
	overlap_tolerance = 2*intersection_tolerance;
    if (fitting_tolerance < intersection_tolerance)
	fitting_tolerance = intersection_tolerance;

    /* First step: Initialize the first two Subsurface.
     * It's just like getting the root of the surface tree.
     */
    typedef std::vector<std::pair<Subsurface*, Subsurface*> > NodePairs;
    NodePairs candidates, next_candidates;
    Subsurface rootA, rootB;
    build_surface_root(surfA, surfaceA_udomain, surfaceA_vdomain, rootA);
    build_surface_root(surfB, surfaceB_udomain, surfaceB_vdomain, rootB);
    if (rootA.Intersect(rootB, intersection_tolerance))
	candidates.push_back(std::make_pair(&rootA, &rootB));
    else
	return 0;

    // We adjust the tolerance from 3D scale to respective 2D scales.
    double intersection_tolerance_A = intersection_tolerance;
    double intersection_tolerance_B = intersection_tolerance;
    double fitting_tolerance_A = fitting_tolerance;
    double fitting_tolerance_B = fitting_tolerance;
    double l = rootA.m_surf->BoundingBox().Diagonal().Length();
    double ul = surfaceA_udomain ? surfaceA_udomain->Length() : surfA->Domain(0).Length();
    double vl = surfaceA_vdomain ? surfaceA_vdomain->Length() : surfA->Domain(1).Length();
    double dl = ON_2dVector(ul, vl).Length();
    if (!ON_NearZero(l)) {
	intersection_tolerance_A = intersection_tolerance/l*dl;
	fitting_tolerance_A = fitting_tolerance/l*dl;
    }
    ul = surfaceB_udomain ? surfaceB_udomain->Length() : surfB->Domain(0).Length();
    vl = surfaceB_vdomain ? surfaceB_vdomain->Length() : surfB->Domain(1).Length();
    dl = ON_2dVector(ul, vl).Length();
    if (!ON_NearZero(l)) {
	intersection_tolerance_B = intersection_tolerance/l*dl;
	fitting_tolerance_B = fitting_tolerance/l*dl;
    }

    ON_3dPointArray curvept, tmp_curvept;
    ON_2dPointArray curveuv, curvest, tmp_curveuv, tmp_curvest;

    // Deal with boundaries with curve-surface intersections.
    for (int i = 0; i < 4; i++) {
	const ON_Surface* surf1 = i >= 2 ? surfB : surfA;
	const ON_Surface* surf2 = i >= 2 ? surfA : surfB;
	ON_2dPointArray& ptarray1 = i >= 2 ? tmp_curvest : tmp_curveuv;
	ON_2dPointArray& ptarray2 = i >= 2 ? tmp_curveuv : tmp_curvest;
	ON_Curve* boundary = surf1->IsoCurve(i%2, surf1->Domain(1-i%2).Min());
	ON_SimpleArray<ON_X_EVENT> x_event;
	ON_Intersect(boundary, surf2, x_event, intersection_tolerance);
	for (int j = 0; j < x_event.Count(); j++) {
	    tmp_curvept.Append(x_event[j].m_A[0]);
	    ON_2dPoint iso_pt;
	    iso_pt.x = i%2 ? surf1->Domain(0).Min() : x_event[j].m_a[0];
	    iso_pt.y = i%2 ? x_event[j].m_a[0] : surf1->Domain(1).Min();
	    ptarray1.Append(iso_pt);
	    ptarray2.Append(ON_2dPoint(x_event[j].m_b[0], x_event[j].m_b[1]));
	    if (x_event[j].m_type == ON_X_EVENT::csx_overlap) {
		// Append the other end-point
		tmp_curvept.Append(x_event[j].m_A[1]);
		iso_pt.x = i%2 ? surf1->Domain(0).Min() : x_event[j].m_a[1];
		iso_pt.y = i%2 ? x_event[j].m_a[1] : surf1->Domain(1).Min();
		ptarray1.Append(iso_pt);
		ptarray2.Append(ON_2dPoint(x_event[j].m_b[2], x_event[j].m_b[3]));
	    }
	}
	if (!surf1->IsClosed(1-i%2)) {
	    x_event.Empty();
	    boundary = surf1->IsoCurve(i%2, surf1->Domain(1-i%2).Max());
	    ON_Intersect(boundary, surf2, x_event, intersection_tolerance);
	    for (int j = 0; j < x_event.Count(); j++) {
		tmp_curvept.Append(x_event[j].m_A[0]);
		ON_2dPoint iso_pt;
		iso_pt.x = i%2 ? surf1->Domain(0).Max() : x_event[j].m_a[0];
		iso_pt.y = i%2 ? x_event[j].m_a[0] : surf1->Domain(1).Max();
		ptarray1.Append(iso_pt);
		ptarray2.Append(ON_2dPoint(x_event[j].m_b[0], x_event[j].m_b[1]));
		if (x_event[j].m_type == ON_X_EVENT::csx_overlap) {
		    tmp_curvept.Append(x_event[j].m_A[1]);
		    iso_pt.x = i%2 ? surf1->Domain(0).Max() : x_event[j].m_a[1];
		    iso_pt.y = i%2 ? x_event[j].m_a[1] : surf1->Domain(1).Max();
		    ptarray1.Append(iso_pt);
		    ptarray2.Append(ON_2dPoint(x_event[j].m_b[2], x_event[j].m_b[3]));
		}
	    }
	}
    }

    /* Second step: calculate the intersection of the bounding boxes.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     */
    for (int h = 0; h <= MAX_SSI_DEPTH; h++) {
	if (candidates.empty())
	    break;
	next_candidates.clear();
	for (NodePairs::iterator i = candidates.begin(); i != candidates.end(); i++) {
	    std::vector<Subsurface*> splittedA, splittedB;
	    if ((*i).first->Split() != 0) {
		splittedA.push_back((*i).first);
	    } else {
		for (int j = 0; j < 4; j++)
		    splittedA.push_back((*i).first->m_children[j]);
	    }
	    if ((*i).second->Split() != 0) {
		splittedB.push_back((*i).second);
	    } else {
		for (int j = 0; j < 4; j++)
		    splittedB.push_back((*i).second->m_children[j]);
	    }
	    for (unsigned int j = 0; j < splittedA.size(); j++)
		for (unsigned int k = 0; k < splittedB.size(); k++)
		    if (splittedB[k]->Intersect(*splittedA[j], intersection_tolerance))
			next_candidates.push_back(std::make_pair(splittedA[j], splittedB[k]));
	}
	candidates = next_candidates;
    }
    bu_log("We get %d intersection bounding boxes.\n", candidates.size());

    /* Third step: get the intersection points using triangular approximation. */
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
	    uv[0].x = bcoor[0].x*u_min + bcoor[0].y*u_min + bcoor[0].z*u_max;
	    uv[0].y = bcoor[0].x*v_min + bcoor[0].y*v_max + bcoor[0].z*v_min;
	    st[0].x = bcoor[1].x*s_min + bcoor[1].y*s_min + bcoor[1].z*s_max;
	    st[0].y = bcoor[1].x*t_min + bcoor[1].y*t_max + bcoor[1].z*t_min;
	}
	if (is_intersect[1]) {
	    bcoor[2] = triangle[0].BarycentricCoordinate(intersect_center[1]);
	    bcoor[3] = triangle[3].BarycentricCoordinate(intersect_center[1]);
	    uv[1].x = bcoor[2].x*u_min + bcoor[2].y*u_min + bcoor[2].z*u_max;
	    uv[1].y = bcoor[2].x*v_min + bcoor[2].y*v_max + bcoor[2].z*v_min;
	    st[1].x = bcoor[3].x*s_min + bcoor[3].y*s_max + bcoor[3].z*s_max;
	    st[1].y = bcoor[3].x*t_max + bcoor[3].y*t_min + bcoor[3].z*t_max;
	}
	if (is_intersect[2]) {
	    bcoor[4] = triangle[1].BarycentricCoordinate(intersect_center[2]);
	    bcoor[5] = triangle[2].BarycentricCoordinate(intersect_center[2]);
	    uv[2].x = bcoor[4].x*u_min + bcoor[4].y*u_max + bcoor[4].z*u_max;
	    uv[2].y = bcoor[4].x*v_max + bcoor[4].y*v_min + bcoor[4].z*v_max;
	    st[2].x = bcoor[5].x*s_min + bcoor[5].y*s_min + bcoor[5].z*s_max;
	    st[2].y = bcoor[5].x*t_min + bcoor[5].y*t_max + bcoor[5].z*t_min;
	}
	if (is_intersect[3]) {
	    bcoor[6] = triangle[1].BarycentricCoordinate(intersect_center[3]);
	    bcoor[7] = triangle[3].BarycentricCoordinate(intersect_center[3]);
	    uv[3].x = bcoor[6].x*u_min + bcoor[6].y*u_max + bcoor[6].z*u_max;
	    uv[3].y = bcoor[6].x*v_max + bcoor[6].y*v_min + bcoor[6].z*v_max;
	    st[3].x = bcoor[7].x*s_min + bcoor[7].y*s_max + bcoor[7].z*s_max;
	    st[3].y = bcoor[7].x*t_max + bcoor[7].y*t_min + bcoor[7].z*t_max;
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
		// Check the validity of the solution
		ON_3dPoint pointA = surfA->PointAt(avguv.x, avguv.y);
		ON_3dPoint pointB = surfB->PointAt(avgst.x, avgst.y);
		if (pointA.DistanceTo(pointB) < intersection_tolerance
		    && pointA.DistanceTo(average) < intersection_tolerance
		    && pointB.DistanceTo(average) < intersection_tolerance) {
		    tmp_curvept.Append(average);
		    tmp_curveuv.Append(avguv);
		    tmp_curvest.Append(avgst);
		}
	    }
	}
    }
    for (int i = 0; i < 4; i++)
	closed_domain(i, surfA, surfB, tmp_curvept, tmp_curveuv, tmp_curvest);

    // Use an O(n^2) naive approach to eliminate duplication
    for (int i = 0; i < tmp_curvept.Count(); i++) {
	int j;
	for (j = 0; j < curvept.Count(); j++)
	    if (tmp_curvept[i].DistanceTo(curvept[j]) < intersection_tolerance
		&& tmp_curveuv[i].DistanceTo(curveuv[j]) < intersection_tolerance_A
		&& tmp_curvest[i].DistanceTo(curvest[j]) < intersection_tolerance_B)
		break;
	// TODO: Use 2D tolerance
	if (j == curvept.Count()) {
	    curvept.Append(tmp_curvept[i]);
	    curveuv.Append(tmp_curveuv[i]);
	    curvest.Append(tmp_curvest[i]);
	}
    }
    bu_log("%d points on the intersection curves.\n", curvept.Count());

    if (!curvept.Count()) {
	return 0;
    }

    // Fourth step: Fit the points in curvept into NURBS curves.
    // Here we use polyline approximation.
    // TODO: Find a better fitting algorithm unless this is good enough.
    // Time complexity: O(n^2*log(n)), really slow when n is large.
    // (n is the number of points generated above)

    // We need to automatically generate a threshold.
    double max_dis = sqrt(rootA.m_surf->BoundingBox().Diagonal().Length()*rootB.m_surf->BoundingBox().Diagonal().Length()) * 0.1;

    double max_dis_2dA = ON_2dVector(surfA->Domain(0).Length(), surfA->Domain(1).Length()).Length() * 0.1;
    double max_dis_2dB = ON_2dVector(surfB->Domain(0).Length(), surfB->Domain(1).Length()).Length() * 0.1;
    bu_log("max_dis: %lf\n", max_dis);
    bu_log("max_dis_2dA: %lf\n", max_dis_2dA);
    bu_log("max_dis_2dB: %lf\n", max_dis_2dB);
    // NOTE: More tests are needed to find a better threshold.

    std::vector<PointPair> ptpairs;
    for (int i = 0; i < curvept.Count(); i++) {
	for (int j = i + 1; j < curvept.Count(); j++) {
	    PointPair ppair;
	    ppair.distance3d = curvept[i].DistanceTo(curvept[j]);
	    ppair.distanceA = curveuv[i].DistanceTo(curveuv[j]);
	    ppair.distanceB = curvest[i].DistanceTo(curvest[j]);
	    if (ppair.distanceA < max_dis_2dA && ppair.distanceB < max_dis_2dB && ppair.distance3d < max_dis) {
		ppair.indexA = i;
		ppair.indexB = j;
		ptpairs.push_back(ppair);
	    }
	}
    }
    std::sort(ptpairs.begin(), ptpairs.end());

    std::vector<ON_SimpleArray<int>*> polylines(curvept.Count());
    int *index = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
    // index[i] = j means curvept[i] is a startpoint/endpoint of polylines[j]
    int *startpt = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
    int *endpt = (int*)bu_malloc(curvept.Count() * sizeof(int), "int");
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
	if (index1 == -1 || index2 == -1)
	    continue;
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

    // Generate NURBS curves from the polylines.
    ON_SimpleArray<ON_Curve *> intersect3d, intersect_uv2d, intersect_st2d;
    ON_SimpleArray<int> single_pts;
    for (unsigned int i = 0; i < polylines.size(); i++) {
	if (polylines[i] != NULL) {
	    int startpoint = (*polylines[i])[0];
	    int endpoint = (*polylines[i])[polylines[i]->Count() - 1];

	    if (polylines[i]->Count() == 1) {
		// It's a single point
		single_pts.Append(startpoint);
		continue;
	    }

	    // The intersection curves in the 3d space
	    ON_3dPointArray ptarray;
	    for (int j = 0; j < polylines[i]->Count(); j++)
		ptarray.Append(curvept[(*polylines[i])[j]]);
	    // If it form a loop in the 3D space
	    if (curvept[startpoint].DistanceTo(curvept[endpoint]) < max_dis) {
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
	    if (curveuv[startpoint].DistanceTo(curveuv[endpoint]) < max_dis_2dA) {
		ON_2dPoint &pt2d = curveuv[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = new ON_PolylineCurve(ptarray);
	    curve->ChangeDimension(2);
	    if (curve->IsLinear(fitting_tolerance_A)) {
		ON_LineCurve *linecurve = new ON_LineCurve(curve->PointAtStart(), curve->PointAtEnd());
		linecurve->ChangeDimension(2);
		intersect_uv2d.Append(linecurve);
	    } else {
		intersect_uv2d.Append(curve);
	    }

	    // The intersection curves in the 2d UV parameter space (surfB)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curvest[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    // If it form a loop in the 2D space (happens rarely compared to 3D)
	    if (curvest[startpoint].DistanceTo(curvest[endpoint]) < max_dis_2dB) {
		ON_2dPoint &pt2d = curvest[startpoint];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = new ON_PolylineCurve(ptarray);
	    curve->ChangeDimension(2);
	    if (curve->IsLinear(fitting_tolerance_B)) {
		ON_LineCurve *linecurve = new ON_LineCurve(curve->PointAtStart(), curve->PointAtEnd());
		linecurve->ChangeDimension(2);
		intersect_st2d.Append(linecurve);
	    } else {
		intersect_st2d.Append(curve);
	    }

	    delete polylines[i];
	}
    }

    bu_log("Segments: %d\n", intersect3d.Count());
    bu_free(index, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");

    // Generate ON_SSX_EVENTs
    if (intersect3d.Count()) {
	for (int i = 0; i < intersect3d.Count(); i++) {
	    ON_SSX_EVENT *ssx = new ON_SSX_EVENT;
	    ssx->m_curve3d = intersect3d[i];
	    ssx->m_curveA = intersect_uv2d[i];
	    ssx->m_curveB = intersect_st2d[i];
	    // Now we can only have ssx_transverse
	    ssx->m_type = ON_SSX_EVENT::ssx_transverse;
	    x.Append(*ssx);
	}
    }

    for (int i = 0; i < single_pts.Count(); i++) {
	// Check duplication (point-curve intersection)
	int j;
	for (j = 0; j < intersect3d.Count(); j++) {
	    ON_ClassArray<ON_PX_EVENT> px_event;
	    if (ON_Intersect(curvept[single_pts[i]], *intersect3d[j], px_event))
		break;
	}
	if (j == intersect3d.Count()) {
	    ON_SSX_EVENT *ssx = new ON_SSX_EVENT;
	    ssx->m_point3d = curvept[single_pts[i]];
	    ssx->m_pointA = curveuv[single_pts[i]];
	    ssx->m_pointB = curvest[single_pts[i]];
	    ssx->m_type = ON_SSX_EVENT::ssx_transverse_point;
	    x.Append(*ssx);
	}
    }

    return x.Count();
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
