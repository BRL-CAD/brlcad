/*                  B O O L E A N . C P P
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
/** @file boolean.cpp
 *
 * Evaluate NURBS booleans (union, intersection and difference).
 *
 */

#include "common.h"

#include <assert.h>
#include <vector>
#include <stack>
#include <queue>
#include <set>
#include <map>
#include <algorithm>
#include <sstream>

#include "vmath.h"
#include "bio.h"
#include "bu/log.h"
#include "brep.h"
#include "raytrace.h"
#include "debug_plot.h"
#include "brep_except.h"

DebugPlot *dplot = NULL;

// Whether to output the debug messages about b-rep booleans.
#define DEBUG_BREP_BOOLEAN 0

// tol value used in ON_Intersect()s. We use a smaller tolerance than the
// default one 0.001.
#define INTERSECTION_TOL 1e-4

// tol value used in ON_3dVector::IsParallelTo(). We use a smaller tolerance
// than the default one ON_PI/180.
#define ANGLE_TOL ON_PI/1800.0

struct IntersectPoint {
    ON_3dPoint m_pt;	// 3D intersection point
    double m_seg_t;	// param on the loop curve
    int m_loop_seg;	// which curve of the loop
    int m_ssx_curve;	// which intersection curve
    int m_curve_pos;	// rank on the chain
    double m_curve_t;	// param on the SSI curve
    enum {
	UNSET,
	IN,
	OUT,
	TANGENT
    } m_dir;		// dir is going inside/outside
    int m_split_li;	// between clx_points[m_split_li] and
			// clx_points[m_split_li+1]
			// after the outerloop is split
};


// A structure to represent the curve segments generated from surface-surface
// intersections, including some information needed by the connectivity graph
struct SSICurve {
    ON_Curve *m_curve;

    SSICurve()
    {
	m_curve = NULL;
    }

    SSICurve(ON_Curve *curve)
    {
	m_curve = curve;
    }

    SSICurve *Duplicate() const
    {
	SSICurve *out = new SSICurve();
	if (out != NULL) {
	    *out = *this;
	    out->m_curve = m_curve->Duplicate();
	}
	return out;
    }
};

enum seam_location {SEAM_NONE, SEAM_ALONG_V, SEAM_ALONG_U, SEAM_ALONG_UV};

HIDDEN double
shortest_parameter_distance(
    const ON_Surface *surf,
    const ON_2dPoint &uv1,
    const ON_2dPoint &uv2)
{
    int seam1 = IsAtSeam(surf, uv1, ON_ZERO_TOLERANCE);
    int seam2 = IsAtSeam(surf, uv2, ON_ZERO_TOLERANCE);

    // TODO: check for singularity, SEAM_ALONG_UV
    if (seam1 != SEAM_NONE && seam1 == seam2) {
	if (seam1 == SEAM_ALONG_V) {
	    return fabs(uv1.y - uv2.y);
	} else if (seam1 == SEAM_ALONG_U) {
	    return fabs(uv1.x - uv2.x);
	}
    }
    return uv1.DistanceTo(uv2);
}

HIDDEN void
append_to_polycurve(ON_Curve *curve, ON_PolyCurve &polycurve);
// We link the SSICurves that share an endpoint, and form this new structure,
// which has many similar behaviors as ON_Curve, e.g. PointAt(), Reverse().
struct LinkedCurve {
private:
    ON_Curve *m_curve;	// an explicit storage of the whole curve
public:
    int m_dim;		// curve dimension
    const ON_Surface *m_surf;

    // The curves contained in this LinkedCurve, including
    // the information needed by the connectivity graph
    ON_SimpleArray<SSICurve> m_ssi_curves;

    // Default constructor
    LinkedCurve()
    {
	m_curve = NULL;
	m_surf = NULL;
	m_dim = 2;
    }

    void Empty()
    {
	m_ssi_curves.Empty();
	delete m_curve;
	m_curve = NULL;
	m_surf = NULL;
    }

    ~LinkedCurve()
    {
	Empty();
    }

    LinkedCurve &operator= (const LinkedCurve &_lc)
    {
	Empty();
	m_curve = _lc.m_curve ? _lc.m_curve->Duplicate() : NULL;
	m_dim = _lc.m_dim;
	m_surf = _lc.m_surf;
	m_ssi_curves = _lc.m_ssi_curves;
	return *this;
    }

    ON_3dPoint PointAtStart() const
    {
	if (m_ssi_curves.Count()) {
	    return m_ssi_curves[0].m_curve->PointAtStart();
	} else {
	    return ON_3dPoint::UnsetPoint;
	}
    }

    ON_3dPoint PointAtEnd() const
    {
	if (m_ssi_curves.Count()) {
	    return m_ssi_curves.Last()->m_curve->PointAtEnd();
	} else {
	    return ON_3dPoint::UnsetPoint;
	}
    }

    bool IsClosed(double dist_tol = ON_ZERO_TOLERANCE, bool wrap = true) const
    {
	if (m_ssi_curves.Count() == 0) {
	    return false;
	}

	ON_3dPoint start_pt = PointAtStart();
	ON_3dPoint end_pt = PointAtEnd();

	if (start_pt == ON_3dPoint::UnsetPoint ||
	    end_pt == ON_3dPoint::UnsetPoint)
	{
	    return false;
	}

	double dist = 0.0;
	if (wrap && m_dim == 2 && m_surf != NULL) {
	    ON_2dPoint start_uv(start_pt.x, start_pt.y);
	    ON_2dPoint end_uv(end_pt.x, end_pt.y);

	    dist = shortest_parameter_distance(m_surf, start_uv, end_uv);
	} else {
	    dist = PointAtStart().DistanceTo(PointAtEnd());
	}
	return dist < dist_tol;
    }

    bool IsValid(double dist_tol = ON_ZERO_TOLERANCE, bool wrap = true) const
    {
	// Check whether the curve has "gaps".
	double gap = 0.0;
	for (int i = 1; i < m_ssi_curves.Count(); i++) {
	    ON_3dPoint start_curr = m_ssi_curves[i].m_curve->PointAtStart();
	    ON_3dPoint end_prev = m_ssi_curves[i - 1].m_curve->PointAtEnd();

	    if (wrap && m_dim == 2 && m_surf != NULL) {
		ON_2dPoint start_uv(start_curr.x, start_curr.y);
		ON_2dPoint end_uv(end_prev.x, end_prev.y);

		gap = shortest_parameter_distance(m_surf, start_uv, end_uv);
	    } else {
		gap = start_curr.DistanceTo(end_prev);
	    }

	    if (gap >= dist_tol) {
		bu_log("The LinkedCurve is not valid.\n");
		return false;
	    }
	}
	return true;
    }

    bool Reverse()
    {
	ON_SimpleArray<SSICurve> new_array;
	for (int i = m_ssi_curves.Count() - 1; i >= 0; i--) {
	    if (!m_ssi_curves[i].m_curve->Reverse()) {
		return false;
	    }
	    new_array.Append(m_ssi_curves[i]);
	}
	m_ssi_curves = new_array;
	return true;
    }

    void Append(const LinkedCurve &lc)
    {
	m_ssi_curves.Append(lc.m_ssi_curves.Count(), lc.m_ssi_curves.Array());
    }

    void Append(const SSICurve &sc)
    {
	m_ssi_curves.Append(sc);
    }

    void AppendCurvesToArray(ON_SimpleArray<ON_Curve *> &arr) const
    {
	for (int i = 0; i < m_ssi_curves.Count(); i++) {
	    arr.Append(m_ssi_curves[i].m_curve->Duplicate());
	}
    }

    const ON_Curve *Curve()
    {
	if (m_curve != NULL) {
	    return m_curve;
	}
	if (m_ssi_curves.Count() == 0 || !IsValid()) {
	    return NULL;
	}
	ON_PolyCurve *polycurve = new ON_PolyCurve;
	for (int i = 0; i < m_ssi_curves.Count(); i++) {
	    append_to_polycurve(m_ssi_curves[i].m_curve->Duplicate(), *polycurve);
	}
	m_curve = polycurve;
	return m_curve;
    }

    const ON_3dPoint PointAt(double t)
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return ON_3dPoint::UnsetPoint;
	}
	return c->PointAt(t);
    }

    const ON_Interval Domain()
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return ON_Interval::EmptyInterval;
	}
	return c->Domain();
    }

    ON_Curve *SubCurve(double t1, double t2)
    {
	const ON_Curve *c = Curve();
	if (c == NULL) {
	    return NULL;
	}
	try {
	    return sub_curve(c, t1, t2);
	} catch (InvalidInterval &e) {
	    bu_log("%s", e.what());
	    return NULL;
	}
    }
};


struct TrimmedFace {
    // curve segments in the face's outer loop
    ON_SimpleArray<ON_Curve *> m_outerloop;
    // several inner loops, each has some curves
    std::vector<ON_SimpleArray<ON_Curve *> > m_innerloop;
    const ON_BrepFace *m_face;
    enum {
	UNKNOWN = -1,
	NOT_BELONG = 0,
	BELONG = 1
    } m_belong_to_final;
    bool m_rev;

    // Default constructor
    TrimmedFace()
    {
	m_face = NULL;
	m_belong_to_final = UNKNOWN;
	m_rev = false;
    }

    // Destructor
    ~TrimmedFace()
    {
	// Delete the curve segments if it's not belong to the result.
	if (m_belong_to_final != BELONG) {
	    for (int i = 0; i < m_outerloop.Count(); i++) {
		if (m_outerloop[i]) {
		    delete m_outerloop[i];
		    m_outerloop[i] = NULL;
		}
	    }
	    for (unsigned int i = 0; i < m_innerloop.size(); i++) {
		for (int j = 0; j < m_innerloop[i].Count(); j++) {
		    if (m_innerloop[i][j]) {
			delete m_innerloop[i][j];
			m_innerloop[i][j] = NULL;
		    }
		}
	    }
	}
    }

    TrimmedFace *Duplicate() const
    {
	TrimmedFace *out = new TrimmedFace();
	out->m_face = m_face;
	for (int i = 0; i < m_outerloop.Count(); i++) {
	    if (m_outerloop[i]) {
		out->m_outerloop.Append(m_outerloop[i]->Duplicate());
	    }
	}
	out->m_innerloop = m_innerloop;
	for (unsigned int i = 0; i < m_innerloop.size(); i++) {
	    for (int j = 0; j < m_innerloop[i].Count(); j++) {
		if (m_innerloop[i][j]) {
		    out->m_innerloop[i][j] = m_innerloop[i][j]->Duplicate();
		}
	    }
	}
	return out;
    }
};

HIDDEN int
loop_t_compare(const IntersectPoint *p1, const IntersectPoint *p2)
{
    // Use for sorting an array. Use strict FP comparison.
    if (p1->m_loop_seg != p2->m_loop_seg) {
	return p1->m_loop_seg - p2->m_loop_seg;
    }
    return p1->m_seg_t > p2->m_seg_t ? 1 : (p1->m_seg_t < p2->m_seg_t ? -1 : 0);
}


HIDDEN int
curve_t_compare(const IntersectPoint *p1, const IntersectPoint *p2)
{
    // Use for sorting an array. Use strict FP comparison.
    return p1->m_curve_t > p2->m_curve_t ? 1 : (p1->m_curve_t < p2->m_curve_t ? -1 : 0);
}


void
append_to_polycurve(ON_Curve *curve, ON_PolyCurve &polycurve)
{
    // use this function rather than ON_PolyCurve::Append() to avoid
    // getting nested polycurves, which makes ON_Brep::IsValid() to fail.

    ON_PolyCurve *nested = ON_PolyCurve::Cast(curve);
    if (nested != NULL) {
	// The input curve is a polycurve
	const ON_CurveArray &segments = nested->SegmentCurves();
	for (int i = 0; i < segments.Count(); i++) {
	    append_to_polycurve(segments[i]->Duplicate(), polycurve);
	}
	delete nested;
    } else {
	polycurve.Append(curve);
    }
}


HIDDEN bool
is_loop_valid(const ON_SimpleArray<ON_Curve *> &loop, double tolerance, ON_PolyCurve *polycurve = NULL)
{
    bool delete_curve = false;
    bool ret = true;

    if (loop.Count() == 0) {
	bu_log("The input loop is empty.\n");
	ret = false;
    }

    // First, use a ON_PolyCurve to represent the loop.
    if (ret) {
	if (polycurve == NULL) {
	    polycurve = new ON_PolyCurve;
	    if (polycurve) {
		delete_curve = true;
	    } else {
		ret = false;
	    }
	}
    }

    // Check the loop is continuous and closed or not.
    if (ret) {
	if (loop[0] != NULL) {
	    append_to_polycurve(loop[0]->Duplicate(), *polycurve);
	}
	for (int i = 1 ; i < loop.Count(); i++) {
	    if (loop[i] && loop[i - 1] && loop[i]->PointAtStart().DistanceTo(loop[i - 1]->PointAtEnd()) < ON_ZERO_TOLERANCE) {
		append_to_polycurve(loop[i]->Duplicate(), *polycurve);
	    } else {
		bu_log("The input loop is not continuous.\n");
		ret = false;
	    }
	}
    }
    if (ret && (polycurve->PointAtStart().DistanceTo(polycurve->PointAtEnd()) >= ON_ZERO_TOLERANCE ||
	!polycurve->IsClosed()))
    {
	bu_log("The input loop is not closed.\n");
	ret = false;
    }

    if (ret) {
	// Check whether the loop is degenerated.
	ON_BoundingBox bbox = polycurve->BoundingBox();
	ret = !ON_NearZero(bbox.Diagonal().Length(), tolerance)
	      && !polycurve->IsLinear(tolerance);
    }

    if (delete_curve) {
	delete polycurve;
    }

    return ret;
}

enum {
    OUTSIDE_OR_ON_LOOP,
    INSIDE_OR_ON_LOOP
};

//   Returns whether the point is inside/on or outside/on the loop
//   boundary.
//
//   Throws InvalidGeometry if loop is invalid.
//
//   If you want to know whether this point is on the loop boundary,
//   call is_point_on_loop().
HIDDEN int
point_loop_location(const ON_2dPoint &pt, const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_PolyCurve polycurve;
    if (!is_loop_valid(loop, ON_ZERO_TOLERANCE, &polycurve)) {
	throw InvalidGeometry("point_loop_location() given invalid loop\n");
    }

    ON_BoundingBox bbox = polycurve.BoundingBox();
    if (!bbox.IsPointIn(pt)) {
	return OUTSIDE_OR_ON_LOOP;
    }

    // The input point is inside the loop's bounding box.
    // out must be outside the closed region (and the bbox).
    ON_2dPoint out = pt + ON_2dVector(bbox.Diagonal());
    ON_LineCurve linecurve(pt, out);
    ON_3dVector line_dir = linecurve.m_line.Direction();

    ON_SimpleArray<ON_X_EVENT> tmp_x;
    for (int i = 0; i < loop.Count(); ++i) {
	ON_SimpleArray<ON_X_EVENT> li_x;
	ON_Intersect(&linecurve, loop[i], li_x, INTERSECTION_TOL);

	for (int j = 0; j < li_x.Count(); ++j) {
	    // ignore tangent and overlap intersections
	    if (li_x[j].m_type != ON_X_EVENT::ccx_overlap &&
		!loop[i]->TangentAt(li_x[j].m_b[0]).IsParallelTo(line_dir, ANGLE_TOL))
	    {
		tmp_x.Append(li_x[j]);
	    }
	}
    }
    ON_SimpleArray<ON_X_EVENT> x_event;
    for (int i = 0; i < tmp_x.Count(); i++) {
	int j;
	for (j = 0; j < x_event.Count(); j++) {
	    if (tmp_x[i].m_A[0].DistanceTo(x_event[j].m_A[0]) < INTERSECTION_TOL &&
		tmp_x[i].m_A[1].DistanceTo(x_event[j].m_A[1]) < INTERSECTION_TOL &&
		tmp_x[i].m_B[0].DistanceTo(x_event[j].m_B[0]) < INTERSECTION_TOL &&
		tmp_x[i].m_B[1].DistanceTo(x_event[j].m_B[1]) < INTERSECTION_TOL)
	    {
		break;
	    }
	}
	if (j == x_event.Count()) {
	    x_event.Append(tmp_x[i]);
	}
    }

    return (x_event.Count() % 2) ? INSIDE_OR_ON_LOOP : OUTSIDE_OR_ON_LOOP;
}


// Returns whether or not point is on the loop boundary.
// Throws InvalidGeometry if loop is invalid.
HIDDEN bool
is_point_on_loop(const ON_2dPoint &pt, const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_PolyCurve polycurve;
    if (!is_loop_valid(loop, ON_ZERO_TOLERANCE, &polycurve)) {
	throw InvalidGeometry("is_point_on_loop() given invalid loop\n");
    }

    ON_3dPoint pt3d(pt);
    for (int i = 0; i < loop.Count(); ++i) {
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (ON_Intersect(pt3d, *loop[i], px_event, INTERSECTION_TOL)) {
	    return true;
	}
    }
    return false;
}

HIDDEN bool
is_point_inside_loop(const ON_2dPoint &pt, const ON_SimpleArray<ON_Curve *> &loop)
{
    return (point_loop_location(pt, loop) == INSIDE_OR_ON_LOOP) && !is_point_on_loop(pt, loop);
}

HIDDEN bool
is_point_outside_loop(const ON_2dPoint &pt, const ON_SimpleArray<ON_Curve *> &loop)
{
    return (point_loop_location(pt, loop) == OUTSIDE_OR_ON_LOOP) && !is_point_on_loop(pt, loop);
}

HIDDEN ON_Interval
union_intervals(const ON_SimpleArray<ON_Interval> &intervals)
{
    ON_Interval union_interval;
    for (int i = 0; i < intervals.Count(); ++i) {
	union_interval.Union(intervals[i]);
    }
    if (!union_interval.IsValid()) {
	throw IntervalGenerationError("union_intervals() created invalid interval\n");
    }
    return union_interval;
}

HIDDEN ON_Interval
intersect_intervals(const ON_Interval &interval1, const ON_Interval &interval2)
{
    ON_Interval intersection_interval;
    if (!intersection_interval.Intersection(interval1, interval2)) {
	throw IntervalGenerationError("intersect_intervals() failed to intersect intervals\n");
    }
    return intersection_interval;
}

HIDDEN void
replace_curve_with_subcurve(ON_Curve *&curve, const ON_Interval &interval)
{
    try {
	ON_Curve *subcurve = sub_curve(curve, interval.Min(), interval.Max());
	delete curve;
	curve = subcurve;
    } catch (InvalidInterval &e) {
	throw GeometryGenerationError("replace_curve_with_subcurve(): NULL "
		"subcurve\n");
    }
}

HIDDEN ON_SimpleArray<ON_Interval>
get_curve_intervals_inside_or_on_face(
    ON_Curve *curve2D,
    const ON_ClassArray<ON_SimpleArray<ON_Curve *> > &face_loops,
    double isect_tol)
{
    // get curve-loop intersections
    ON_SimpleArray<double> isect_curve_t;
    ON_SimpleArray<ON_X_EVENT> ccx_events;

    for (int i = 0; i < face_loops.Count(); ++i) {
	for (int j = 0; j < face_loops[i].Count(); ++j) {
	    ON_Intersect(curve2D, face_loops[i][j], ccx_events, isect_tol);
	}
    }

    // get a sorted list of just the parameters on the first curve
    // where it intersects the outerloop
    for (int i = 0; i < ccx_events.Count(); i++) {
	isect_curve_t.Append(ccx_events[i].m_a[0]);

	if (ccx_events[i].m_type == ON_X_EVENT::ccx_overlap) {
	    isect_curve_t.Append(ccx_events[i].m_a[1]);
	}
    }
    isect_curve_t.QuickSort(ON_CompareIncreasing);


    // insert start and end parameters so every part of the curve is tested
    isect_curve_t.Insert(0, curve2D->Domain().Min());
    isect_curve_t.Append(curve2D->Domain().Max());

    // if the midpoint of an interval is inside/on the face, keep the
    // entire interval
    ON_SimpleArray<ON_Interval> included_intervals;
    for (int i = 0; i < isect_curve_t.Count() - 1; i++) {
	ON_Interval interval(isect_curve_t[i], isect_curve_t[i + 1]);
	if (ON_NearZero(interval.Length(), ON_ZERO_TOLERANCE)) {
	    continue;
	}
	ON_2dPoint pt = curve2D->PointAt(interval.Mid());

	bool point_included = false;
	try {
	    // inside/on outerloop
	    if (!is_point_outside_loop(pt, face_loops[0])) {

		// outside/on innerloops
		point_included = true;
		for (int j = 1; j < face_loops.Count(); ++j) {
		    if (is_point_inside_loop(pt, face_loops[j])) {
			point_included = false;
			break;
		    }
		}
	    }
	} catch (InvalidGeometry &e) {
	    bu_log("%s", e.what());
	}
	if (point_included) {
	    included_intervals.Append(interval);
	}
    }

    // merge continuous intervals
    ON_SimpleArray<ON_Interval> final_intervals;
    for (int j, i = 0; i < included_intervals.Count(); i = j) {
	ON_Interval merged_interval = included_intervals[i];

	for (j = i + 1; j < included_intervals.Count(); ++j) {
	    ON_Interval &next = included_intervals[j];

	    if (ON_NearZero(next.Min() - merged_interval.Max(), ON_ZERO_TOLERANCE)) {
		ON_Interval new_interval = merged_interval;

		if (new_interval.Union(next)) {
		    merged_interval = new_interval;
		} else {
		    break;
		}
	    } else {
		break;
	    }
	}
	final_intervals.Append(merged_interval);
    }

    return final_intervals;
}

HIDDEN int
compare_interval(const ON_Interval *a, const ON_Interval *b)
{
    return a->Compare(*b);
}

struct IntervalPoints {
    ON_3dPoint min;
    ON_3dPoint mid;
    ON_3dPoint max;
};

class IntervalParams {
public:
    double min;
    double mid;
    double max;

    void
    MakeIncreasing(void)
    {
	if (min > mid) {
	    std::swap(min, mid);
	}
	if (mid > max) {
	    std::swap(mid, max);

	    if (min > mid) {
		std::swap(min, mid);
	    }
	}
    }
};

// given parameters in a curve interval, create new interval
// parameters that reflect the the non-degenerate (different
// dimensioned) curve interval used to generate the curve parameters
HIDDEN IntervalParams
curve_interval_from_params(
    IntervalParams interval_t,
    const ON_Curve *curve)
{
    if (!curve->IsClosed()) {
	return interval_t;
    }

    if (interval_t.min > interval_t.max) {
	std::swap(interval_t.min, interval_t.max);
    }
    double min_t = interval_t.min;
    double max_t = interval_t.max;

    ON_Interval cdom = curve->Domain();
    if (!(min_t < max_t || min_t > max_t)) {
	// if endpoints are both at closed curve joint, put them at
	// either end of the curve domain
	if (ON_NearZero(cdom.Min() - min_t, ON_ZERO_TOLERANCE)) {
	    interval_t.max = cdom.Max();
	} else if (ON_NearZero(cdom.Max() - min_t, ON_ZERO_TOLERANCE)) {
	    interval_t.min = cdom.Min();
	}
    } else {
	// if interval doesn't include midpt, assume the point nearest
	// the seam needs to be on the opposite side of the domain
	ON_Interval curr(min_t, max_t);
	if (!curr.Includes(interval_t.mid)) {
	    if (fabs(cdom.Min() - min_t) > fabs(cdom.Max() - max_t)) {
		interval_t.max = cdom.Min();
	    } else {
		interval_t.min = cdom.Max();
	    }
	}
    }
    interval_t.MakeIncreasing();

    return interval_t;
}

// given interval points in surface uv space, create new interval
// points that reflect the non-degenerate curve parameter interval
// used to generate the input points
HIDDEN IntervalPoints
uv_interval_from_points(
    IntervalPoints interval_pts,
    const ON_Surface *surf)
{
    ON_3dPoint &min_uv = interval_pts.min;
    ON_3dPoint &max_uv = interval_pts.max;

    ON_Interval udom = surf->Domain(0);
    ON_Interval vdom = surf->Domain(1);

    // TODO: check for singularity
    int seam_min = IsAtSeam(surf, min_uv, ON_ZERO_TOLERANCE);
    int seam_max = IsAtSeam(surf, max_uv, ON_ZERO_TOLERANCE);

    if ((seam_min && seam_max) && (min_uv == max_uv)) {
	// if uv endpoints are identical and on a seam
	// they need to be on opposite sides of the domain
	if (ON_NearZero(udom.Min() - min_uv[0], ON_ZERO_TOLERANCE) ||
	    ON_NearZero(udom.Max() - min_uv[0], ON_ZERO_TOLERANCE))
	{
	    // point on west/east edge becomes one point on west
	    // edge, one point on east edge
	    min_uv[0] = udom.Min();
	    max_uv[0] = udom.Max();
	} else if (ON_NearZero(vdom.Min() - min_uv[1], ON_ZERO_TOLERANCE) ||
		   ON_NearZero(vdom.Max() - min_uv[1], ON_ZERO_TOLERANCE))
	{
	    // point on south/north edge becomes one point on
	    // south edge, one point on north edge
	    min_uv[1] = vdom.Min();
	    max_uv[1] = vdom.Max();
	}
    } else if (!seam_min != !seam_max) { // XOR
	// if just one point is on a seam, make sure it's on the
	// correct side of the domain

	// get interval midpoint in uv space
	ON_ClassArray<ON_PX_EVENT> events;
	ON_3dPoint midpt = surf->PointAt(interval_pts.mid.x,
		interval_pts.mid.y);
	ON_Intersect(midpt, *surf, events, INTERSECTION_TOL);

	if (events.Count() == 1) {
	    // Check that the non-seam parameter of the
	    // midpoint is between the non-seam parameters of
	    // the interval uv on the seam and the other
	    // interval uv. If the midpoint non-seam parameter
	    // is outside the interval we'll move the seam_uv
	    // to the other side of the domain.
	    //
	    // For example, if the surface has a seam at the
	    // west/east edge and we have seam_uv (0.0, .1)
	    // and other_uv (.5, .6) we'll check that the
	    // interval midpoint uv has a u in [0.0, .5]. 
	    //
	    // A midpoint of (.25, .25) would be okay. A
	    // midpoint of (.75, .25) would necessitate us
	    // moving the seam_uv from the west edge to the
	    // east edge, e.g (1.0, .1).
	    int seam = seam_min ? seam_min : seam_max;
	    ON_3dPoint &seam_uv = seam_min ? min_uv : max_uv;
	    ON_3dPoint other_uv = seam_min ? max_uv : min_uv;

	    double *seam_t = &seam_uv[1];
	    double seam_opp_t = vdom.Max() - seam_uv[1];
	    double other_t = other_uv[1];
	    double midpt_t = events[0].m_b[1];
	    if (seam != SEAM_ALONG_U) {
		seam_t = &seam_uv[0];
		seam_opp_t = udom.Max() - seam_uv[0];
		other_t = other_uv[0];
		midpt_t = events[0].m_b[0];
	    }

	    ON_Interval curr(*seam_t, other_t);
	    if (!curr.Includes(midpt_t)) {
		// need to flip the seam point to the other
		// side of the domain
		*seam_t = seam_opp_t;
	    }
	}
    }
    return interval_pts;
}

HIDDEN IntervalPoints
interval_2d_to_uv(
    const ON_Interval &interval_2d,
    const ON_Curve *curve2d,
    const ON_Surface *surf)
{
    // initialize endpoints from evaluated surface uv points
    IntervalPoints pts;
    pts.min = curve2d->PointAt(interval_2d.Min());
    pts.mid = curve2d->PointAt(interval_2d.Mid());
    pts.max = curve2d->PointAt(interval_2d.Max());

    return uv_interval_from_points(pts, surf);
}

HIDDEN ON_Interval
interval_2d_to_3d(
    const ON_Interval &interval,
    const ON_Curve *curve2d,
    const ON_Curve *curve3d,
    const ON_Surface *surf)
{
    ON_Interval interval_3d;

    ON_Interval whole_domain = curve2d->Domain();
    whole_domain.MakeIncreasing();
    if (ON_NearZero(interval.Min() - whole_domain.Min(), ON_ZERO_TOLERANCE) &&
	ON_NearZero(interval.Max() - whole_domain.Max(), ON_ZERO_TOLERANCE))
    {
	interval_3d = curve3d->Domain();
	interval_3d.MakeIncreasing();
    } else {
	// get 2d curve interval endpoints as uv points
	IntervalPoints interval_uv =
	    interval_2d_to_uv(interval, curve2d, surf);

	// evaluate surface at uv points to get 3d interval points
	IntervalPoints pts_3d;
	pts_3d.min = surf->PointAt(interval_uv.min.x, interval_uv.min.y);
	pts_3d.mid = surf->PointAt(interval_uv.mid.x, interval_uv.mid.y);
	pts_3d.max = surf->PointAt(interval_uv.max.x, interval_uv.max.y);

	// convert 3d points into 3d curve parameters
	ON_ClassArray<ON_PX_EVENT> events;
	ON_Intersect(pts_3d.min, *curve3d, events, INTERSECTION_TOL);
	ON_Intersect(pts_3d.mid, *curve3d, events, INTERSECTION_TOL);
	ON_Intersect(pts_3d.max, *curve3d, events, INTERSECTION_TOL);

	if (events.Count() == 3) {
	    IntervalParams curve_t;
	    curve_t.min = events[0].m_b[0];
	    curve_t.mid = events[1].m_b[0];
	    curve_t.max = events[2].m_b[0];

	    curve_t = curve_interval_from_params(curve_t, curve3d);
	    interval_3d.Set(curve_t.min, curve_t.max);
	}
    }
    return interval_3d;
}


// Convert parameter intervals of a 2d face curve into equivalent
// parameter intervals on a matching 3d curve.
HIDDEN ON_SimpleArray<ON_Interval>
intervals_2d_to_3d(
    const ON_SimpleArray<ON_Interval> &intervals_2d,
    const ON_Curve *curve2d,
    const ON_Curve *curve3d,
    const ON_BrepFace *face)
{
    ON_SimpleArray<ON_Interval> out;
    const ON_Surface *surf = face->SurfaceOf();

    for (int i = 0; i < intervals_2d.Count(); ++i) {
	ON_Interval interval_3d = interval_2d_to_3d(intervals_2d[i], curve2d,
		curve3d, surf);

	if (interval_3d.IsValid()) {
	    out.Append(interval_3d);
	}
    }
    out.QuickSort(compare_interval);
    return out;
}

// Convert parameter interval of a 3d curve into the equivalent parameter
// interval on a matching 2d face curve.
HIDDEN ON_Interval
interval_3d_to_2d(
    const ON_Interval &interval,
    const ON_Curve *curve2d,
    const ON_Curve *curve3d,
    const ON_BrepFace *face)
{
    ON_Interval interval_2d;

    ON_Interval whole_domain = curve3d->Domain();
    whole_domain.MakeIncreasing();

    if (ON_NearZero(interval.Min() - whole_domain.Min(), ON_ZERO_TOLERANCE) &&
	ON_NearZero(interval.Max() - whole_domain.Max(), ON_ZERO_TOLERANCE))
    {
	interval_2d = curve2d->Domain();
	interval_2d.MakeIncreasing();
    } else {
	const ON_Surface *surf = face->SurfaceOf();

	IntervalPoints pts;
	pts.min = curve3d->PointAt(interval.Min());
	pts.mid = curve3d->PointAt(interval.Mid());
	pts.max = curve3d->PointAt(interval.Max());
	
	ON_ClassArray<ON_PX_EVENT> events;
	ON_Intersect(pts.min, *surf, events, INTERSECTION_TOL);
	ON_Intersect(pts.mid, *surf, events, INTERSECTION_TOL);
	ON_Intersect(pts.max, *surf, events, INTERSECTION_TOL);

	if (events.Count() == 3) {
	    IntervalPoints interval_uv;
	    interval_uv.min = events[0].m_b;
	    interval_uv.mid = events[1].m_b;
	    interval_uv.max = events[2].m_b;

	    interval_uv = uv_interval_from_points(interval_uv, surf);

	    // intersect surface uv parameters with 2d curve to convert
	    // surface uv parameters to 2d curve parameters
	    events.Empty();
	    ON_Intersect(interval_uv.min, *curve2d, events, INTERSECTION_TOL);
	    ON_Intersect(interval_uv.mid, *curve2d, events, INTERSECTION_TOL);
	    ON_Intersect(interval_uv.max, *curve2d, events, INTERSECTION_TOL);

	    if (events.Count() == 3) {
		IntervalParams curve_t;
		curve_t.min = events[0].m_b[0];
		curve_t.mid = events[1].m_b[0];
		curve_t.max = events[2].m_b[0];

		curve_t = curve_interval_from_params(curve_t, curve2d);
		interval_2d.Set(curve_t.min, curve_t.max);
	    }
	}
    }
    return interval_2d;
}

HIDDEN void
get_subcurves_inside_faces(
    ON_SimpleArray<ON_Curve *> &subcurves_on1,
    ON_SimpleArray<ON_Curve *> &subcurves_on2,
    const ON_Brep *brep1,
    const ON_Brep *brep2,
    int face_i1,
    int face_i2,
    ON_SSX_EVENT *event)
{
    // The ON_SSX_EVENT from SSI is the intersection of two whole surfaces.
    // We need to get the part that lies inside both trimmed patches.
    // (brep1's face[face_i1] and brep2's face[face_i2])
    ON_SimpleArray<ON_SSX_EVENT *> out;

    if (event == NULL) {
	return;
    }

    if (event->m_curve3d == NULL || event->m_curveA == NULL || event->m_curveB == NULL) {
	return;
    }

    // get the face loops
    if (face_i1 < 0 || face_i1 >= brep1->m_F.Count()) {
	bu_log("get_subcurves_inside_faces(): invalid face_i1 (%d).\n", face_i1);
	return;
    }
    if (face_i2 < 0 || face_i2 >= brep2->m_F.Count()) {
	bu_log("get_subcurves_inside_faces(): invalid face_i2 (%d).\n", face_i2);
	return;
    }

    const ON_SimpleArray<int> &face1_li = brep1->m_F[face_i1].m_li;
    ON_ClassArray<ON_SimpleArray<ON_Curve *> > face1_loops;
    for (int i = 0; i < face1_li.Count(); ++i) {
	const ON_BrepLoop &brep_loop = brep1->m_L[face1_li[i]];

	ON_SimpleArray<ON_Curve *> loop_curves;
	for (int j = 0; j < brep_loop.m_ti.Count(); ++j) {
	    ON_Curve *trim2d =
		brep1->m_C2[brep1->m_T[brep_loop.m_ti[j]].m_c2i];
	    loop_curves.Append(trim2d);
	}
	face1_loops.Append(loop_curves);
    }

    const ON_SimpleArray<int> &face2_li = brep2->m_F[face_i2].m_li;
    ON_ClassArray<ON_SimpleArray<ON_Curve *> > face2_loops;
    for (int i = 0; i < face2_li.Count(); ++i) {
	const ON_BrepLoop &brep_loop = brep2->m_L[face2_li[i]];
	ON_SimpleArray<ON_Curve *> loop_curves;

	for (int j = 0; j < brep_loop.m_ti.Count(); ++j) {
	    ON_Curve *trim2d =
		brep2->m_C2[brep2->m_T[brep_loop.m_ti[j]].m_c2i];
	    loop_curves.Append(trim2d);
	}
	face2_loops.Append(loop_curves);
    }

    // find the intervals of the curves that are inside/on each face
    ON_SimpleArray<ON_Interval> intervals1, intervals2;

    intervals1 = get_curve_intervals_inside_or_on_face(event->m_curveA,
	    face1_loops, INTERSECTION_TOL);

    intervals2 = get_curve_intervals_inside_or_on_face(event->m_curveB,
	    face2_loops, INTERSECTION_TOL);

    // get subcurves for each face
    for (int i = 0; i < intervals1.Count(); ++i) {
	// convert interval on face 1 to equivalent interval on face 2
	ON_Interval interval3d = interval_2d_to_3d(intervals1[i],
		event->m_curveA, event->m_curve3d, &brep1->m_F[face_i1]);
	if (!interval3d.IsValid()) {
	    continue;
	}

	ON_Interval interval_on2 = interval_3d_to_2d(interval3d,
		event->m_curveB, event->m_curve3d, &brep2->m_F[face_i2]);
	if (!interval_on2.IsValid()) {
	    continue;
	}
	
	// create subcurve from interval
	try {
	    ON_Curve *subcurve_on2 = sub_curve(event->m_curveB,
		    interval_on2.Min(), interval_on2.Max());

	    subcurves_on2.Append(subcurve_on2);
	} catch (InvalidInterval &e) {
	    bu_log("%s", e.what());
	}
    }
    for (int i = 0; i < intervals2.Count(); ++i) {
	// convert interval on face 2 to equivalent interval on face 1
	ON_Interval interval3d = interval_2d_to_3d(intervals2[i],
		event->m_curveB, event->m_curve3d, &brep2->m_F[face_i2]);
	if (!interval3d.IsValid()) {
	    continue;
	}

	ON_Interval interval_on1 = interval_3d_to_2d(interval3d,
		event->m_curveA, event->m_curve3d, &brep1->m_F[face_i1]);
	if (!interval_on1.IsValid()) {
	    continue;
	}
	
	// create subcurve from interval
	try {
	    ON_Curve *subcurve_on1 = sub_curve(event->m_curveA,
		    interval_on1.Min(), interval_on1.Max());

	    subcurves_on1.Append(subcurve_on1);
	} catch (InvalidInterval &e) {
	    bu_log("%s", e.what());
	}
    }
}

HIDDEN double
bbox_diagonal_length(ON_Curve *curve)
{
    double len = 0.0;
    if (curve) {
	ON_BoundingBox bbox;
	if (curve->GetTightBoundingBox(bbox)) {
	    len = bbox.Diagonal().Length();
	} else {
	    len = curve->PointAtStart().DistanceTo(curve->PointAtEnd());
	}
    }
    return len;
}

HIDDEN void
split_curve(ON_Curve *&left, ON_Curve *&right, const ON_Curve *in, double t)
{
    try {
	left = sub_curve(in, in->Domain().m_t[0], t);
    } catch (InvalidInterval &e) {
	left = NULL;
    }
    try {
	right = sub_curve(in, t, in->Domain().m_t[1]);
    } catch (InvalidInterval &e) {
	right = NULL;
    }
}

HIDDEN double
configure_for_linking(
    const ON_Surface *surf,
    LinkedCurve *&first,
    LinkedCurve *&second,
    LinkedCurve &in1,
    LinkedCurve &in2)
{
    double dist_s1s2 = shortest_parameter_distance(surf, in1.PointAtStart(),
	    in2.PointAtStart());
    double dist_s1e2 = shortest_parameter_distance(surf, in1.PointAtStart(),
	    in2.PointAtEnd());
    double dist_e1s2 = shortest_parameter_distance(surf, in1.PointAtEnd(),
	    in2.PointAtStart());
    double dist_e1e2 = shortest_parameter_distance(surf, in1.PointAtEnd(),
	    in2.PointAtEnd());

    double min_dist = std::min(dist_s1s2, dist_s1e2);
    min_dist = std::min(min_dist, dist_e1s2);
    min_dist = std::min(min_dist, dist_e1e2);

    first = second = NULL;
    if (dist_s1e2 <= min_dist) {
	first = &in2;
	second = &in1;
    } else if (dist_e1s2 <= min_dist) {
	first = &in1;
	second = &in2;
    } else if (dist_s1s2 <= min_dist) {
	if (in1.Reverse()) {
	    first = &in1;
	    second = &in2;
	}
    } else if (dist_e1e2 <= min_dist) {
	if (in2.Reverse()) {
	    first = &in1;
	    second = &in2;
	}
    }
    return min_dist;
}

struct LinkedCurveX {
    int ssi_idx_a;
    int ssi_idx_b;
    ON_SimpleArray<ON_X_EVENT> events;
};

HIDDEN std::vector<LinkedCurveX>
intersect_linked_curves(const LinkedCurve &a, const LinkedCurve &b)
{
    std::vector<LinkedCurveX> out;
    for (int i = 0; i < a.m_ssi_curves.Count(); ++i) {
	for (int j = 0; j < b.m_ssi_curves.Count(); ++j) {
	    ON_SimpleArray<ON_X_EVENT> events;
	    ON_Intersect(a.m_ssi_curves[i].m_curve, b.m_ssi_curves[j].m_curve,
		    events, INTERSECTION_TOL);

	    if (events.Count() > 0) {
		LinkedCurveX new_lcx = {i, j, events};
		out.push_back(new_lcx);
	    }
	}
    }
    return out;
}

HIDDEN ON_ClassArray<LinkedCurve>
link_curves(const ON_Surface *surf, const ON_SimpleArray<SSICurve> &in)
{
    // There might be two reasons why we need to link these curves.
    // 1) They are from intersections with two different surfaces.
    // 2) They are not continuous in the other surface's UV domain.

    ON_ClassArray<LinkedCurve> tmp;
    for (int i = 0; i < in.Count(); i++) {
	LinkedCurve linked;
	linked.m_surf = surf;
	linked.m_ssi_curves.Append(in[i]);
	tmp.Append(linked);
    }

    // As usual, we use a greedy approach.
    for (int i = 0; i < tmp.Count(); i++) {
	for (int j = 0; j < tmp.Count(); j++) {
	    if (tmp[i].m_ssi_curves.Count() == 0 || tmp[i].IsClosed()) {
		break;
	    }

	    if (tmp[j].m_ssi_curves.Count() == 0 || tmp[j].IsClosed() || j == i) {
		continue;
	    }

	    LinkedCurve *c1 = NULL, *c2 = NULL;
	    double dist = configure_for_linking(surf, c1, c2, tmp[i], tmp[j]);

	    if (dist > INTERSECTION_TOL) {
		std::vector<LinkedCurveX> lc_events =
		    intersect_linked_curves(tmp[i], tmp[j]);

		if (lc_events.size() == 0) {
		    continue;
		}

		LinkedCurveX lcx = lc_events[0];
		ON_X_EVENT event = lcx.events[0];

		// intersecting ssx curves indicates an error in the curves
		if (lc_events.size() > 1 || lcx.events.Count() > 1) {
		    bu_log("couldn't link converging intersection curves\n");
		    continue;
		} else {
		    // For a single intersection, try to link the
		    // curves anyway. Assume that one or both curve
		    // endpoints is just a little past where it should
		    // be. Split the curves at the intersection, and
		    // discard the portion with the smaller bbox
		    // diagonal.
		    ON_Curve *icurve, *jcurve;
		    icurve = tmp[i].m_ssi_curves[lcx.ssi_idx_a].m_curve;
		    jcurve = tmp[j].m_ssi_curves[lcx.ssi_idx_b].m_curve;

		    ON_Curve *ileft, *iright, *jleft, *jright;
		    ileft = iright = jleft = jright = NULL;
		    split_curve(ileft, iright, icurve, event.m_a[0]);
		    split_curve(jleft, jright, jcurve, event.m_b[0]);

		    if (bbox_diagonal_length(ileft) <
			bbox_diagonal_length(iright))
		    {
			std::swap(ileft, iright);
		    }
		    ON_Curve *isub = ileft;
		    delete iright;

		    if (bbox_diagonal_length(jleft) <
			bbox_diagonal_length(jright))
		    {
			std::swap(jleft, jright);
		    }
		    ON_Curve *jsub = jleft;
		    delete jright;

		    if (isub && jsub) {
			// replace the original ssi curves with the
			// trimmed versions
			SSICurve issi(isub), jssi(jsub);
			isub = jsub = NULL;

			delete tmp[i].m_ssi_curves[lcx.ssi_idx_a].m_curve;
			tmp[i].m_ssi_curves[lcx.ssi_idx_a] = issi;

			delete tmp[j].m_ssi_curves[lcx.ssi_idx_b].m_curve;
			tmp[j].m_ssi_curves[lcx.ssi_idx_b] = jssi;

			// proceed as if they're linkable
			configure_for_linking(surf, c1, c2, tmp[i], tmp[j]);
		    }
		    delete isub;
		    delete jsub;
		}
	    }

	    if (c1 != NULL && c2 != NULL) {
		LinkedCurve new_curve;
		new_curve.m_surf = surf;
		new_curve.Append(*c1);
		if (dist > ON_ZERO_TOLERANCE) {
		    new_curve.Append(SSICurve(new ON_LineCurve(c1->PointAtEnd(), c2->PointAtStart())));
		}
		new_curve.Append(*c2);
		tmp[i] = new_curve;
		tmp[j].m_ssi_curves.Empty();
	    }

	    // Check whether tmp[i] is closed within a tolerance
	    if (tmp[i].IsClosed(INTERSECTION_TOL) && !tmp[i].IsClosed(ON_ZERO_TOLERANCE)) {
		// make IsClosed() true
		ON_3dPoint start_pt = tmp[i].PointAtStart();
		ON_3dPoint end_pt = tmp[i].PointAtEnd();

		ON_2dPoint start_uv(start_pt.x, start_pt.y);
		ON_2dPoint end_uv(end_pt.x, end_pt.y);

		int seam_start = IsAtSeam(tmp[i].m_surf, start_uv, INTERSECTION_TOL / 2.0);
		int seam_end = IsAtSeam(tmp[i].m_surf, end_uv, INTERSECTION_TOL / 2.0);

		if (seam_start != SEAM_NONE && seam_start == seam_end) {
		    if (seam_start == SEAM_ALONG_V) {
			ON_Interval udom = tmp[i].m_surf->Domain(0);

			bool start_on_west = false;
			if (ON_NearZero(start_pt.x - udom.Min(), INTERSECTION_TOL / 2.0)) {
			    start_on_west = true;
			}
			bool end_on_west = false;
			if (ON_NearZero(end_pt.x - udom.Min(), INTERSECTION_TOL / 2.0)) {
			    end_on_west = true;
			}

			double avg_y = (start_pt.y + end_pt.y) / 2.0;
			ON_3dPoint east_edge(udom.Max(), avg_y, 0.0);
			ON_3dPoint west_edge(udom.Min(), avg_y, 0.0);

			if (start_on_west && !end_on_west) {
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, east_edge)));
			    tmp[i].Append(SSICurve(new ON_LineCurve(west_edge, start_pt)));
			} else if (!start_on_west && end_on_west) {
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, west_edge)));
			    tmp[i].Append(SSICurve(new ON_LineCurve(east_edge, start_pt)));
			}  else {
			    // same side
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, start_pt)));
			}
		    } else if (seam_start == SEAM_ALONG_U) {
			ON_Interval vdom = tmp[i].m_surf->Domain(1);

			bool start_on_south = false;
			if (ON_NearZero(start_pt.y - vdom.Min(), INTERSECTION_TOL / 2.0)) {
			    start_on_south = true;
			}
			bool end_on_south = false;
			if (ON_NearZero(end_pt.y - vdom.Min(), INTERSECTION_TOL / 2.0)) {
			    end_on_south = true;
			}

			double avg_x = (start_pt.x + end_pt.x) / 2.0;
			ON_3dPoint north_edge(avg_x, vdom.Max(), 0.0);
			ON_3dPoint south_edge(avg_x, vdom.Min(), 0.0);

			if (start_on_south && !end_on_south) {
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, north_edge)));
			    tmp[i].Append(SSICurve(new ON_LineCurve(south_edge, start_pt)));
			} else if (!start_on_south && end_on_south) {
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, south_edge)));
			    tmp[i].Append(SSICurve(new ON_LineCurve(south_edge, start_pt)));
			}  else {
			    // same side
			    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, start_pt)));
			}
		    }
		} else {
		    tmp[i].Append(SSICurve(new ON_LineCurve(end_pt, start_pt)));
		}
	    }
	}
    }

    // Append the remaining curves to out.
    ON_ClassArray<LinkedCurve> out;
    for (int i = 0; i < tmp.Count(); i++) {
	if (tmp[i].m_ssi_curves.Count() != 0) {
	    out.Append(tmp[i]);
	}
    }

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("link_curves(): %d curves remaining.\n", out.Count());
    }

    return out;
}


class CurvePoint {
public:
    int source_loop;
    int loop_index;
    double curve_t;
    ON_2dPoint pt;

    enum Location {
	BOUNDARY,
	INSIDE,
	OUTSIDE
    } location;

    static CurvePoint::Location
    PointLoopLocation(ON_2dPoint pt, const ON_SimpleArray<ON_Curve *> &loop);

    CurvePoint(
	int loop,
	int li,
	double pt_t,
	ON_Curve *curve,
	const ON_SimpleArray<ON_Curve *> &other_loop)
	: source_loop(loop), loop_index(li), curve_t(pt_t)
    {
	pt = curve->PointAt(curve_t);
	location = PointLoopLocation(pt, other_loop);
    }

    CurvePoint(
	int loop,
	int li,
	double t,
	ON_2dPoint p,
	CurvePoint::Location l)
	: source_loop(loop), loop_index(li), curve_t(t), pt(p), location(l)
    {
    }

    bool
    operator<(const CurvePoint &other) const
    {
	// for points not on the same loop, compare the actual points
	if (source_loop != other.source_loop) {
	    if (ON_NearZero(pt.DistanceTo(other.pt), INTERSECTION_TOL)) {
		return false;
	    }
	    return pt < other.pt;
	}

	// for points on the same loop, compare loop position
	if (loop_index == other.loop_index) {
	    if (ON_NearZero(curve_t - other.curve_t, INTERSECTION_TOL)) {
		return false;
	    }
	    return curve_t < other.curve_t;
	}
	return loop_index < other.loop_index;
    }

    bool
    operator==(const CurvePoint &other) const
    {
	return ON_NearZero(pt.DistanceTo(other.pt), INTERSECTION_TOL);
    }

    bool
    operator!=(const CurvePoint &other) const
    {
	return !ON_NearZero(pt.DistanceTo(other.pt), INTERSECTION_TOL);
    }
};

class CurvePointAbsoluteCompare {
public:
    bool
    operator()(const CurvePoint &a, const CurvePoint &b) const
    {
	if (ON_NearZero(a.pt.DistanceTo(b.pt), INTERSECTION_TOL)) {
	    return false;
	}
	return a.pt < b.pt;
    }
};

CurvePoint::Location
CurvePoint::PointLoopLocation(
    ON_2dPoint pt,
    const ON_SimpleArray<ON_Curve *> &loop)
{
    if (is_point_on_loop(pt, loop)) {
	return CurvePoint::BOUNDARY;
    }
    if (point_loop_location(pt, loop) == OUTSIDE_OR_ON_LOOP) {
	return CurvePoint::OUTSIDE;
    }
    return CurvePoint::INSIDE;
}

class CurveSegment {
public:
    ON_SimpleArray<ON_Curve *> &orig_loop;
    CurvePoint from, to;
    enum Location {
	BOUNDARY,
	INSIDE,
	OUTSIDE
    } location;

    CurveSegment(
	ON_SimpleArray<ON_Curve *> &loop,
	CurvePoint f,
	CurvePoint t,
	CurveSegment::Location l)
	: orig_loop(loop), from(f), to(t), location(l)
    {
    }

    void
    Reverse(void)
    {
	std::swap(from, to);
    }

    ON_Curve *
    Curve(void) const
    {
	ON_Curve *from_curve = orig_loop[from.loop_index];
	ON_Curve *to_curve = orig_loop[to.loop_index];
	ON_Interval from_dom = from_curve->Domain();
	ON_Interval to_dom = to_curve->Domain();

	// if endpoints are on the same curve, just get the part between them
	if (from.loop_index == to.loop_index) {
	    ON_Curve *seg_curve =
		sub_curve(from_curve, from.curve_t, to.curve_t);
	    return seg_curve;
	}
	// if endpoints are on different curves, we may need a subcurve of
	// just the 'from' curve, just the 'to' curve, or both
	if (ON_NearZero(from.curve_t - from_dom.m_t[1], INTERSECTION_TOL)) {
	    // starting at end of 'from' same as starting at start of 'to'
	    ON_Curve *seg_curve = sub_curve(to_curve, to_dom.m_t[0],
		    to.curve_t);
	    return seg_curve;
	}
	if (ON_NearZero(to.curve_t - to_dom.m_t[0], INTERSECTION_TOL)) {
	    // ending at start of 'to' same as ending at end of 'from'
	    ON_Curve *seg_curve = sub_curve(from_curve, from.curve_t,
		    from_dom.m_t[1]);
	    return seg_curve;
	}
	ON_PolyCurve *pcurve = new ON_PolyCurve();
	append_to_polycurve(sub_curve(from_curve, from.curve_t,
		    from_dom.m_t[1]), *pcurve);
	append_to_polycurve(sub_curve(to_curve, to_dom.m_t[0], to.curve_t),
		*pcurve);
	return pcurve;
    }

    bool
    IsDegenerate(void)
    {
	ON_Curve *seg_curve = NULL;
	try {
	    seg_curve = Curve();
	} catch (InvalidInterval &e) {
	    return true;
	}

	double length = 0.0;
	if (seg_curve->IsLinear(INTERSECTION_TOL)) {
	    length = seg_curve->PointAtStart().DistanceTo(
		    seg_curve->PointAtEnd());
	} else {
	    double min[3] = {0.0, 0.0, 0.0};
	    double max[3] = {0.0, 0.0, 0.0};

	    seg_curve->GetBBox(min, max, true);
	    length = DIST_PT_PT(min, max);
	}
	delete seg_curve;
	return length < INTERSECTION_TOL;
    }

    bool
    operator<(const CurveSegment &other) const
    {
	return from < other.from;
    }
};

#define LOOP_DIRECTION_CCW  1
#define LOOP_DIRECTION_CW  -1
#define LOOP_DIRECTION_NONE 0

HIDDEN bool
close_small_gap(ON_SimpleArray<ON_Curve *> &loop, int curr, int next)
{
    ON_3dPoint end_curr = loop[curr]->PointAtEnd();
    ON_3dPoint start_next = loop[next]->PointAtStart();

    double gap = end_curr.DistanceTo(start_next);
    if (gap <= INTERSECTION_TOL && gap >= ON_ZERO_TOLERANCE) {
	ON_Curve *closing_seg = new ON_LineCurve(end_curr, start_next);
	loop.Insert(next, closing_seg);
	return true;
    }
    return false;
}

HIDDEN void
close_small_gaps(ON_SimpleArray<ON_Curve *> &loop)
{
    if (loop.Count() == 0) {
	return;
    }
    for (int i = 0; i < loop.Count() - 1; ++i) {
	if (close_small_gap(loop, i, i + 1)) {
	    ++i;
	}
    }
    close_small_gap(loop, loop.Count() - 1, 0);
}

ON_Curve *
get_loop_curve(const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_PolyCurve *pcurve = new ON_PolyCurve();
    for (int i = 0; i < loop.Count(); ++i) {
	append_to_polycurve(loop[i]->Duplicate(), *pcurve);
    }
    return pcurve;
}

std::list<ON_SimpleArray<ON_Curve *> >::iterator
find_innerloop(std::list<ON_SimpleArray<ON_Curve *> > &loops)
{
    std::list<ON_SimpleArray<ON_Curve *> >::iterator k;
    for (k = loops.begin(); k != loops.end(); ++k) {
	ON_Curve *loop_curve = get_loop_curve(*k);

	if (ON_ClosedCurveOrientation(*loop_curve, NULL) == LOOP_DIRECTION_CW) {
	    delete loop_curve;
	    return k;
	}
	delete loop_curve;
    }
    return loops.end();
}

bool
set_loop_direction(ON_SimpleArray<ON_Curve *> &loop, int dir)
{
    ON_Curve *curve = get_loop_curve(loop);
    int curr_dir = ON_ClosedCurveOrientation(*curve, NULL);
    delete curve;

    if (curr_dir == LOOP_DIRECTION_NONE) {
	// can't set the correct direction
	return false;
    }
    if (curr_dir != dir) {
	// need reverse
	for (int i = 0; i < loop.Count(); ++i) {
	    if (!loop[i]->Reverse()) {
		return false;
	    }
	}
	loop.Reverse();
    }
    // curve already has the correct direction
    return true;
}

void
add_point_to_set(std::multiset<CurvePoint> &set, CurvePoint pt)
{
    if (set.count(pt) < 2) {
	set.insert(pt);
    }
}

std::multiset<CurveSegment>
make_segments(
    std::multiset<CurvePoint> &curve1_points,
    ON_SimpleArray<ON_Curve *> &loop1,
    ON_SimpleArray<ON_Curve *> &loop2)
{
    std::multiset<CurveSegment> out;

    std::multiset<CurvePoint>::iterator first = curve1_points.begin();
    std::multiset<CurvePoint>::iterator curr = first;
    std::multiset<CurvePoint>::iterator next = ++curve1_points.begin();

    for (; next != curve1_points.end(); ++curr, ++next) {
	CurvePoint from = *curr;
	CurvePoint to = *next;
	CurveSegment new_seg(loop1, from, to, CurveSegment::BOUNDARY);

	if (new_seg.IsDegenerate()) {
	    continue;
	}

	if (from.location == CurvePoint::BOUNDARY &&
	      to.location == CurvePoint::BOUNDARY)
	{
	    ON_Curve *seg_curve = loop1[from.loop_index];

	    double end_t = to.curve_t;
	    if (from.loop_index != to.loop_index) {
		end_t = seg_curve->Domain().m_t[1];
	    }
	    ON_2dPoint seg_midpt = seg_curve->PointAt(
		    ON_Interval(from.curve_t, end_t).Mid());

	    CurvePoint::Location midpt_location =
		CurvePoint::PointLoopLocation(seg_midpt, loop2);

	    if (midpt_location == CurvePoint::INSIDE) {
		new_seg.location = CurveSegment::INSIDE;
	    } else if (midpt_location == CurvePoint::OUTSIDE) {
		new_seg.location = CurveSegment::OUTSIDE;
	    }
	} else if (from.location == CurvePoint::INSIDE ||
		    to.location == CurvePoint::INSIDE)
	{
	    new_seg.location = CurveSegment::INSIDE;
	} else if (from.location == CurvePoint::OUTSIDE ||
		     to.location == CurvePoint::OUTSIDE)
	{
	    new_seg.location = CurveSegment::OUTSIDE;
	}
	out.insert(new_seg);
    }
    return out;
}

void
set_append_segment(
    std::multiset<CurveSegment> &out,
    const CurveSegment &seg)
{
    std::multiset<CurveSegment>::iterator i;
    for (i = out.begin(); i != out.end(); ++i) {
	if ((i->from == seg.to) && (i->to == seg.from)) {
	    // if this segment is a reversed version of an existing
	    // segment, it cancels the existing segment out
	    ON_Curve *prev_curve = i->Curve();
	    ON_Curve *seg_curve = seg.Curve();
	    ON_SimpleArray<ON_X_EVENT> events;
	    ON_Intersect(prev_curve, seg_curve, events, INTERSECTION_TOL);
	    if (events.Count() == 1 && events[0].m_type == ON_X_EVENT::ccx_overlap) {
		out.erase(i);
		return;
	    }
	}
    }
    out.insert(seg);
}

void
set_append_segments_at_location(
	std::multiset<CurveSegment> &out,
	std::multiset<CurveSegment> &in,
	CurveSegment::Location location,
	bool reversed_segs_cancel)
{
    std::multiset<CurveSegment>::iterator i;
    if (reversed_segs_cancel) {
	for (i = in.begin(); i != in.end(); ++i) {
	    if (i->location == location) {
		set_append_segment(out, *i);
	    }
	}
    } else {
	for (i = in.begin(); i != in.end(); ++i) {
	    if (i->location == location) {
		out.insert(*i);
	    }
	}
    }
}

std::multiset<CurveSegment>
find_similar_segments(
    std::multiset<CurveSegment> &set,
    const CurveSegment &seg)
{
    std::multiset<CurveSegment> out;

    std::multiset<CurveSegment>::iterator i;
    for (i = set.begin(); i != set.end(); ++i) {
	if (((i->from == seg.from) && (i->to == seg.to)) ||
	    ((i->from == seg.to)   && (i->to == seg.from)))
	{
	    out.insert(*i);
	}
    }
    return out;
}

std::multiset<CurveSegment>
get_op_segments(
	std::multiset<CurveSegment> &curve1_segments,
	std::multiset<CurveSegment> &curve2_segments,
	op_type op)
{
    std::multiset<CurveSegment> out;

    if (op == BOOLEAN_INTERSECT) {
	set_append_segments_at_location(out, curve1_segments,
		CurveSegment::INSIDE, true);

	set_append_segments_at_location(out, curve2_segments,
		CurveSegment::INSIDE, true);

	std::multiset<CurveSegment> c1_boundary_segs;
	set_append_segments_at_location(c1_boundary_segs, curve1_segments,
		CurveSegment::BOUNDARY, false);

	std::multiset<CurveSegment> c2_boundary_segs;
	set_append_segments_at_location(c2_boundary_segs, curve2_segments,
		CurveSegment::BOUNDARY, false);

	std::multiset<CurveSegment>::iterator i;
	for (i = c1_boundary_segs.begin(); i != c1_boundary_segs.end(); ++i) {
	    std::multiset<CurveSegment> curve1_matches =
		find_similar_segments(c1_boundary_segs, *i);

	    std::multiset<CurveSegment> curve2_matches =
		find_similar_segments(c2_boundary_segs, *i);

	    if (curve1_matches.size() > 1 || curve2_matches.size() > 1) {
		continue;
	    }
	    if (curve1_matches.begin()->from == curve2_matches.begin()->from) {
		out.insert(*i);
	    }
	}
    } else if (op == BOOLEAN_DIFF) {
	set_append_segments_at_location(out, curve1_segments,
		CurveSegment::OUTSIDE, true);

	set_append_segments_at_location(out, curve2_segments,
		CurveSegment::INSIDE, true);

	std::multiset<CurveSegment> c1_boundary_segs;
	set_append_segments_at_location(c1_boundary_segs, curve1_segments,
		CurveSegment::BOUNDARY, false);

	std::multiset<CurveSegment> c2_boundary_segs;
	set_append_segments_at_location(c2_boundary_segs, curve2_segments,
		CurveSegment::BOUNDARY, false);

	std::multiset<CurveSegment>::iterator i;
	for (i = c1_boundary_segs.begin(); i != c1_boundary_segs.end(); ++i) {
	    std::multiset<CurveSegment> curve1_matches =
		find_similar_segments(c1_boundary_segs, *i);

	    std::multiset<CurveSegment> curve2_matches =
		find_similar_segments(c2_boundary_segs, *i);

	    if (curve1_matches.size() > 1) {
		continue;
	    }
	    if (curve1_matches.begin()->from == curve2_matches.begin()->from ||
		curve2_matches.size() > 1)
	    {
		out.insert(*i);
	    }
	}
    }

    return out;
}

std::list<ON_SimpleArray<ON_Curve *> >
construct_loops_from_segments(
    std::multiset<CurveSegment> &segments)
{
    std::list<ON_SimpleArray<ON_Curve *> > out;

    while (!segments.empty()) {
	std::vector<std::multiset<CurveSegment>::iterator> loop_segs;
	std::multiset<CurvePoint, CurvePointAbsoluteCompare> visited_points;

	std::multiset<CurveSegment>::iterator curr_seg, prev_seg;
        curr_seg = segments.begin();

	loop_segs.push_back(curr_seg);
	visited_points.insert(curr_seg->from);
	visited_points.insert(curr_seg->to);

	bool closed_curve = (curr_seg->from == curr_seg->to);
	while (!closed_curve) {
	    // look for a segment that connects to the previous
	    prev_seg = curr_seg;
	    for (curr_seg = segments.begin(); curr_seg != segments.end(); ++curr_seg) {
		if (curr_seg->from == prev_seg->to) {
		    break;
		}
	    }

	    if (curr_seg == segments.end()) {
		// no segment connects to the prev one
		break;
	    } else {
		// Extend our loop with the joining segment.
		// If we've visited its endpoint before, then the loop
		// is now closed.
		loop_segs.push_back(curr_seg);
		visited_points.insert(curr_seg->to);
		closed_curve = (visited_points.count(curr_seg->to) > 1);
	    }
	}

	if (closed_curve) {
	    // find the segment the closing segment connected to (it
	    // may not be the first segment)
	    size_t i;
	    for (i = 0; i < loop_segs.size(); ++i) {
		if (loop_segs[i]->from == loop_segs.back()->to) {
		    break;
		}
	    }
	    // Form a curve from the closed chain of segments.
	    // Remove the used segments from the available set.
	    ON_SimpleArray<ON_Curve *> loop;
	    for (; i < loop_segs.size(); ++i) {
		try {
		    loop.Append(loop_segs[i]->Curve());
		} catch (InvalidInterval &e) {
		    bu_log("%s", e.what());
		}
		segments.erase(loop_segs[i]);
	    }
	    out.push_back(loop);
	} else {
	    // couldn't join to the last segment, discard it
	    segments.erase(loop_segs.back());
	    bu_log("construct_loops_from_segments: found unconnected segment\n");
	}
	loop_segs.clear();
	visited_points.clear();
    }
    return out;
}

std::multiset<CurvePoint>
get_loop_points(
    int source_loop,
    ON_SimpleArray<ON_Curve *> loop1,
    ON_SimpleArray<ON_Curve *> loop2)
{
    std::multiset<CurvePoint> out;

    ON_Curve *loop1_seg = loop1[0];
    out.insert(CurvePoint(source_loop, 0, loop1_seg->Domain().m_t[0], loop1_seg,
		loop2));

    for (int i = 0; i < loop1.Count(); ++i) {
	loop1_seg = loop1[i];
	out.insert(CurvePoint(source_loop, i, loop1_seg->Domain().m_t[1],
		    loop1_seg, loop2));
    }

    return out;
}

// Get the result of a boolean combination of two loops. Based on the
// algorithm from this paper:
//
// Margalit, Avraham and Gary D. Knott. 1989. "An Algorithm for
// Computing the Union, Intersection or Difference of two Polygons."
// Computers & Graphics 13:167-183. 
//
// gvu.gatech.edu/people/official/jarek/graphics/papers/04PolygonBooleansMargalit.pdf
std::list<ON_SimpleArray<ON_Curve *> >
loop_boolean(
    const ON_SimpleArray<ON_Curve *> &l1,
    const ON_SimpleArray<ON_Curve *> &l2,
    op_type op)
{
    std::list<ON_SimpleArray<ON_Curve *> > out;
    ON_SimpleArray<ON_Curve *> loop1, loop2;

    for (int i = 0; i < l1.Count(); ++i) {
	loop1.Append(l1[i]->Duplicate());
    }

    if (op != BOOLEAN_INTERSECT && op != BOOLEAN_DIFF) {
	bu_log("loop_boolean: unsupported operation\n");
	out.push_back(loop1);
	return out;
    }

    for (int i = 0; i < l2.Count(); ++i) {
	loop2.Append(l2[i]->Duplicate());
    }

    // set curve directions based on operation
    int loop1_dir, loop2_dir;

    loop1_dir = loop2_dir = LOOP_DIRECTION_CCW;
    if (op == BOOLEAN_DIFF) {
	loop2_dir = LOOP_DIRECTION_CW;
    }

    if (!set_loop_direction(loop1, loop1_dir) ||
	!set_loop_direction(loop2, loop2_dir))
    {
	bu_log("loop_boolean: couldn't standardize curve directions\n");
	out.push_back(loop1);

	for (int i = 0; i < l2.Count(); ++i) {
	    delete loop2[i];
	}
	return out;
    }

    std::multiset<CurvePoint> loop1_points, loop2_points;

    loop1_points = get_loop_points(1, loop1, loop2);
    loop2_points = get_loop_points(2, loop2, loop1);

    for (int i = 0; i < loop1.Count(); ++i) {
	for (int j = 0; j < loop2.Count(); ++j) {
	    ON_SimpleArray<ON_X_EVENT> x_events;
	    ON_Intersect(loop1[i], loop2[j], x_events, INTERSECTION_TOL);

	    for (int k = 0; k < x_events.Count(); ++k) {
		add_point_to_set(loop1_points, CurvePoint(1, i,
			    x_events[k].m_a[0], x_events[k].m_A[0],
			    CurvePoint::BOUNDARY));

		add_point_to_set(loop2_points, CurvePoint(2, j,
			    x_events[k].m_b[0], x_events[k].m_B[0],
			    CurvePoint::BOUNDARY));

		if (x_events[k].m_type == ON_X_EVENT::ccx_overlap) {
		    add_point_to_set(loop1_points, CurvePoint(1, i,
				x_events[k].m_a[1], x_events[k].m_A[1],
				CurvePoint::BOUNDARY));

		    add_point_to_set(loop2_points, CurvePoint(2, j,
				x_events[k].m_b[1], x_events[k].m_B[1],
				CurvePoint::BOUNDARY));
		}
	    }
	}
    }

    std::multiset<CurveSegment> loop1_segments, loop2_segments;
    loop1_segments = make_segments(loop1_points, loop1, loop2);
    loop2_segments = make_segments(loop2_points, loop2, loop1);

    std::multiset<CurveSegment> out_segments =
	get_op_segments(loop1_segments, loop2_segments, op);

    out = construct_loops_from_segments(out_segments);
    std::list<ON_SimpleArray<ON_Curve *> >::iterator li;
    for (li = out.begin(); li != out.end(); ++li) {
	close_small_gaps(*li);
    }

    for (int i = 0; i < l1.Count(); ++i) {
	delete loop1[i];
    }
    for (int i = 0; i < l2.Count(); ++i) {
	delete loop2[i];
    }
    return out;
}

std::list<ON_SimpleArray<ON_Curve *> >
innerloops_inside_outerloop(
    ON_SimpleArray<ON_Curve *> &outerloop_curve,
    const std::vector<ON_SimpleArray<ON_Curve *> > &innerloop_curves)
{
    std::list<ON_SimpleArray<ON_Curve *> > out;

    for (size_t i = 0; i < innerloop_curves.size(); ++i) {
	std::list<ON_SimpleArray<ON_Curve *> > new_innerloops;
	new_innerloops = loop_boolean(outerloop_curve, innerloop_curves[i],
		BOOLEAN_INTERSECT);

	std::list<ON_SimpleArray<ON_Curve *> >::iterator j;
	for (j = new_innerloops.begin(); j != new_innerloops.end(); ++j) {
	    set_loop_direction(*j, LOOP_DIRECTION_CW);
	    out.push_back(*j);
	}
    }
    return out;
}

TrimmedFace *
make_face_from_loops(
	const TrimmedFace *orig_face,
	ON_SimpleArray<ON_Curve *> &outerloop,
	const std::vector<ON_SimpleArray<ON_Curve *> > &innerloops)
{
    TrimmedFace *face = new TrimmedFace();

    face->m_face = orig_face->m_face;
    face->m_outerloop.Append(outerloop.Count(), outerloop.Array());

    std::list<ON_SimpleArray<ON_Curve *> > new_innerloops;
    new_innerloops = innerloops_inside_outerloop(outerloop, innerloops);

    std::list<ON_SimpleArray<ON_Curve *> >::iterator i;
    for (i = new_innerloops.begin(); i != new_innerloops.end(); ++i) {
	face->m_innerloop.push_back(*i);
    }
    return face;
}

void
append_faces_from_loops(
	ON_SimpleArray<TrimmedFace *> &out,
	const TrimmedFace *orig_face,
	std::list<ON_SimpleArray<ON_Curve *> > &new_loops)
{
    // copy face inner loops
    std::vector<ON_SimpleArray<ON_Curve *> > all_innerloops;
    for (size_t i = 0; i < orig_face->m_innerloop.size(); ++i) {
	ON_SimpleArray<ON_Curve *> loop;
	loop.Append(orig_face->m_innerloop[i].Count(),
		orig_face->m_innerloop[i].Array());
	all_innerloops.push_back(loop);
    }

    // add new innerloops, removing them from the input list
    std::list<ON_SimpleArray<ON_Curve *> >::iterator innerloop;

    size_t first_new = all_innerloops.size();

    innerloop = find_innerloop(new_loops);
    while (innerloop != new_loops.end()) {
	all_innerloops.push_back(*innerloop);
	new_loops.erase(innerloop);

	innerloop = find_innerloop(new_loops);
    }

    // make a face from each outerloop, using appropriate innerloops
    std::list<ON_SimpleArray<ON_Curve *> >::iterator i;
    for (i = new_loops.begin(); i != new_loops.end(); ++i) {
	out.Append(make_face_from_loops(orig_face, *i, all_innerloops));
    }

    // delete original innerloops from the new set
    for (size_t loop = first_new; loop < all_innerloops.size(); ++loop) {
	for (int j = 0; j < all_innerloops[loop].Count(); ++j) {
	    delete all_innerloops[loop][j];
	    all_innerloops[loop][j] = NULL;
	}
    }
}

/* Turn an open curve into a closed curve by using segments from the
 * face outerloop to connect its endpoints.
 *
 * Returns false on failure, true otherwise.
 */
std::vector<ON_SimpleArray<ON_Curve *> >
split_face_into_loops(
    const TrimmedFace *orig_face,
    LinkedCurve &linked_curve)
{
    std::vector<ON_SimpleArray<ON_Curve *> > out;

    if (linked_curve.IsClosed(ON_ZERO_TOLERANCE, false)) {
	ON_SimpleArray<ON_Curve *> loop;
	for (int i = 0; i < orig_face->m_outerloop.Count(); ++i) {
	    loop.Append(orig_face->m_outerloop[i]->Duplicate());
	}
	out.push_back(loop);
	return out;
    }

    /* We followed the algorithms described in:
     * S. Krishnan, A. Narkhede, and D. Manocha. BOOLE: A System to Compute
     * Boolean Combinations of Sculptured Solids. Technical Report TR95-008,
     * Department of Computer Science, University of North Carolina, 1995.
     * Appendix B: Partitioning a Simple Polygon using Non-Intersecting
     * Chains.
     */

    // Get the intersection points between the SSI curves and the outerloop.
    ON_SimpleArray<IntersectPoint> clx_points;
    bool intersects_outerloop = false;
    for (int i = 0; i < orig_face->m_outerloop.Count(); i++) {
	ON_SimpleArray<ON_X_EVENT> x_events;
	ON_Intersect(orig_face->m_outerloop[i], linked_curve.Curve(),
		x_events, INTERSECTION_TOL);

	for (int j = 0; j < x_events.Count(); j++) {
	    IntersectPoint tmp_pt;
	    tmp_pt.m_pt = x_events[j].m_A[0];
	    tmp_pt.m_seg_t = x_events[j].m_a[0];
	    tmp_pt.m_curve_t = x_events[j].m_b[0];
	    tmp_pt.m_loop_seg = i;
	    clx_points.Append(tmp_pt);

	    if (x_events[j].m_type == ON_X_EVENT::ccx_overlap) {
		tmp_pt.m_pt = x_events[j].m_A[1];
		tmp_pt.m_seg_t = x_events[j].m_a[1];
		tmp_pt.m_curve_t = x_events[j].m_b[1];
		clx_points.Append(tmp_pt);
	    }
	    if (x_events.Count()) {
		intersects_outerloop = true;
	    }
	}
    }

    // can't close curves that don't partition the face
    if (!intersects_outerloop || clx_points.Count() < 2) {
	ON_SimpleArray<ON_Curve *> loop;
	for (int i = 0; i < orig_face->m_outerloop.Count(); ++i) {
	    loop.Append(orig_face->m_outerloop[i]->Duplicate());
	}
	out.push_back(loop);
	return out;
    }

    // rank these intersection points
    clx_points.QuickSort(curve_t_compare);
    for (int i = 0; i < clx_points.Count(); i++) {
	clx_points[i].m_curve_pos = i;
    }

    // classify intersection points
    ON_SimpleArray<IntersectPoint> new_pts;
    double curve_min_t = linked_curve.Domain().Min();
    double curve_max_t = linked_curve.Domain().Max();

    for (int i = 0; i < clx_points.Count(); i++) {
	bool is_first_ipt = (i == 0);
	bool is_last_ipt = (i == (clx_points.Count() - 1));

	IntersectPoint *ipt = &clx_points[i];
	double curve_t = ipt->m_curve_t;

	ON_3dPoint prev = linked_curve.PointAtStart();
	if (!is_first_ipt) {
	    double prev_curve_t = clx_points[i - 1].m_curve_t;
	    prev = linked_curve.PointAt((curve_t + prev_curve_t) * .5);
	}
	ON_3dPoint next = linked_curve.PointAtEnd();
	if (!is_last_ipt) {
	    double next_curve_t = clx_points[i + 1].m_curve_t;
	    next = linked_curve.PointAt((curve_t + next_curve_t) * .5);
	}
	// If the point is on the boundary, we treat it with the same
	// way as it's outside.
	// For example, the prev side is inside, and the next's on
	// boundary, that point should be IntersectPoint::OUT, the
	// same as the next's outside the loop.
	// Other cases are similar.
	bool prev_in, next_in;
	try {
	    prev_in = is_point_inside_loop(prev, orig_face->m_outerloop);
	    next_in = is_point_inside_loop(next, orig_face->m_outerloop);
	} catch (InvalidGeometry &e) {
	    bu_log("%s", e.what());
	    // not a loop
	    ipt->m_dir = IntersectPoint::UNSET;
	    continue;
	}
	if (is_first_ipt && ON_NearZero(curve_t - curve_min_t)) {
	    ipt->m_dir = next_in ? IntersectPoint::IN : IntersectPoint::OUT;
	    continue;
	}
	if (is_last_ipt && ON_NearZero(curve_t - curve_max_t)) {
	    ipt->m_dir = prev_in ? IntersectPoint::OUT : IntersectPoint::IN;
	    continue;
	}
	if (prev_in && next_in) {
	    // tangent point, both sides in, duplicate that point
	    new_pts.Append(*ipt);
	    new_pts.Last()->m_dir = IntersectPoint::TANGENT;
	    new_pts.Last()->m_curve_pos = ipt->m_curve_pos;
	    ipt->m_dir = IntersectPoint::TANGENT;
	} else if (!prev_in && !next_in) {
	    // tangent point, both sides out, useless
	    ipt->m_dir = IntersectPoint::UNSET;
	} else if (prev_in && !next_in) {
	    // transversal point, going outside
	    ipt->m_dir = IntersectPoint::OUT;
	} else {
	    // transversal point, going inside
	    ipt->m_dir = IntersectPoint::IN;
	}
    }

    clx_points.Append(new_pts.Count(), new_pts.Array());
    clx_points.QuickSort(loop_t_compare);

    // Split the outer loop.
    ON_SimpleArray<ON_Curve *> outerloop_segs;
    int clx_i = 0;
    for (int loop_seg = 0; loop_seg < orig_face->m_outerloop.Count(); loop_seg++) {
	ON_Curve *remainder = orig_face->m_outerloop[loop_seg]->Duplicate();
	if (remainder == NULL) {
	    bu_log("ON_Curve::Duplicate() failed.\n");
	    continue;
	}
	for (; clx_i < clx_points.Count() && clx_points[clx_i].m_loop_seg == loop_seg; clx_i++) {
	    IntersectPoint &ipt = clx_points[clx_i];
	    ON_Curve *portion_before_ipt = NULL;
	    if (remainder) {
		double start_t = remainder->Domain().Min();
		double end_t = remainder->Domain().Max();
		if (ON_NearZero(ipt.m_seg_t - end_t)) {
		    // Can't call Split() if ipt is at start (that
		    // case is handled by the initialization) or ipt
		    // is at end (handled here).
		    portion_before_ipt = remainder;
		    remainder = NULL;
		} else if (!ON_NearZero(ipt.m_seg_t - start_t)) {
		    if (!remainder->Split(ipt.m_seg_t, portion_before_ipt, remainder)) {
			bu_log("Split failed.\n");
			bu_log("Domain: [%f, %f]\n", end_t, start_t);
			bu_log("m_seg_t: %f\n", ipt.m_seg_t);
		    }
		}
	    }
	    if (portion_before_ipt) {
		outerloop_segs.Append(portion_before_ipt);
	    }
	    ipt.m_split_li = outerloop_segs.Count() - 1;
	}
	if (remainder) {
	    outerloop_segs.Append(remainder);
	}
    }

    // Append the first element at the last to handle some special cases.
    if (clx_points.Count()) {
	clx_points.Append(clx_points[0]);
	clx_points.Last()->m_loop_seg += orig_face->m_outerloop.Count();
	for (int i = 0; i <= clx_points[0].m_split_li; i++) {
	    ON_Curve *dup = outerloop_segs[i]->Duplicate();
	    if (dup == NULL) {
		bu_log("ON_Curve::Duplicate() failed.\n");
	    }
	    outerloop_segs.Append(dup);
	}
	clx_points.Last()->m_split_li = outerloop_segs.Count() - 1;
    }

    if (DEBUG_BREP_BOOLEAN) {
	for (int i = 0; i < clx_points.Count(); i++) {
	    IntersectPoint &ipt = clx_points[i];
	    bu_log("clx_points[%d](count = %d): ", i, clx_points.Count());
	    bu_log("m_curve_pos = %d, m_dir = %d\n", ipt.m_curve_pos, ipt.m_dir);
	}
    }

    std::stack<int> s;
    for (int i = 0; i < clx_points.Count(); i++) {
	if (clx_points[i].m_dir == IntersectPoint::UNSET) {
	    continue;
	}
	if (s.empty()) {
	    s.push(i);
	    continue;
	}
	const IntersectPoint &p = clx_points[s.top()];
	const IntersectPoint &q = clx_points[i];

	if (loop_t_compare(&p, &q) > 0 || q.m_split_li < p.m_split_li) {
	    bu_log("stack error or sort failure.\n");
	    bu_log("s.top() = %d, i = %d\n", s.top(), i);
	    bu_log("p->m_split_li = %d, q->m_split_li = %d\n", p.m_split_li, q.m_split_li);
	    bu_log("p->m_loop_seg = %d, q->m_loop_seg = %d\n", p.m_loop_seg, q.m_loop_seg);
	    bu_log("p->m_seg_t = %g, q->m_seg_t = %g\n", p.m_seg_t, q.m_seg_t);
	    continue;
	}
	if (q.m_curve_pos - p.m_curve_pos == 1 &&
	    q.m_dir != IntersectPoint::IN &&
	    p.m_dir != IntersectPoint::OUT)
	{
	    s.pop();
	} else if (p.m_curve_pos - q.m_curve_pos == 1 &&
		   p.m_dir != IntersectPoint::IN &&
		   q.m_dir != IntersectPoint::OUT)
	{
	    s.pop();
	} else {
	    s.push(i);
	    continue;
	}

	// need to form a new loop
	ON_SimpleArray<ON_Curve *> newloop;
	int curve_count = q.m_split_li - p.m_split_li;
	for (int j = p.m_split_li + 1; j <= q.m_split_li; j++) {
	    // No need to duplicate the curve, because the pointer
	    // in the array 'outerloop_segs' will be moved out later.
	    newloop.Append(outerloop_segs[j]);
	}

	// The curves on the outer loop is from p to q, so the curves on the
	// SSI curve should be from q to p (to form a loop)
	double t1 = p.m_curve_t, t2 = q.m_curve_t;
	bool need_reverse = true;
	if (t1 > t2) {
	    std::swap(t1, t2);
	    need_reverse = false;
	}
	ON_Curve *seg_on_SSI = linked_curve.SubCurve(t1, t2);
	if (seg_on_SSI == NULL) {
	    bu_log("sub_curve() failed.\n");
	    continue;
	}
	if (need_reverse) {
	    if (!seg_on_SSI->Reverse()) {
		bu_log("Reverse failed.\n");
		continue;
	    }
	}
	newloop.Append(seg_on_SSI);
	close_small_gaps(newloop);

	ON_Curve *rev_seg_on_SSI = seg_on_SSI->Duplicate();
	if (!rev_seg_on_SSI || !rev_seg_on_SSI->Reverse()) {
	    bu_log("Reverse failed.\n");
	    continue;
	} else {
	    // Update the outerloop
	    outerloop_segs[p.m_split_li + 1] = rev_seg_on_SSI;
	    int k = p.m_split_li + 2;
	    for (int j = q.m_split_li + 1; j < outerloop_segs.Count(); j++) {
		outerloop_segs[k++] = outerloop_segs[j];
	    }
	    while (k < outerloop_segs.Count()) {
		outerloop_segs[outerloop_segs.Count() - 1] = NULL;
		outerloop_segs.Remove();
	    }
	    // Update m_split_li
	    for (int j = i + 1; j < clx_points.Count(); j++) {
		clx_points[j].m_split_li -= curve_count - 1;
	    }
	}

	// append the new loop if it's valid
	if (is_loop_valid(newloop, ON_ZERO_TOLERANCE)) {
	    ON_SimpleArray<ON_Curve *> loop;
	    loop.Append(newloop.Count(), newloop.Array());
	    out.push_back(loop);
	} else {
	    for (int j = 0; j < newloop.Count(); j++) {
		delete newloop[j];
	    }
	}
    }

    // Remove the duplicated segments before the first intersection point.
    if (clx_points.Count()) {
	for (int i = 0; i <= clx_points[0].m_split_li; i++) {
	    delete outerloop_segs[0];
	    outerloop_segs[0] = NULL;
	    outerloop_segs.Remove(0);
	}
    }

    if (out.size() > 0) {
	// The remaining part after splitting some parts out.
	close_small_gaps(outerloop_segs);
	if (is_loop_valid(outerloop_segs, ON_ZERO_TOLERANCE)) {
	    ON_SimpleArray<ON_Curve *> loop;
	    loop.Append(outerloop_segs.Count(), outerloop_segs.Array());
	    out.push_back(loop);
	} else {
	    for (int i = 0; i < outerloop_segs.Count(); i++)
		if (outerloop_segs[i]) {
		    delete outerloop_segs[i];
		}
	}
    }
    return out;
}

void
free_loops(std::vector<ON_SimpleArray<ON_Curve *> > &loops)
{
    for (size_t i = 0; i < loops.size(); ++i) {
	for (int j = 0; j < loops[i].Count(); ++j) {
	    delete loops[i][j];
	    loops[i][j] = NULL;
	}
    }
}

bool
loop_is_degenerate(const ON_SimpleArray<ON_Curve *> &loop)
{
    if (loop.Count() < 1) {
	return true;
    }
    ON_Curve *loop_curve = get_loop_curve(loop);

    ON_2dPoint joint = loop_curve->PointAtStart();
    ON_2dPoint mid = loop_curve->PointAt(loop_curve->Domain().Mid());

    ON_Curve *left = NULL, *right = NULL;
    loop_curve->Split(loop_curve->Domain().Mid(), left, right);
    delete loop_curve;

    ON_SimpleArray<ON_X_EVENT> events;
    ON_Intersect(left, right, events, INTERSECTION_TOL);
    delete left;
    delete right;

    // two halves of loop should intersect at the start/end point and
    // the midpoint, and nowhere else
    if (events.Count() != 2) {
	return true;
    }
    for (int i = 0; i < events.Count(); ++i) {
	if (events[i].m_type != ON_X_EVENT::ccx_point) {
	    return true;
	}
	
	ON_2dPoint ipt = events[i].m_A[0];
	if (!ON_NearZero(ipt.DistanceTo(joint), INTERSECTION_TOL) &&
	    !ON_NearZero(ipt.DistanceTo(mid), INTERSECTION_TOL)) {
	    return true;
	}
    }
    return false;
}

// It might be worth investigating the following approach to building a set of faces from the splitting
// in order to achieve robustness in the final result:
//
// A) trim the raw SSI curves with the trimming loops from both faces and get "final" curve segments in
//    3D and both 2D parametric spaces.  Consolidate curves where different faces created the same curve.
// B) assemble the new 2D segments and whatever pieces are needed from the existing trimming curves to
//    form new 2D loops (which must be non-self-intersecting), whose roles in A and B respectively
//    would be determined by the boolean op and each face's role within it.
// C) build "representative polygons" for all the 2D loops in each face, new and old - representative in
//    this case meaning that the intersection behavior of the general loops is accurately duplicated
//    by the polygons, which should be assurable by identifying and using all 2D curve intersections and possibly
//    horizontal and vertical tangents - and use clipper to perform the boolean ops.  Using the resulting polygons,
//    deduce and assemble the final trimming loops (and face or faces) created from A and B respectively.

// Note that the 2D information alone cannot be enough to decide *which* faces created from these splits
// end up in the final brep.  A case to think about here is the case of two spheres intersecting -
// depending on A, the exact trimming
// loop in B may need to either define the small area as a new face, or everything BUT the small area
// as a new face - different A spheres may either almost fully contain B or just intersect it.  That case
// would seem to suggest that we do need some sort of inside/outside test, since B doesn't have enough
// information to determine which face is to be saved without consulting A.  Likewise, A may either save
// just the piece inside the loop or everything outside it, depending on B.  This is the same situation we
// were in with the original face sets.
//
// A possible improvement here might be to calculate the best fit plane of the intersection curve and rotate
// both the faces in question and A so that that plane is centered at the origin with the normal in z+.
// In that orientation, axis aligned bounding box tests can be made that will be as informative
// as possible, and may allow many inside/outside decisions to be made without an explicit raytrace.  Coplanar
// faces will have to be handled differently, but for convex cases there should be enough information to decide.
// Concave cases may require a raytrace, but there is one other possible approach - if, instead of using the
// whole brep and face bounding boxes we start with the bounding box of the intersection curve and construct
// the sub-box that 'slices' through the parent bbox to the furthest wall in the opposite direction from the
// surface normal, then see which of the two possible
// faces' bounding boxes removes the most volume from that box when subtracted, we may be able to decide
// (say, for a subtraction) which face is cutting deeper.  It's not clear to me yet if such an approach would
// work or would scale to complex cases, but it may be worth thinking about.
HIDDEN ON_SimpleArray<TrimmedFace *>
split_trimmed_face(
    const TrimmedFace *orig_face,
    ON_ClassArray<LinkedCurve> &ssx_curves)
{
    ON_SimpleArray<TrimmedFace *> out;
    out.Append(orig_face->Duplicate());

    if (ssx_curves.Count() == 0) {
	// no curves, no splitting
	return out;
    }

    ON_Curve *face_outerloop = get_loop_curve(orig_face->m_outerloop);
    if (!face_outerloop->IsClosed()) {
	// need closed outerloop
	delete face_outerloop;
	return out;
    }
    delete face_outerloop;

    for (int i = 0; i < ssx_curves.Count(); ++i) {
	std::vector<ON_SimpleArray<ON_Curve *> > ssx_loops;

	// get closed version of current ssx curve
	if (ssx_curves[i].IsClosed(ON_ZERO_TOLERANCE, false)) {
	    ON_SimpleArray<ON_Curve *> loop;
	    loop.Append(ssx_curves[i].Curve()->Duplicate());
	    ssx_loops.push_back(loop);
	} else {
	    ssx_loops = split_face_into_loops(orig_face, ssx_curves[i]);
	}

	ON_SimpleArray<TrimmedFace *> next_out;
	for (size_t j = 0; j < ssx_loops.size(); ++j) {
	    if (loop_is_degenerate(ssx_loops[j])) {
		continue;
	    }

	    for (int k = 0; k < out.Count(); ++k) {
		std::list<ON_SimpleArray<ON_Curve *> > intersect_loops, diff_loops;

		// get the portion of the current outerloop inside the
		// closed ssx curve
		intersect_loops = loop_boolean(out[k]->m_outerloop,
			ssx_loops[j], BOOLEAN_INTERSECT);

		if (ssx_curves[i].IsClosed(ON_ZERO_TOLERANCE, false)) {
		    if (intersect_loops.size() == 0) {
			// no intersection, just keep the face as-is
			next_out.Append(out[k]->Duplicate());
			continue;
		    }

		    // for a naturally closed intersection curve, we
		    // also need the portion outside the curve
		    diff_loops = loop_boolean(out[k]->m_outerloop, ssx_loops[j],
			    BOOLEAN_DIFF);
		}

		// make new faces from the loops
		append_faces_from_loops(next_out, out[k], intersect_loops);
		append_faces_from_loops(next_out, out[k], diff_loops);
	    }
	}
	free_loops(ssx_loops);

	if (next_out.Count() > 0) {
	    // replace previous faces with the new ones
	    for (int j = 0; j < out.Count(); ++j) {
		delete out[j];
	    }
	    out.Empty();

	    out.Append(next_out.Count(), next_out.Array());
	}
    }

    for (int i = 0; i < out.Count(); ++i) {
	close_small_gaps(out[i]->m_outerloop);
	for (size_t j = 0; j < out[i]->m_innerloop.size(); ++j) {
	    close_small_gaps(out[i]->m_innerloop[j]);
	}
    }

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("Split to %d faces.\n", out.Count());
	for (int i = 0; i < out.Count(); i++) {
	    bu_log("Trimmed Face %d:\n", i);
	    bu_log("outerloop:\n");
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    textlog.PushIndent();
	    for (int j = 0; j < out[i]->m_outerloop.Count(); j++) {
		textlog.Print("Curve %d\n", j);
		out[i]->m_outerloop[j]->Dump(textlog);
	    }
	    bu_log(ON_String(wstr).Array());

	    for (unsigned int j = 0; j < out[i]->m_innerloop.size(); j++) {
		bu_log("innerloop %d:\n", j);
		ON_wString wstr2;
		ON_TextLog textlog2(wstr2);
		textlog2.PushIndent();
		for (int k = 0; k < out[i]->m_innerloop[j].Count(); k++) {
		    textlog2.Print("Curve %d\n", k);
		    out[i]->m_innerloop[j][k]->Dump(textlog2);
		}
		bu_log(ON_String(wstr2).Array());
	    }
	}
    }
    return out;
}


HIDDEN bool
is_same_surface(const ON_Surface *surf1, const ON_Surface *surf2)
{
    // Approach: Get their NURBS forms, and compare their CVs.
    // If their CVs are all the same (location and weight), they are
    // regarded as the same surface.

    if (surf1 == NULL || surf2 == NULL) {
	return false;
    }
    /*
        // Deal with two planes, if that's what we have - in that case
        // the determination can be more general than the CV comparison
        ON_Plane surf1_plane, surf2_plane;
        if (surf1->IsPlanar(&surf1_plane) && surf2->IsPlanar(&surf2_plane)) {
    	ON_3dVector surf1_normal = surf1_plane.Normal();
    	ON_3dVector surf2_normal = surf2_plane.Normal();
    	if (surf1_normal.IsParallelTo(surf2_normal) == 1) {
    	    if (surf1_plane.DistanceTo(surf2_plane.Origin()) < ON_ZERO_TOLERANCE) {
    		return true;
    	    } else {
    		return false;
    	    }
    	} else {
    	    return false;
    	}
        }
    */

    ON_NurbsSurface nurbs_surf1, nurbs_surf2;
    if (!surf1->GetNurbForm(nurbs_surf1) || !surf2->GetNurbForm(nurbs_surf2)) {
	return false;
    }

    if (nurbs_surf1.Degree(0) != nurbs_surf2.Degree(0)
	|| nurbs_surf1.Degree(1) != nurbs_surf2.Degree(1)) {
	return false;
    }

    if (nurbs_surf1.CVCount(0) != nurbs_surf2.CVCount(0)
	|| nurbs_surf1.CVCount(1) != nurbs_surf2.CVCount(1)) {
	return false;
    }

    for (int i = 0; i < nurbs_surf1.CVCount(0); i++) {
	for (int j = 0; j < nurbs_surf2.CVCount(1); j++) {
	    ON_4dPoint cvA, cvB;
	    nurbs_surf1.GetCV(i, j, cvA);
	    nurbs_surf2.GetCV(i, j, cvB);
	    if (cvA != cvB) {
		return false;
	    }
	}
    }

    if (nurbs_surf1.KnotCount(0) != nurbs_surf2.KnotCount(0)
	|| nurbs_surf1.KnotCount(1) != nurbs_surf2.KnotCount(1)) {
	return false;
    }

    for (int i = 0; i < nurbs_surf1.KnotCount(0); i++) {
	if (!ON_NearZero(nurbs_surf1.m_knot[0][i] - nurbs_surf2.m_knot[0][i])) {
	    return false;
	}
    }

    for (int i = 0; i < nurbs_surf1.KnotCount(1); i++) {
	if (!ON_NearZero(nurbs_surf1.m_knot[1][i] - nurbs_surf2.m_knot[1][i])) {
	    return false;
	}
    }

    return true;
}


HIDDEN void
add_elements(ON_Brep *brep, ON_BrepFace &face, const ON_SimpleArray<ON_Curve *> &loop, ON_BrepLoop::TYPE loop_type)
{
    if (!loop.Count()) {
	return;
    }

    ON_BrepLoop &breploop = brep->NewLoop(loop_type, face);
    const ON_Surface *srf = face.SurfaceOf();

    // Determine whether a segment should be a seam trim, according to the
    // requirements in ON_Brep::IsValid() (See opennurbs_brep.cpp)
    for (int k = 0; k < loop.Count(); k++) {
	ON_BOOL32 bClosed[2];
	bClosed[0] = srf->IsClosed(0);
	bClosed[1] = srf->IsClosed(1);
	if (bClosed[0] || bClosed[1]) {
	    ON_Surface::ISO iso1, iso2, iso_type;
	    int endpt_index = -1;
	    iso1 = srf->IsIsoparametric(*loop[k]);
	    if (ON_Surface::E_iso == iso1 && bClosed[0]) {
		iso_type = ON_Surface::W_iso;
		endpt_index = 1;
	    } else if (ON_Surface::W_iso == iso1 && bClosed[0]) {
		iso_type = ON_Surface::E_iso;
		endpt_index = 1;
	    } else if (ON_Surface::S_iso == iso1 && bClosed[1]) {
		iso_type = ON_Surface::N_iso;
		endpt_index = 0;
	    } else if (ON_Surface::N_iso == iso1 && bClosed[1]) {
		iso_type = ON_Surface::S_iso;
		endpt_index = 0;
	    }
	    if (endpt_index != -1) {
		ON_Interval side_interval;
		const double side_tol = 1.0e-4;
		double s0, s1;
		bool seamed = false;
		side_interval.Set(loop[k]->PointAtStart()[endpt_index], loop[k]->PointAtEnd()[endpt_index]);
		if (((ON_Surface::N_iso == iso_type || ON_Surface::W_iso == iso_type) && side_interval.IsIncreasing())
		    || ((ON_Surface::S_iso == iso_type || ON_Surface::E_iso == iso_type) && side_interval.IsDecreasing())) {
		    for (int i = 0; i < breploop.m_ti.Count(); i++) {
			ON_BrepTrim &trim = brep->m_T[breploop.m_ti[i]];
			if (ON_BrepTrim::boundary != trim.m_type) {
			    continue;
			}
			iso2 = srf->IsIsoparametric(trim);
			if (iso2 != iso_type) {
			    continue;
			}
			s1 = side_interval.NormalizedParameterAt(trim.PointAtStart()[endpt_index]);
			if (fabs(s1 - 1.0) > side_tol) {
			    continue;
			}
			s0 = side_interval.NormalizedParameterAt(trim.PointAtEnd()[endpt_index]);
			if (fabs(s0) > side_tol) {
			    continue;
			}

			// Check 3D distances - not included in ON_Brep::IsValid().
			// So with this check, we only add seam trims if their end points
			// are the same within ON_ZERO_TOLERANCE. This will cause IsValid()
			// reporting "they should be seam trims connected to the same edge",
			// because the 2D tolerance (side_tol) are hardcoded to 1.0e-4.
			// We still add this check because we treat two vertexes to be the
			// same only if their distance < ON_ZERO_TOLERANCE. (Maybe 3D dist
			// should also be added to ON_Brep::IsValid()?)
			if (srf->PointAt(trim.PointAtStart().x, trim.PointAtStart().y).DistanceTo(
				srf->PointAt(loop[k]->PointAtEnd().x, loop[k]->PointAtEnd().y)) >= ON_ZERO_TOLERANCE) {
			    continue;
			}

			if (srf->PointAt(trim.PointAtEnd().x, trim.PointAtEnd().y).DistanceTo(
				srf->PointAt(loop[k]->PointAtStart().x, loop[k]->PointAtStart().y)) >= ON_ZERO_TOLERANCE) {
			    continue;
			}

			// We add another checking, which is not included in ON_Brep::IsValid()
			// - they should be iso boundaries of the surface.
			double s2 = srf->Domain(1 - endpt_index).NormalizedParameterAt(loop[k]->PointAtStart()[1 - endpt_index]);
			double s3 = srf->Domain(1 - endpt_index).NormalizedParameterAt(trim.PointAtStart()[1 - endpt_index]);
			if ((fabs(s2 - 1.0) < side_tol && fabs(s3) < side_tol) ||
			    (fabs(s2) < side_tol && fabs(s3 - 1.0) < side_tol)) {
			    // Find a trim that should share the same edge
			    int ti = brep->AddTrimCurve(loop[k]);
			    ON_BrepTrim &newtrim = brep->NewTrim(brep->m_E[trim.m_ei], true, breploop, ti);
			    // newtrim.m_type = ON_BrepTrim::seam;
			    newtrim.m_tolerance[0] = newtrim.m_tolerance[1] = MAX_FASTF;
			    seamed = true;
			    break;
			}
		    }
		    if (seamed) {
			continue;
		    }
		}
	    }
	}

	ON_Curve *c3d = NULL;
	// First, try the ON_Surface::Pushup() method.
	// If Pushup() does not succeed, use sampling method.
	c3d = face.SurfaceOf()->Pushup(*(loop[k]), INTERSECTION_TOL);
	if (!c3d) {
	    ON_3dPointArray ptarray(101);
	    for (int l = 0; l <= 100; l++) {
		ON_3dPoint pt2d;
		pt2d = loop[k]->PointAt(loop[k]->Domain().ParameterAt(l / 100.0));
		ptarray.Append(face.SurfaceOf()->PointAt(pt2d.x, pt2d.y));
	    }
	    c3d = new ON_PolylineCurve(ptarray);
	}

	if (c3d->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE) {
	    // The trim is singular
	    int i;
	    ON_3dPoint vtx = c3d->PointAtStart();
	    for (i = brep->m_V.Count() - 1; i >= 0; i--) {
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE) {
		    break;
		}
	    }
	    if (i < 0) {
		i = brep->m_V.Count();
		brep->NewVertex(c3d->PointAtStart(), 0.0);
	    }
	    int ti = brep->AddTrimCurve(loop[k]);
	    ON_BrepTrim &trim = brep->NewSingularTrim(brep->m_V[i], breploop, srf->IsIsoparametric(*loop[k]), ti);
	    trim.m_tolerance[0] = trim.m_tolerance[1] = MAX_FASTF;
	    delete c3d;
	    continue;
	}

	ON_2dPoint start = loop[k]->PointAtStart(), end = loop[k]->PointAtEnd();
	int start_idx, end_idx;

	// Get the start vertex index
	if (k > 0) {
	    start_idx = brep->m_T.Last()->m_vi[1];
	} else {
	    ON_3dPoint vtx = face.SurfaceOf()->PointAt(start.x, start.y);
	    int i;
	    for (i = 0; i < brep->m_V.Count(); i++) {
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE) {
		    break;
		}
	    }
	    start_idx = i;
	    if (i == brep->m_V.Count()) {
		brep->NewVertex(vtx, 0.0);
	    }
	}

	// Get the end vertex index
	if (c3d->IsClosed()) {
	    end_idx = start_idx;
	} else {
	    ON_3dPoint vtx = face.SurfaceOf()->PointAt(end.x, end.y);
	    int i;
	    for (i = 0; i < brep->m_V.Count(); i++) {
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE) {
		    break;
		}
	    }
	    end_idx = i;
	    if (i == brep->m_V.Count()) {
		brep->NewVertex(vtx, 0.0);
	    }
	}

	brep->AddEdgeCurve(c3d);
	int ti = brep->AddTrimCurve(loop[k]);
	ON_BrepEdge &edge = brep->NewEdge(brep->m_V[start_idx], brep->m_V[end_idx],
					  brep->m_C3.Count() - 1, (const ON_Interval *)0, MAX_FASTF);
	ON_BrepTrim &trim = brep->NewTrim(edge, 0, breploop, ti);
	trim.m_tolerance[0] = trim.m_tolerance[1] = MAX_FASTF;
    }
}

HIDDEN bool
is_point_on_brep_surface(const ON_3dPoint &pt, const ON_Brep *brep, ON_SimpleArray<Subsurface *> &surf_tree)
{
    // Decide whether a point is on a brep's surface.
    // Basic approach: use PSI on the point with all the surfaces.

    if (brep == NULL || pt.IsUnsetPoint()) {
	bu_log("is_point_on_brep_surface(): brep == NULL || pt.IsUnsetPoint()\n");
	return false;
    }

    if (surf_tree.Count() != brep->m_S.Count()) {
	bu_log("is_point_on_brep_surface(): surf_tree.Count() != brep->m_S.Count()\n");
	return false;
    }

    ON_BoundingBox bbox = brep->BoundingBox();
    bbox.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    bbox.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bbox.IsPointIn(pt)) {
	return false;
    }

    for (int i = 0; i < brep->m_F.Count(); i++) {
	const ON_BrepFace &face = brep->m_F[i];
	const ON_Surface *surf = face.SurfaceOf();
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (!ON_Intersect(pt, *surf, px_event, INTERSECTION_TOL, 0, 0, surf_tree[face.m_si])) {
	    continue;
	}

	// Get the trimming curves of the face, and determine whether the
	// points are inside the outerloop
	ON_SimpleArray<ON_Curve *> outerloop;
	const ON_BrepLoop &loop = brep->m_L[face.m_li[0]];  // outerloop only
	for (int j = 0; j < loop.m_ti.Count(); j++) {
	    outerloop.Append(brep->m_C2[brep->m_T[loop.m_ti[j]].m_c2i]);
	}
	ON_2dPoint pt2d(px_event[0].m_b[0], px_event[0].m_b[1]);
	try {
	    if (!is_point_outside_loop(pt2d, outerloop)) {
		return true;
	    }
	} catch (InvalidGeometry &e) {
	    bu_log("%s", e.what());
	}
    }

    return false;
}


HIDDEN bool
is_point_inside_brep(const ON_3dPoint &pt, const ON_Brep *brep, ON_SimpleArray<Subsurface *> &surf_tree)
{
    // Decide whether a point is inside a brep's surface.
    // Basic approach: intersect a ray with the brep, and count the number of
    // intersections (like the raytrace)
    // Returns true (inside) or false (outside) provided the pt is not on the
    // surfaces. (See also is_point_on_brep_surface())

    if (brep == NULL || pt.IsUnsetPoint()) {
	bu_log("is_point_inside_brep(): brep == NULL || pt.IsUnsetPoint()\n");
	return false;
    }

    if (surf_tree.Count() != brep->m_S.Count()) {
	bu_log("is_point_inside_brep(): surf_tree.Count() != brep->m_S.Count()\n");
	return false;
    }

    ON_BoundingBox bbox = brep->BoundingBox();
    bbox.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    bbox.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bbox.IsPointIn(pt)) {
	return false;
    }

    ON_3dVector diag = bbox.Diagonal() * 1.5; // Make it even longer
    ON_LineCurve line(pt, pt + diag);	// pt + diag should be outside, if pt
    // is inside the bbox

    ON_3dPointArray isect_pt;
    for (int i = 0; i < brep->m_F.Count(); i++) {
	const ON_BrepFace &face = brep->m_F[i];
	const ON_Surface *surf = face.SurfaceOf();
	ON_SimpleArray<ON_X_EVENT> x_event;
	if (!ON_Intersect(&line, surf, x_event, INTERSECTION_TOL, 0.0, 0, 0, 0, 0, 0, surf_tree[face.m_si])) {
	    continue;
	}

	// Get the trimming curves of the face, and determine whether the
	// points are inside the outerloop
	ON_SimpleArray<ON_Curve *> outerloop;
	const ON_BrepLoop &loop = brep->m_L[face.m_li[0]];  // outerloop only
	for (int j = 0; j < loop.m_ti.Count(); j++) {
	    outerloop.Append(brep->m_C2[brep->m_T[loop.m_ti[j]].m_c2i]);
	}
	try {
	    for (int j = 0; j < x_event.Count(); j++) {
		ON_2dPoint pt2d(x_event[j].m_b[0], x_event[j].m_b[1]);
		if (!is_point_outside_loop(pt2d, outerloop)) {
		    isect_pt.Append(x_event[j].m_B[0]);
		}
		if (x_event[j].m_type == ON_X_EVENT::ccx_overlap) {
		    pt2d = ON_2dPoint(x_event[j].m_b[2], x_event[j].m_b[3]);
		    if (!is_point_outside_loop(pt2d, outerloop)) {
			isect_pt.Append(x_event[j].m_B[1]);
		    }
		}
	    }
	} catch (InvalidGeometry &e) {
	    bu_log("%s", e.what());
	}
    }

    // Remove duplications
    ON_3dPointArray pt_no_dup;
    for (int i = 0; i < isect_pt.Count(); i++) {
	int j;
	for (j = 0; j < pt_no_dup.Count(); j++) {
	    if (isect_pt[i].DistanceTo(pt_no_dup[j]) < INTERSECTION_TOL) {
		break;
	    }
	}
	if (j == pt_no_dup.Count()) {
	    // No duplication, append to the array
	    pt_no_dup.Append(isect_pt[i]);
	}
    }

    return pt_no_dup.Count() % 2 != 0;
}

HIDDEN bool
is_point_inside_trimmed_face(const ON_2dPoint &pt, const TrimmedFace *tface)
{
    bool inside = false;
    if (is_point_inside_loop(pt, tface->m_outerloop)) {
	inside = true;
	for (size_t i = 0; i < tface->m_innerloop.size(); ++i) {
	    if (!is_point_outside_loop(pt, tface->m_innerloop[i])) {
		inside = false;
		break;
	    }
	}
    }
    return inside;
}

// TODO: For faces that have most of their area trimmed away, this is
// a very inefficient algorithm. If the grid approach doesn't work
// after a small number of samples, we should switch to an alternative
// algorithm, such as sampling around points on the inner loops.
HIDDEN ON_2dPoint
get_point_inside_trimmed_face(const TrimmedFace *tface)
{
    const int GP_MAX_STEPS = 8; // must be a power of two

    ON_PolyCurve polycurve;
    if (!is_loop_valid(tface->m_outerloop, ON_ZERO_TOLERANCE, &polycurve)) {
	throw InvalidGeometry("face_brep_location(): invalid outerloop.\n");
    }
    ON_BoundingBox bbox =  polycurve.BoundingBox();
    double u_len = bbox.m_max.x - bbox.m_min.x;
    double v_len = bbox.m_max.y - bbox.m_min.y;

    ON_2dPoint test_pt2d;
    bool found = false;
    for (int steps = 1; steps <= GP_MAX_STEPS && !found; steps *= 2) {
	double u_halfstep = u_len / (steps * 2.0);
	double v_halfstep = v_len / (steps * 2.0);

	for (int i = 0; i < steps && !found; ++i) {
	    test_pt2d.x = bbox.m_min.x + u_halfstep * (1 + 2 * i);

	    for (int j = 0; j < steps && !found; ++j) {
		test_pt2d.y = bbox.m_min.y + v_halfstep * (1 + 2 * j);
		found = is_point_inside_trimmed_face(test_pt2d, tface);
	    }
	}
    }
    if (!found) {
	throw AlgorithmError("Cannot find a point inside this trimmed face. Aborted.\n");
    }
    return test_pt2d;
}

enum {
    OUTSIDE_BREP,
    INSIDE_BREP,
    ON_BREP_SURFACE
};

// Returns the location of the face with respect to the brep.
//
// Throws InvalidGeometry if given invalid arguments.
// Throws AlgorithmError if a point inside the TrimmedFace can't be
// found for testing.
HIDDEN int
face_brep_location(const TrimmedFace *tface, const ON_Brep *brep, ON_SimpleArray<Subsurface *> &surf_tree)
{
    if (tface == NULL || brep == NULL) {
	throw InvalidGeometry("face_brep_location(): given NULL argument.\n");
    }

    const ON_BrepFace *bface = tface->m_face;
    if (bface == NULL) {
	throw InvalidGeometry("face_brep_location(): TrimmedFace has NULL face.\n");
    }

    ON_BoundingBox brep2box = brep->BoundingBox();
    brep2box.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    brep2box.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bface->BoundingBox().Intersection(brep2box)) {
	return OUTSIDE_BREP;
    }

    if (tface->m_outerloop.Count() == 0) {
	throw InvalidGeometry("face_brep_location(): the input TrimmedFace is not trimmed.\n");
    }

    ON_PolyCurve polycurve;
    if (!is_loop_valid(tface->m_outerloop, ON_ZERO_TOLERANCE, &polycurve)) {
	throw InvalidGeometry("face_brep_location(): invalid outerloop.\n");
    }
    ON_2dPoint test_pt2d = get_point_inside_trimmed_face(tface);
    ON_3dPoint test_pt3d = tface->m_face->PointAt(test_pt2d.x, test_pt2d.y);

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("valid test point: (%g, %g, %g)\n", test_pt3d.x, test_pt3d.y, test_pt3d.z);
    }

    if (is_point_on_brep_surface(test_pt3d, brep, surf_tree)) {
	// because the overlap parts will be split out as separated trimmed
	// faces, if one point on a trimmed face is on the brep's surface,
	// the whole trimmed face should be on the surface
	return ON_BREP_SURFACE;
    }

    return is_point_inside_brep(test_pt3d, brep, surf_tree) ? INSIDE_BREP : OUTSIDE_BREP;
}

HIDDEN ON_ClassArray<ON_SimpleArray<SSICurve> >
get_face_intersection_curves(
	ON_SimpleArray<Subsurface *> &surf_tree1,
	ON_SimpleArray<Subsurface *> &surf_tree2,
	const ON_Brep *brep1,
	const ON_Brep *brep2,
	op_type operation)
{
    std::set<int> unused1, unused2;
    std::set<int> finalform1, finalform2;
    int face_count1 = brep1->m_F.Count();
    int face_count2 = brep2->m_F.Count();

    /* Depending on the operation type and the bounding box behaviors, we
     * can sometimes decide immediately whether a face will end up in the
     * final brep or will have no role in the intersections - do that
     * categorization up front */
    for (int i = 0; i < face_count1 + face_count2; i++) {
	const ON_BrepFace &face = i < face_count1 ? brep1->m_F[i] : brep2->m_F[i - face_count1];
	const ON_Brep *brep = i < face_count1 ? brep2 : brep1;
	std::set<int> *unused = i < face_count1 ? &unused1 : &unused2;
	std::set<int> *intact = i < face_count1 ? &finalform1 : &finalform2;
	int curr_index = i < face_count1 ? i : i - face_count1;
	if (face.BoundingBox().MinimumDistanceTo(brep->BoundingBox()) > ON_ZERO_TOLERANCE) {
	    switch (operation) {
	    case BOOLEAN_UNION:
		intact->insert(curr_index);
		break;
	    case BOOLEAN_DIFF:
		if (i < face_count1) {
		    intact->insert(curr_index);
		}
		if (i >= face_count1) {
		    unused->insert(curr_index);
		}
		break;
	    case BOOLEAN_INTERSECT:
		unused->insert(curr_index);
		break;
	    default:
		throw InvalidBooleanOperation("Error - unknown "
					      "boolean operation\n");
	    }
	}
    }

    // For the faces that we can't rule out, there are several possible roles they can play:
    //
    // 1.  Fully utilized in the new brep
    // 2.  Changed into a new set of faces by intersections, each of which must be evaluated
    // 3.  Fully discarded by the new brep
    //
    // We won't be able to distinguish between 1 and 3 at this stage, but we can narrow in
    // on which faces might fall into category 2 and what faces they might interact with.
    std::set<std::pair<int, int> > intersection_candidates;
    for (int i = 0; i < face_count1; i++) {
	if (unused1.find(i) == unused1.end() && finalform1.find(i) == finalform1.end()) {
	    for (int j = 0; j < face_count2; j++) {
		if (unused2.find(j) == unused2.end() &&  finalform2.find(j) == finalform2.end()) {
		    // If the two faces don't interact according to their bounding boxes,
		    // they won't be a source of events - otherwise, they must be checked.
		    fastf_t disjoint = brep1->m_F[i].BoundingBox().MinimumDistanceTo(brep2->m_F[j].BoundingBox());
		    if (!(disjoint > ON_ZERO_TOLERANCE)) {
			intersection_candidates.insert(std::pair<int, int>(i, j));
		    }
		}
	    }
	}
    }

    // For those not in category 2 an inside/outside test on the breps combined with the boolean op
    // should be enough to decide the issue, but there is a problem.  If *all* faces of a brep are
    // inside the other brep and the operation is a subtraction, we don't want a "floating" inside-out
    // brep volume inside the outer volume and topologically isolated.  Normally this is handled by
    // creating a face that connects the outer and inner shells, but this is potentially a non-trivial
    // operation.  The only thing that comes immediately to mind is to find the center point of the
    // bounding box of the inner brep, create a plane using that point and the z+ unit vector for a normal, and
    // cut both breps in half with that plane to form four new breps and two new subtraction problems.
    //
    // More broadly, this is a problem - unioning two half-spheres with a sphere subtracted out of their
    // respective centers will end up with isolated surfaces in the middle of the union unless the routines
    // know they must keep one of the coplanar faces in order to topologically connect the middle.  However,
    // in the case where there is no center sphere the central face should be removed.  It may be that the
    // condition to satisfy for removal is no interior trimming loops on the face.
    //
    //
    // Also worth thinking about - would it be possible to then do edge comparisons to
    // determine which of the "fully used/fully non-used" faces are needed?

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("Summary of brep status: \n unused1: %d\n unused2: %d\n finalform1: %d\n finalform2 %d\nintersection_candidates(%d):\n", unused1.size(), unused2.size(), finalform1.size(), finalform2.size(), intersection_candidates.size());
	for (std::set<std::pair<int, int> >::iterator it = intersection_candidates.begin(); it != intersection_candidates.end(); ++it) {
	    bu_log("     (%d,%d)\n", (*it).first, (*it).second);
	}
    }

    int surf_count1 = brep1->m_S.Count();
    int surf_count2 = brep2->m_S.Count();
    for (int i = 0; i < surf_count1; i++) {
	surf_tree1.Append(new Subsurface(brep1->m_S[i]->Duplicate()));
    }
    for (int i = 0; i < surf_count2; i++) {
	surf_tree2.Append(new Subsurface(brep2->m_S[i]->Duplicate()));
    }

    ON_ClassArray<ON_SimpleArray<SSICurve> > curves_array(face_count1 + face_count2);

    // count must equal capacity for array copy to work as expected
    // when the result of the function is assigned
    curves_array.SetCount(curves_array.Capacity());

    // calculate intersection curves
    for (int i = 0; i < face_count1; i++) {
	for (int j = 0; j < face_count2; j++) {
	    if (intersection_candidates.find(std::pair<int, int>(i, j)) != intersection_candidates.end()) {
		ON_Surface *surf1, *surf2;
		ON_ClassArray<ON_SSX_EVENT> events;
		int results = 0;
		surf1 = brep1->m_S[brep1->m_F[i].m_si];
		surf2 = brep2->m_S[brep2->m_F[j].m_si];
		if (is_same_surface(surf1, surf2)) {
		    continue;
		}

		// Possible enhancement: Some faces may share the same surface.
		// We can store the result of SSI to avoid re-computation.
		results = ON_Intersect(surf1,
				       surf2,
				       events,
				       INTERSECTION_TOL,
				       0.0,
				       0.0,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       surf_tree1[brep1->m_F[i].m_si],
				       surf_tree2[brep2->m_F[j].m_si]);
		if (results <= 0) {
		    continue;
		}

		dplot->SSX(events, brep1, brep1->m_F[i].m_si, brep2, brep2->m_F[j].m_si);

		ON_SimpleArray<ON_Curve *> face1_curves, face2_curves;
		for (int k = 0; k < events.Count(); k++) {
		    if (events[k].m_type == ON_SSX_EVENT::ssx_tangent ||
			events[k].m_type == ON_SSX_EVENT::ssx_transverse ||
			events[k].m_type == ON_SSX_EVENT::ssx_overlap)
		    {
			ON_SimpleArray<ON_Curve *> subcurves_on1, subcurves_on2;

			get_subcurves_inside_faces(subcurves_on1,
				subcurves_on2, brep1, brep2, i, j, &events[k]);

			for (int l = 0; l < subcurves_on1.Count(); ++l) {
			    SSICurve ssi_on1;
			    ssi_on1.m_curve = subcurves_on1[l];
			    curves_array[i].Append(ssi_on1);

			    face1_curves.Append(subcurves_on1[l]);
			}
			for (int l = 0; l < subcurves_on2.Count(); ++l) {
			    SSICurve ssi_on2;
			    ssi_on2.m_curve = subcurves_on2[l];
			    curves_array[face_count1 + j].Append(ssi_on2);

			    face2_curves.Append(subcurves_on2[l]);
			}
		    }
		}
		dplot->ClippedFaceCurves(surf1, surf2, face1_curves, face2_curves);

		if (DEBUG_BREP_BOOLEAN) {
		    // Look for coplanar faces
		    ON_Plane surf1_plane, surf2_plane;
		    if (surf1->IsPlanar(&surf1_plane) && surf2->IsPlanar(&surf2_plane)) {
			/* We already checked for disjoint above, so the only remaining question is the normals */
			if (surf1_plane.Normal().IsParallelTo(surf2_plane.Normal())) {
			    bu_log("Faces brep1->%d and brep2->%d are coplanar and intersecting\n", i, j);
			}
		    }
		}

	    }
	}
    }

    return curves_array;
}

HIDDEN ON_SimpleArray<ON_Curve *>get_face_trim_loop(const ON_Brep *brep, int face_loop_index)
{
    const ON_BrepLoop &loop = brep->m_L[face_loop_index];
    const ON_SimpleArray<int> &trim_index = loop.m_ti;
    ON_SimpleArray<ON_Curve *> face_trim_loop;
    for (int i = 0; i < trim_index.Count(); ++i) {
	ON_Curve *curve2d = brep->m_C2[brep->m_T[trim_index[i]].m_c2i];
	face_trim_loop.Append(curve2d->Duplicate());
    }
    return face_trim_loop;
}

HIDDEN ON_SimpleArray<TrimmedFace *>
get_trimmed_faces(const ON_Brep *brep)
{
    ON_SimpleArray<TrimmedFace *> trimmed_faces;
    int face_count = brep->m_F.Count();
    for (int i = 0; i < face_count; i++) {
	const ON_BrepFace &face = brep->m_F[i];
	const ON_SimpleArray<int> &loop_index = face.m_li;

	TrimmedFace *trimmed_face = new TrimmedFace();
	trimmed_face->m_face = &face;

	if (loop_index.Count() > 0) {
	    ON_SimpleArray<ON_Curve *> index_loop = get_face_trim_loop(brep, loop_index[0]);
	    trimmed_face->m_outerloop = index_loop;
	    for (int j = 1; j < loop_index.Count(); j++) {
		index_loop = get_face_trim_loop(brep, loop_index[j]);
		trimmed_face->m_innerloop.push_back(index_loop);
	    }
	}
	trimmed_faces.Append(trimmed_face);
    }
    return trimmed_faces;
}

HIDDEN void
categorize_trimmed_faces(
	ON_ClassArray<ON_SimpleArray<TrimmedFace *> > &trimmed_faces,
	const ON_Brep *brep1,
	const ON_Brep *brep2,
	ON_SimpleArray<Subsurface *> &surf_tree1,
	ON_SimpleArray<Subsurface *> &surf_tree2,
	op_type operation)
{
    int face_count1 = brep1->m_F.Count();
    for (int i = 0; i < trimmed_faces.Count(); i++) {
	/* Perform inside-outside test to decide whether the trimmed face should
	 * be used in the final b-rep structure or not.
	 * Different operations should be dealt with accordingly.
	 * Use connectivity graphs (optional) which represents the topological
	 * structure of the b-rep. This can reduce time-consuming inside-outside
	 * tests.
	 */
	const ON_SimpleArray<TrimmedFace *> &splitted = trimmed_faces[i];
	const ON_Brep *another_brep = i >= face_count1 ? brep1 : brep2;
	ON_SimpleArray<Subsurface *> &surf_tree = i >= face_count1 ? surf_tree1 : surf_tree2;
	for (int j = 0; j < splitted.Count(); j++) {
	    if (splitted[j]->m_belong_to_final != TrimmedFace::UNKNOWN) {
		// Visited before, don't need to test again
		continue;
	    }

	    int face_location = -1;
	    try {
		face_location = face_brep_location(splitted[j], another_brep, surf_tree);
	    } catch (InvalidGeometry &e) {
		bu_log("%s", e.what());
	    } catch (AlgorithmError &e) {
		bu_log("%s", e.what());
	    }
	    if (face_location < 0) {
		if (DEBUG_BREP_BOOLEAN) {
		    bu_log("Whether the trimmed face is inside/outside is unknown.\n");
		}
		splitted[j]->m_belong_to_final = TrimmedFace::NOT_BELONG;
		continue;
	    }

	    splitted[j]->m_rev = false;
	    splitted[j]->m_belong_to_final = TrimmedFace::NOT_BELONG;
	    switch (face_location) {
		case INSIDE_BREP:
		    if (operation == BOOLEAN_INTERSECT ||
			operation == BOOLEAN_XOR ||
			(operation == BOOLEAN_DIFF && i >= face_count1))
		    {
			splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		    }
		    if (operation == BOOLEAN_DIFF || operation == BOOLEAN_XOR) {
			splitted[j]->m_rev = true;
		    }
		    break;
		case OUTSIDE_BREP:
		    if (operation == BOOLEAN_UNION ||
			operation == BOOLEAN_XOR ||
			(operation == BOOLEAN_DIFF && i < face_count1))
		    {
			splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		    }
		    break;
		case ON_BREP_SURFACE:
		    // get a 3d point on the face
		    ON_2dPoint face_pt2d = get_point_inside_trimmed_face(splitted[j]);
		    ON_3dPoint face_pt3d = splitted[j]->m_face->PointAt(face_pt2d.x, face_pt2d.y);

		    // find the matching point on the other brep
		    ON_3dPoint brep_pt2d;
		    const ON_Surface *brep_surf;
		    bool found = false;
		    for (int fi = 0; fi < another_brep->m_F.Count(); ++fi) {
			const ON_BrepFace &face = another_brep->m_F[fi];
			brep_surf = face.SurfaceOf();
			ON_ClassArray<ON_PX_EVENT> px_event;

			if (ON_Intersect(face_pt3d, *brep_surf, px_event,
				    INTERSECTION_TOL, 0, 0, surf_tree[face.m_si]))
			{
			    found = true;
			    brep_pt2d = px_event[0].m_b;
			    break;
			}
		    }
		    if (found) {
			// compare normals of surfaces at shared point
			ON_3dVector brep_norm, face_norm;
			brep_surf->EvNormal(brep_pt2d.x, brep_pt2d.y, brep_norm);
			splitted[j]->m_face->SurfaceOf()->EvNormal(face_pt2d.x, face_pt2d.y, face_norm);

			double dot = ON_DotProduct(brep_norm, face_norm);
			bool same_direction = false;
			if (dot > 0) {
			    // normals appear to have same direction
			    same_direction = true;
			}

			if ((operation == BOOLEAN_UNION && same_direction) ||
			    (operation == BOOLEAN_INTERSECT && same_direction) ||
			    (operation == BOOLEAN_DIFF && !same_direction && i < face_count1))
			{
			    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
			}
		    }
		    // TODO: Actually only one of them is needed in the final brep structure
	    }
	    if (DEBUG_BREP_BOOLEAN) {
		bu_log("The trimmed face is %s the other brep.",
			(face_location == INSIDE_BREP) ? "inside" :
			((face_location == OUTSIDE_BREP) ? "outside" : "on the surface of"));
	    }
	}
    }
}

HIDDEN ON_ClassArray<ON_SimpleArray<TrimmedFace *> >
get_evaluated_faces(const ON_Brep *brep1, const ON_Brep *brep2, op_type operation)
{
    ON_SimpleArray<Subsurface *> surf_tree1, surf_tree2;

    ON_ClassArray<ON_SimpleArray<SSICurve> > curves_array =
	get_face_intersection_curves(surf_tree1, surf_tree2, brep1, brep2, operation);

    int face_count1 = brep1->m_F.Count();
    int face_count2 = brep2->m_F.Count();

    ON_SimpleArray<TrimmedFace *> brep1_faces, brep2_faces;
    brep1_faces = get_trimmed_faces(brep1);
    brep2_faces = get_trimmed_faces(brep2);

    ON_SimpleArray<TrimmedFace *> original_faces = brep1_faces;
    for (int i = 0; i < brep2_faces.Count(); ++i) {
	original_faces.Append(brep2_faces[i]);
    }

    if (original_faces.Count() != face_count1 + face_count2) {
	throw GeometryGenerationError("ON_Boolean() Error: TrimmedFace"
				      " generation failed.\n");
    }

    // split the surfaces with the intersection curves
    ON_ClassArray<ON_SimpleArray<TrimmedFace *> > trimmed_faces;
    for (int i = 0; i < original_faces.Count(); i++) {
	TrimmedFace *first = original_faces[i];

	ON_ClassArray<LinkedCurve> linked_curves =
	    link_curves(first->m_face->SurfaceOf(), curves_array[i]);

	dplot->LinkedCurves(first->m_face->SurfaceOf(), linked_curves);

	ON_SimpleArray<TrimmedFace *> splitted = split_trimmed_face(first, linked_curves);
	trimmed_faces.Append(splitted);

	// Delete the curves passed in.
	// Only the copies of them will be used later.
	for (int j = 0; j < linked_curves.Count(); j++) {
	    for (int k = 0; k < linked_curves[j].m_ssi_curves.Count(); k++) {
		if (linked_curves[j].m_ssi_curves[k].m_curve) {
		    delete linked_curves[j].m_ssi_curves[k].m_curve;
		    linked_curves[j].m_ssi_curves[k].m_curve = NULL;
		}
	    }
	}
    }

    if (trimmed_faces.Count() != original_faces.Count()) {
	throw GeometryGenerationError("ON_Boolean() Error: "
				      "trimmed_faces.Count() != original_faces.Count()\n");
    }

    for (int i = 0; i < original_faces.Count(); i++) {
	delete original_faces[i];
	original_faces[i] = NULL;
    }

    categorize_trimmed_faces(trimmed_faces, brep1, brep2, surf_tree1, surf_tree2, operation);

    dplot->SplitFaces(trimmed_faces);

    for (int i = 0; i < surf_tree1.Count(); i++) {
	delete surf_tree1[i];
    }

    for (int i = 0; i < surf_tree2.Count(); i++) {
	delete surf_tree2[i];
    }

    return trimmed_faces;
}

HIDDEN void
standardize_loop_orientations(ON_Brep *brep)
{
    std::map<int, int> reversed_curve2d, reversed_edge;
    for (int face_idx = 0; face_idx < brep->m_F.Count(); ++face_idx) {
	const ON_BrepFace &eb_face = brep->m_F[face_idx];

	for (int loop_idx = 0; loop_idx < eb_face.LoopCount(); ++loop_idx) {
	    const ON_BrepLoop &face_loop = brep->m_L[eb_face.m_li[loop_idx]];
	    if (face_loop.m_type != ON_BrepLoop::outer &&
		face_loop.m_type != ON_BrepLoop::inner) {
		continue;
	    }

	    int loop_direction = brep->LoopDirection(face_loop);
	    if ((loop_direction == LOOP_DIRECTION_CCW && face_loop.m_type == ON_BrepLoop::inner) ||
		(loop_direction == LOOP_DIRECTION_CW && face_loop.m_type == ON_BrepLoop::outer))
	    {
		// found reversed loop
		int brep_li = eb_face.m_li[loop_idx];
		ON_BrepLoop &reversed_loop = brep->m_L[brep_li];

		// reverse all the loop's curves
		for (int trim_idx = 0; trim_idx < reversed_loop.TrimCount(); ++trim_idx) {
		    ON_BrepTrim *trim = reversed_loop.Trim(trim_idx);

		    // Replace trim curve2d with a reversed copy.
		    // We'll use a previously made curve, or else
		    // make a new one.
		    if (reversed_curve2d.find(trim->m_c2i) != reversed_curve2d.end()) {
			trim->ChangeTrimCurve(reversed_curve2d[trim->m_c2i]);
		    } else {
			ON_Curve *curve_copy = trim->TrimCurveOf()->DuplicateCurve();
			int copy_c2i = brep->AddTrimCurve(curve_copy);

			reversed_curve2d[trim->m_c2i] = copy_c2i;

			trim->ChangeTrimCurve(copy_c2i);
			trim->Reverse();
		    }
		    // Replace trim edge with a reversed copy.
		    // We'll use a previously made edge, or else
		    // make a new one.
		    if (reversed_edge.find(trim->m_ei) != reversed_edge.end()) {
			trim->RemoveFromEdge(false, false);
			trim->AttachToEdge(reversed_edge[trim->m_ei], trim->m_bRev3d);
		    } else {
			ON_BrepEdge *edge = trim->Edge();
			ON_BrepVertex &v_start = *edge->Vertex(0);
			ON_BrepVertex &v_end = *edge->Vertex(1);
			ON_Interval dom = edge->ProxyCurveDomain();

			ON_Curve *curve_copy = trim->EdgeCurveOf()->DuplicateCurve();
			int copy_c3i = brep->AddEdgeCurve(curve_copy);
			ON_BrepEdge &edge_copy = brep->NewEdge(v_start,
				v_end, copy_c3i, &dom, edge->m_tolerance);

			reversed_edge[trim->m_ei] = copy_c3i;

			trim->RemoveFromEdge(false, false);
			trim->AttachToEdge(edge_copy.m_edge_index, trim->m_bRev3d);
			trim->Edge()->Reverse();
		    }
		}
		// need to reverse the order of trims in the loop
		// too so they appear continuous
		reversed_loop.m_ti.Reverse();
	    }
	}
    }
}

int
ON_Boolean(ON_Brep *evaluated_brep, const ON_Brep *brep1, const ON_Brep *brep2, op_type operation)
{
    static int calls = 0;
    ++calls;
    std::ostringstream prefix;
    prefix << "bool" << calls;
    dplot = new DebugPlot(prefix.str().c_str());
    dplot->Surfaces(brep1, brep2);

    ON_ClassArray<ON_SimpleArray<TrimmedFace *> > trimmed_faces;
    try {
	/* Deal with the trivial cases up front */
	if (brep1->BoundingBox().MinimumDistanceTo(brep2->BoundingBox()) > ON_ZERO_TOLERANCE) {
	    switch (operation) {
	    case BOOLEAN_UNION:
		evaluated_brep->Append(*brep1);
		evaluated_brep->Append(*brep2);
		break;
	    case BOOLEAN_DIFF:
		evaluated_brep->Append(*brep1);
		break;
	    case BOOLEAN_INTERSECT:
		return 0;
		break;
	    default:
		throw InvalidBooleanOperation("Error - unknown boolean operation\n");
	    }
	    evaluated_brep->ShrinkSurfaces();
	    evaluated_brep->Compact();
	    dplot->WriteLog();
	    return 0;
	}
	trimmed_faces = get_evaluated_faces(brep1, brep2, operation);
    } catch (InvalidBooleanOperation &e) {
	bu_log("%s", e.what());
	dplot->WriteLog();
	return -1;
    } catch (GeometryGenerationError &e) {
	bu_log("%s", e.what());
	dplot->WriteLog();
	return -1;
    }

    int face_count1 = brep1->m_F.Count();
    int face_count2 = brep2->m_F.Count();
    for (int i = 0; i < trimmed_faces.Count(); i++) {
	const ON_SimpleArray<TrimmedFace *> &splitted = trimmed_faces[i];
	const ON_Surface *surf = splitted.Count() ? splitted[0]->m_face->SurfaceOf() : NULL;
	bool added = false;
	for (int j = 0; j < splitted.Count(); j++) {
	    TrimmedFace *t_face = splitted[j];
	    if (t_face->m_belong_to_final == TrimmedFace::BELONG) {
		// Add the surfaces, faces, loops, trims, vertices, edges, etc.
		// to the brep structure.
		if (!added) {
		    ON_Surface *new_surf = surf->Duplicate();
		    evaluated_brep->AddSurface(new_surf);
		    added = true;
		}
		ON_BrepFace &new_face = evaluated_brep->NewFace(evaluated_brep->m_S.Count() - 1);

		add_elements(evaluated_brep, new_face, t_face->m_outerloop, ON_BrepLoop::outer);
		// ON_BrepLoop &loop = evaluated_brep->m_L[evaluated_brep->m_L.Count() - 1];
		for (unsigned int k = 0; k < t_face->m_innerloop.size(); k++) {
		    add_elements(evaluated_brep, new_face, t_face->m_innerloop[k], ON_BrepLoop::inner);
		}

		evaluated_brep->SetTrimIsoFlags(new_face);
		const ON_BrepFace &original_face = i >= face_count1 ? brep2->m_F[i - face_count1] : brep1->m_F[i];
		if (original_face.m_bRev ^ t_face->m_rev) {
		    evaluated_brep->FlipFace(new_face);
		}
	    }
	}
    }

    for (int i = 0; i < face_count1 + face_count2; i++) {
	for (int j = 0; j < trimmed_faces[i].Count(); j++) {
	    if (trimmed_faces[i][j]) {
		delete trimmed_faces[i][j];
		trimmed_faces[i][j] = NULL;
	    }
	}
    }

    evaluated_brep->ShrinkSurfaces();
    evaluated_brep->Compact();
    standardize_loop_orientations(evaluated_brep);

    // Check IsValid() and output the message.
    ON_wString ws;
    ON_TextLog log(ws);
    evaluated_brep->IsValid(&log);
    bu_log(ON_String(ws).Array());

    dplot->WriteLog();
    delete dplot;
    dplot = NULL;

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
