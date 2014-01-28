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
#include <stdexcept>
#include "bio.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"
#include "raytrace.h"

// Whether to output the debug messages about b-rep booleans.
#define DEBUG_BREP_BOOLEAN 0

// tol value used in ON_Intersect()s. We use a smaller tolerance than the
// default one 0.001.
#define INTERSECTION_TOL 1e-4

// tol value used in ON_3dVector::IsParallelTo(). We use a smaller tolerance
// than the default one ON_PI/180.
#define ANGLE_TOL ON_PI/1800.0

class InvalidBooleanOperation : public std::invalid_argument
{
public:
    InvalidBooleanOperation(const std::string &msg = "") : std::invalid_argument(msg) {}
};

class InvalidGeometry : public std::invalid_argument
{
public:
    InvalidGeometry(const std::string &msg = "") : std::invalid_argument(msg) {}
};

class GeometryGenerationError : public std::runtime_error
{
public:
    GeometryGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};

class IntervalGenerationError : public std::runtime_error
{
public:
    IntervalGenerationError(const std::string &msg = "") : std::runtime_error(msg) {}
};

struct IntersectPoint {
    ON_3dPoint m_pt;	// 3D intersection point
    int m_seg;		// which curve of the loop
    double m_t;		// param on the loop curve
    int m_type;		// which intersection curve
    int m_rank;		// rank on the chain
    double m_t_for_rank;// param on the SSI curve
    enum {
	UNSET,
	IN,
	OUT
    } m_in_out;		// dir is going inside/outside
    int m_pos;		// between curve[m_pos] and curve[m_pos+1]
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

HIDDEN void
append_to_polycurve(ON_Curve *curve, ON_PolyCurve &polycurve);
// We link the SSICurves that share an endpoint, and form this new structure,
// which has many similar behaviors as ON_Curve, e.g. PointAt(), Reverse().
struct LinkedCurve {
private:
    ON_Curve *m_curve;	// an explicit storage of the whole curve
public:
    // The curves contained in this LinkedCurve, including
    // the information needed by the connectivity graph
    ON_SimpleArray<SSICurve> m_ssi_curves;

    // Default constructor
    LinkedCurve()
    {
	m_curve = NULL;
    }

    ~LinkedCurve()
    {
	if (m_curve) {
	    delete m_curve;
	}
	m_curve = NULL;
    }

    LinkedCurve &operator= (const LinkedCurve &_lc)
    {
	m_curve = _lc.m_curve ? _lc.m_curve->Duplicate() : NULL;
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

    bool IsClosed() const
    {
	if (m_ssi_curves.Count() == 0) {
	    return false;
	}
	return PointAtStart().DistanceTo(PointAtEnd()) < ON_ZERO_TOLERANCE;
    }

    bool IsValid() const
    {
	// Check whether the curve has "gaps".
	for (int i = 1; i < m_ssi_curves.Count(); i++) {
	    if (m_ssi_curves[i].m_curve->PointAtStart().DistanceTo(m_ssi_curves[i - 1].m_curve->PointAtEnd()) >= ON_ZERO_TOLERANCE) {
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
	return sub_curve(c, t1, t2);
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
compare_t(const IntersectPoint *p1, const IntersectPoint *p2)
{
    // Use for sorting an array. Use strict FP comparison.
    if (p1->m_seg != p2->m_seg) {
	return p1->m_seg - p2->m_seg;
    }
    return p1->m_t > p2->m_t ? 1 : (p1->m_t < p2->m_t ? -1 : 0);
}


HIDDEN int
compare_for_rank(IntersectPoint * const *p1, IntersectPoint * const *p2)
{
    // Use for sorting an array. Use strict FP comparison.
    return (*p1)->m_t_for_rank > (*p2)->m_t_for_rank ? 1 : ((*p1)->m_t_for_rank < (*p2)->m_t_for_rank ? -1 : 0);
}


HIDDEN void
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
    if (ret && polycurve->PointAtStart().DistanceTo(polycurve->PointAtEnd()) >= ON_ZERO_TOLERANCE) {
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
    ON_SimpleArray<ON_X_EVENT> x_event;
    ON_Intersect(&linecurve, &polycurve, x_event, INTERSECTION_TOL);
    int count = x_event.Count();
    for (int i = 0; i < x_event.Count(); i++) {
	// Find tangent intersections.
	// What should we do if it's ccx_overlap?
	if (polycurve.TangentAt(x_event[i].m_b[0]).IsParallelTo(linecurve.m_line.Direction(), ANGLE_TOL)) {
	    count++;
	}
    }

    return (count % 2) ? INSIDE_OR_ON_LOOP : OUTSIDE_OR_ON_LOOP;
}


// Returns whether or not point is on the loop boundary.
// Throws InvalidGeometry if loop is invalid.
HIDDEN bool
is_point_on_loop(const ON_2dPoint &pt, const ON_SimpleArray<ON_Curve *> &loop)
{
    ON_PolyCurve polycurve;
    if (!is_loop_valid(loop, ON_ZERO_TOLERANCE, &polycurve)) {
	throw InvalidGeometry("is_ponit_on_loop() given invalid loop\n");
    }

    ON_ClassArray<ON_PX_EVENT> px_event;
    return ON_Intersect(ON_3dPoint(pt), polycurve, px_event, INTERSECTION_TOL) ? 1 : 0;
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
    ON_Curve *subcurve = sub_curve(curve, interval.Min(), interval.Max());
    delete curve;
    curve = subcurve;

    if (curve == NULL) {
	throw GeometryGenerationError("replace_curve_with_subcurve(): NULL subcurve\n");
    }
}

HIDDEN int
get_subcurve_inside_faces(const ON_Brep *brep1, const ON_Brep *brep2, int face_i1, int face_i2, ON_SSX_EVENT *event)
{
    // The ON_SSX_EVENT from SSI is the intersection of two whole surfaces.
    // We need to get the part that lies inside both trimmed patches.
    // (brep1's face[face_i1] and brep2's face[face_i2])
    // returns 0 for success, -1 for error.

    if (event == NULL) {
	return -1;
    }

    if (event->m_curve3d == NULL || event->m_curveA == NULL || event->m_curveB == NULL) {
	return -1;
    }

    // 1. Get the outerloops.
    ON_SimpleArray<ON_Curve *> outerloop1, outerloop2;
    if (face_i1 < 0 || face_i1 >= brep1->m_F.Count()) {
	bu_log("get_subcurve_inside_faces(): invalid face_i1 (%d).\n", face_i1);
	return -1;
    }
    if (face_i2 < 0 || face_i2 >= brep2->m_F.Count()) {
	bu_log("get_subcurve_inside_faces(): invalid face_i2 (%d).\n", face_i2);
	return -1;
    }
    const ON_BrepLoop &loop1 = brep1->m_L[brep1->m_F[face_i1].m_li[0]];
    const ON_BrepLoop &loop2 = brep2->m_L[brep2->m_F[face_i2].m_li[0]];

    for (int i = 0; i < loop1.TrimCount(); i++) {
	outerloop1.Append(brep1->m_C2[brep1->m_T[loop1.m_ti[i]].m_c2i]);
    }
    for (int i = 0; i < loop2.TrimCount(); i++) {
	outerloop2.Append(brep2->m_C2[brep2->m_T[loop2.m_ti[i]].m_c2i]);
    }

    // 2.1 Intersect the curves in event with outerloop1, and get the parts
    // inside. (Represented with param intervals on the curve's domain [0, 1])
    ON_SimpleArray<double> isect_location;
    ON_SimpleArray<ON_Interval> intervals1, intervals2;
    ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
    for (int i = 0; i < outerloop1.Count(); i++) {
	ON_Intersect(event->m_curveA, outerloop1[i], x_event1, INTERSECTION_TOL);
    }
    for (int i = 0; i < x_event1.Count(); i++) {
	isect_location.Append(x_event1[i].m_a[0]);
	if (x_event1[i].m_type == ON_X_EVENT::ccx_overlap) {
	    isect_location.Append(x_event1[i].m_a[1]);
	}
    }
    isect_location.QuickSort(ON_CompareIncreasing);
    if (isect_location.Count() != 0) {
	if (!ON_NearZero(isect_location[0] - event->m_curveA->Domain().Min())) {
	    isect_location.Insert(0, event->m_curveA->Domain().Min());
	}
	if (!ON_NearZero(*isect_location.Last() - event->m_curveA->Domain().Max())) {
	    isect_location.Append(event->m_curveA->Domain().Max());
	}
	for (int i = 0; i < isect_location.Count() - 1; i++) {
	    ON_Interval interval(isect_location[i], isect_location[i + 1]);
	    if (ON_NearZero(interval.Length())) {
		continue;
	    }
	    ON_2dPoint pt = event->m_curveA->PointAt(interval.Mid());
	    try {
		if (is_point_inside_loop(pt, outerloop1)) {
		    intervals1.Append(interval);
		}
	    } catch (InvalidGeometry &e) {
		bu_log("%s", e.what());
	    }
	}
    } else {
	intervals1.Append(event->m_curveA->Domain());    // No intersection
    }

    // 2.2 Intersect the curves in event with outerloop2, and get the parts
    // inside. (Represented with param intervals on the curve's domain [0, 1])
    isect_location.Empty();
    for (int i = 0; i < outerloop2.Count(); i++) {
	ON_Intersect(event->m_curveB, outerloop2[i], x_event2, INTERSECTION_TOL);
    }
    for (int i = 0; i < x_event2.Count(); i++) {
	isect_location.Append(x_event2[i].m_a[0]);
	if (x_event2[i].m_type == ON_X_EVENT::ccx_overlap) {
	    isect_location.Append(x_event2[i].m_a[1]);
	}
    }
    isect_location.QuickSort(ON_CompareIncreasing);
    if (isect_location.Count() != 0) {
	if (!ON_NearZero(isect_location[0] - event->m_curveB->Domain().Min())) {
	    isect_location.Insert(0, event->m_curveB->Domain().Min());
	}
	if (!ON_NearZero(*isect_location.Last() - event->m_curveB->Domain().Max())) {
	    isect_location.Append(event->m_curveB->Domain().Max());
	}
	for (int i = 0; i < isect_location.Count() - 1; i++) {
	    ON_Interval interval(isect_location[i], isect_location[i + 1]);
	    if (ON_NearZero(interval.Length())) {
		continue;
	    }
	    ON_2dPoint pt = event->m_curveB->PointAt(interval.Mid());
	    try {
		if (is_point_inside_loop(pt, outerloop2)) {
		    // According to openNURBS's definition, the domain of m_curve3d,
		    // m_curveA, m_curveB in an ON_SSX_EVENT should be the same.
		    // (See ON_SSX_EVENT::IsValid()).
		    // So we don't need to pull the interval back to A
		    intervals2.Append(interval);
		}
	    } catch (InvalidGeometry &e) {
		bu_log("%s", e.what());
	    }
	}
    } else {
	intervals2.Append(event->m_curveB->Domain());    // No intersection
    }

    // 3. Merge the intervals and get the final result.
    try {
	ON_Interval merged_interval1 = union_intervals(intervals1);
	ON_Interval merged_interval2 = union_intervals(intervals2);
	ON_Interval shared_interval = intersect_intervals(merged_interval1, merged_interval2);

	if (DEBUG_BREP_BOOLEAN) {
	    bu_log("shared_interval: [%g, %g]\n", shared_interval.Min(), shared_interval.Max());
	}

	// 4. Replace with the sub-curves.
	replace_curve_with_subcurve(event->m_curve3d, shared_interval);
	replace_curve_with_subcurve(event->m_curveA, shared_interval);
	replace_curve_with_subcurve(event->m_curveB, shared_interval);
    } catch (IntervalGenerationError &e) {
	bu_log("%s", e.what());
	return -1;
    } catch (GeometryGenerationError &e) {
	bu_log("%s", e.what());
	return -1;
    }
    return 0;
}


HIDDEN ON_ClassArray<LinkedCurve>
link_curves(const ON_SimpleArray<SSICurve> &in)
{
    // There might be two reasons why we need to link these curves.
    // 1) They are from intersections with two different surfaces.
    // 2) They are not continuous in the other surface's UV domain.

    ON_ClassArray<LinkedCurve> tmp;
    for (int i = 0; i < in.Count(); i++) {
	LinkedCurve linked;
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
	    double dis;
	    // Link curves that share an end point.
	    if ((dis = tmp[i].PointAtStart().DistanceTo(tmp[j].PointAtEnd())) < INTERSECTION_TOL) {
		// end -- start -- end -- start
		c1 = &tmp[j];
		c2 = &tmp[i];
	    } else if ((dis = tmp[i].PointAtEnd().DistanceTo(tmp[j].PointAtStart())) < INTERSECTION_TOL) {
		// start -- end -- start -- end
		c1 = &tmp[i];
		c2 = &tmp[j];
	    } else if ((dis = tmp[i].PointAtStart().DistanceTo(tmp[j].PointAtStart())) < INTERSECTION_TOL) {
		// end -- start -- start -- end
		if (tmp[i].Reverse()) {
		    c1 = &tmp[i];
		    c2 = &tmp[j];
		}
	    } else if ((dis = tmp[i].PointAtEnd().DistanceTo(tmp[j].PointAtEnd())) < INTERSECTION_TOL) {
		// start -- end -- end -- start
		if (tmp[j].Reverse()) {
		    c1 = &tmp[i];
		    c2 = &tmp[j];
		}
	    } else {
		continue;
	    }

	    if (c1 != NULL && c2 != NULL) {
		LinkedCurve new_curve;
		new_curve.Append(*c1);
		if (dis > ON_ZERO_TOLERANCE) {
		    new_curve.Append(SSICurve(new ON_LineCurve(c1->PointAtEnd(), c2->PointAtStart())));
		}
		new_curve.Append(*c2);
		tmp[i] = new_curve;
		tmp[j].m_ssi_curves.Empty();
	    }

	    // Check whether tmp[i] is closed within a tolerance
	    if (tmp[i].PointAtStart().DistanceTo(tmp[i].PointAtEnd()) < INTERSECTION_TOL && !tmp[i].IsClosed()) {
		// make IsClosed() true
		tmp[i].Append(SSICurve(new ON_LineCurve(tmp[i].PointAtEnd(), tmp[i].PointAtStart())));
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
split_trimmed_face(const TrimmedFace *in, ON_ClassArray<LinkedCurve> &curves)
{
    /* We followed the algorithms described in:
     * S. Krishnan, A. Narkhede, and D. Manocha. BOOLE: A System to Compute
     * Boolean Combinations of Sculptured Solids. Technical Report TR95-008,
     * Department of Computer Science, University of North Carolina, 1995.
     * Appendix B: Partitioning a Simple Polygon using Non-Intersecting
     * Chains.
     */
    ON_SimpleArray<TrimmedFace *> out;
    if (curves.Count() == 0) {
	// No curve, no splitting
	out.Append(in->Duplicate());
	return out;
    }

    // Get the intersection points between the SSI curves and the outerloop.
    ON_SimpleArray<IntersectPoint> intersect;
    ON_SimpleArray<bool> have_intersect(curves.Count());
    for (int i = 0; i < curves.Count(); i++) {
	have_intersect[i] = false;
    }

    for (int i = 0; i < in->m_outerloop.Count(); i++) {
	for (int j = 0; j < curves.Count(); j++) {
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(in->m_outerloop[i], curves[j].Curve(), x_event, INTERSECTION_TOL);
	    for (int k = 0; k < x_event.Count(); k++) {
		IntersectPoint tmp_pt;
		tmp_pt.m_pt = x_event[k].m_A[0];
		tmp_pt.m_seg = i;
		tmp_pt.m_t = x_event[k].m_a[0];
		tmp_pt.m_type = j;
		tmp_pt.m_t_for_rank = x_event[k].m_b[0];
		intersect.Append(tmp_pt);
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap) {
		    tmp_pt.m_pt = x_event[k].m_A[1];
		    // tmp_pt.m_seg = i;
		    tmp_pt.m_t = x_event[k].m_a[1];
		    // tmp_pt.m_type = j;
		    tmp_pt.m_t_for_rank = x_event[k].m_b[1];
		    intersect.Append(tmp_pt);
		}
		if (x_event.Count()) {
		    have_intersect[j] = true;
		}
	    }
	}
    }

    // deal with the situations where there is no intersection
    std::vector<ON_SimpleArray<ON_Curve *> > innerloops;
    for (int i = 0; i < curves.Count(); i++) {
	if (!have_intersect[i]) {
	    // The start point cannot be on the boundary of the loop, because
	    // there is no intersections between curves[i] and the loop.
	    try {
		if (point_loop_location(curves[i].PointAtStart(), in->m_outerloop) == INSIDE_OR_ON_LOOP) {
		    if (curves[i].IsClosed()) {
			ON_SimpleArray<ON_Curve *> index_loop;
			curves[i].AppendCurvesToArray(index_loop);
			innerloops.push_back(index_loop);
			TrimmedFace *new_face = new TrimmedFace();
			new_face->m_face = in->m_face;
			curves[i].AppendCurvesToArray(new_face->m_outerloop);
			out.Append(new_face);
		    }
		}
	    } catch (InvalidGeometry &e) {
		bu_log("%s", e.what());
	    }
	}
    }

    // rank these intersection points
    // during the time using pts_on_curves(), must make sure that
    // the capacity of intersect[] never change.
    ON_ClassArray<ON_SimpleArray<IntersectPoint *> > pts_on_curves(curves.Count());
    for (int i = 0; i < intersect.Count(); i++) {
	pts_on_curves[intersect[i].m_type].Append(&(intersect[i]));
    }
    for (int i = 0; i < curves.Count(); i++) {
	pts_on_curves[i].QuickSort(compare_for_rank);
	for (int j = 0; j < pts_on_curves[i].Count(); j++) {
	    pts_on_curves[i][j]->m_rank = j;
	}
    }

    ON_SimpleArray<IntersectPoint> intersect_append;

    // Determine whether it's going inside or outside (IntersectPoint::m_in_out).
    for (int i = 0; i < curves.Count(); i++) {
	for (int j = 0;  j < pts_on_curves[i].Count(); j++) {
	    IntersectPoint *ipt = pts_on_curves[i][j];
	    if (pts_on_curves[i].Count() < 2) {
		ipt->m_in_out = IntersectPoint::UNSET;
	    } else {
		ON_3dPoint left = j == 0 ? curves[i].PointAtStart() :
				  curves[i].PointAt((ipt->m_t_for_rank + pts_on_curves[i][j - 1]->m_t_for_rank) * 0.5);
		ON_3dPoint right = j == pts_on_curves[i].Count() - 1 ? curves[i].PointAtEnd() :
				   curves[i].PointAt((ipt->m_t_for_rank + pts_on_curves[i][j + 1]->m_t_for_rank) * 0.5);
		// If the point is on the boundary, we treat it with the same
		// way as it's outside.
		// For example, the left side is inside, and the right's on
		// boundary, that point should be IntersectPoint::OUT, the
		// same as the right's outside the loop.
		// Other cases are similar.
		int left_in, right_in;
		try {
		    left_in = is_point_inside_loop(left, in->m_outerloop);
		    right_in = is_point_inside_loop(right, in->m_outerloop);
		} catch (InvalidGeometry &e) {
		    bu_log("%s", e.what());
		    // not a loop
		    ipt->m_in_out = IntersectPoint::UNSET;
		    continue;
		}
		if (j == 0 && ON_NearZero(ipt->m_t_for_rank - curves[i].Domain().Min())) {
		    ipt->m_in_out = right_in ? IntersectPoint::IN : IntersectPoint::OUT;
		} else if (j == pts_on_curves[i].Count() - 1 && ON_NearZero(ipt->m_t_for_rank - curves[i].Domain().Max())) {
		    ipt->m_in_out = left_in ? IntersectPoint::OUT : IntersectPoint::IN;
		} else {
		    if (left_in && right_in) {
			// tangent point, both sides in, duplicate that point
			intersect_append.Append(*ipt);
			intersect_append.Last()->m_in_out = IntersectPoint::IN;
			intersect_append.Last()->m_rank = ipt->m_rank + 1;
			for (int k = j + 1; k < pts_on_curves[i].Count(); k++) {
			    pts_on_curves[i][k]->m_rank++;
			}
			ipt->m_in_out = IntersectPoint::OUT;
		    } else if (!left_in && !right_in) {
			// tangent point, both sides out, useless
			ipt->m_in_out = IntersectPoint::UNSET;
		    } else if (left_in && !right_in) {
			// transversal point, going outside
			ipt->m_in_out = IntersectPoint::OUT;
		    } else {
			// transversal point, going inside
			ipt->m_in_out = IntersectPoint::IN;
		    }
		}
	    }
	}
    }

    intersect.Append(intersect_append.Count(), intersect_append.Array());
    intersect.QuickSort(compare_t);

    // Split the outer loop.
    ON_SimpleArray<ON_Curve *> outerloop;   // segments of the outerloop
    int isect_iter = 0;
    for (int i = 0; i < in->m_outerloop.Count(); i++) {
	ON_Curve *curve_on_loop = in->m_outerloop[i]->Duplicate();
	if (curve_on_loop == NULL) {
	    bu_log("ON_Curve::Duplicate() failed.\n");
	    continue;
	}
	for (; isect_iter < intersect.Count() && intersect[isect_iter].m_seg == i; isect_iter++) {
	    const IntersectPoint &isect_pt = intersect[isect_iter];
	    ON_Curve *left = NULL;
	    bool split_called = false;
	    if (curve_on_loop) {
		if (ON_NearZero(isect_pt.m_t - curve_on_loop->Domain().Max())) {
		    // Don't call Split(), which may fail when the point is
		    // at the ends.
		    left = curve_on_loop;
		    curve_on_loop = NULL;
		} else if (!ON_NearZero(isect_pt.m_t - curve_on_loop->Domain().Min())) {
		    curve_on_loop->Split(isect_pt.m_t, left, curve_on_loop);
		    split_called = true;
		}
	    }
	    if (left != NULL) {
		outerloop.Append(left);
	    } else if (split_called) {
		bu_log("Split failed.\n");
		if (curve_on_loop) {
		    bu_log("Domain: [%f, %f]\n", curve_on_loop->Domain().Min(), curve_on_loop->Domain().Max());
		    bu_log("m_t: %f\n", isect_pt.m_t);
		}
	    }
	    intersect[isect_iter].m_pos = outerloop.Count() - 1;
	}
	if (curve_on_loop) {
	    outerloop.Append(curve_on_loop);
	}
    }

    // Append the first element at the last to handle some special cases.
    if (intersect.Count()) {
	intersect.Append(intersect[0]);
	intersect.Last()->m_seg += in->m_outerloop.Count();
	for (int i = 0; i <= intersect[0].m_pos; i++) {
	    ON_Curve *dup = outerloop[i]->Duplicate();
	    if (dup != NULL) {
		outerloop.Append(dup);
	    } else {
		bu_log("ON_Curve::Duplicate() failed.\n");
		outerloop.Append(NULL);
	    }
	}
	intersect.Last()->m_pos = outerloop.Count() - 1;
    }

    if (DEBUG_BREP_BOOLEAN)
	for (int i = 0; i < intersect.Count(); i++) {
	    bu_log("intersect[%d](count = %d): m_type = %d, m_rank = %d, m_in_out = %d\n", i, intersect.Count(), intersect[i].m_type, intersect[i].m_rank, intersect[i].m_in_out);
	}

    std::stack<int> s;

    for (int i = 0; i < intersect.Count(); i++) {
	// Ignore UNSET IntersectPoints.
	if (intersect[i].m_in_out == IntersectPoint::UNSET) {
	    continue;
	}

	if (s.empty()) {
	    s.push(i);
	    continue;
	}

	const IntersectPoint &p = intersect[s.top()];
	const IntersectPoint &q = intersect[i];

	if (compare_t(&p, &q) > 0 || q.m_pos < p.m_pos) {
	    bu_log("stack error or sort failure.\n");
	    bu_log("s.top() = %d, i = %d\n", s.top(), i);
	    bu_log("p->m_pos = %d, q->m_pos = %d\n", p.m_pos, q.m_pos);
	    bu_log("p->m_seg = %d, q->m_seg = %d\n", p.m_seg, q.m_seg);
	    bu_log("p->m_t = %g, q->m_t = %g\n", p.m_t, q.m_t);
	    continue;
	}
	if (q.m_type != p.m_type) {
	    s.push(i);
	    continue;
	} else if (q.m_rank - p.m_rank == 1 && q.m_in_out == IntersectPoint::OUT && p.m_in_out == IntersectPoint::IN) {
	    s.pop();
	} else if (p.m_rank - q.m_rank == 1 && p.m_in_out == IntersectPoint::OUT && q.m_in_out == IntersectPoint::IN) {
	    s.pop();
	} else {
	    s.push(i);
	    continue;
	}

	// need to form a new loop
	ON_SimpleArray<ON_Curve *> newloop;
	int curve_count = q.m_pos - p.m_pos;
	for (int j = p.m_pos + 1; j <= q.m_pos; j++) {
	    // No need to duplicate the curve, because the pointer
	    // in the array 'outerloop' will be moved out later.
	    newloop.Append(outerloop[j]);
	}

	if (p.m_type != q.m_type) {
	    bu_log("Error: p->type != q->type\n");
	    continue;
	}

	// The curves on the outer loop is from p to q, so the curves on the
	// SSI curve should be from q to p (to form a loop)
	double t1 = p.m_t_for_rank, t2 = q.m_t_for_rank;
	bool need_reverse = true;
	if (t1 > t2) {
	    std::swap(t1, t2);
	    need_reverse = false;
	}
	ON_Curve *seg_on_SSI = curves[p.m_type].SubCurve(t1, t2);
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

	ON_Curve *rev_seg_on_SSI = seg_on_SSI->Duplicate();
	if (!rev_seg_on_SSI || !rev_seg_on_SSI->Reverse()) {
	    bu_log("Reverse failed.\n");
	    continue;
	} else {
	    // Update the outerloop
	    outerloop[p.m_pos + 1] = rev_seg_on_SSI;
	    int k = p.m_pos + 2;
	    for (int j = q.m_pos + 1; j < outerloop.Count(); j++) {
		outerloop[k++] = outerloop[j];
	    }
	    while (k < outerloop.Count()) {
		outerloop[outerloop.Count() - 1] = NULL;
		outerloop.Remove();
	    }
	    // Update m_pos
	    for (int j = i + 1; j < intersect.Count(); j++) {
		intersect[j].m_pos -= curve_count - 1;
	    }
	}

	// Append a trimmed face with newloop as its outerloop
	// Don't add a face if the outerloop is not valid (e.g. degenerated).
	if (is_loop_valid(newloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *new_face = new TrimmedFace();
	    new_face->m_face = in->m_face;
	    new_face->m_outerloop.Append(newloop.Count(), newloop.Array());
	    out.Append(new_face);
	} else {
	    for (int j = 0; j < newloop.Count(); j++) {
		delete newloop[j];
	    }
	}
    }

    // Remove the duplicated segments before the first intersection point.
    if (intersect.Count()) {
	for (int i = 0; i <= intersect[0].m_pos; i++) {
	    delete outerloop[0];
	    outerloop[0] = NULL;
	    outerloop.Remove(0);
	}
    }

    if (out.Count() == 0) {
	out.Append(in->Duplicate());
    } else {
	// The remaining part after splitting some parts out.
	if (is_loop_valid(outerloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *new_face = new TrimmedFace();
	    new_face->m_face = in->m_face;
	    new_face->m_outerloop = outerloop;
	    // First copy the array with pointers, and then change
	    // the pointers into copies.
	    new_face->m_innerloop = in->m_innerloop;
	    for (unsigned int i = 0; i < in->m_innerloop.size(); i++)
		for (int j = 0; j < in->m_innerloop[i].Count(); j++)
		    if (in->m_innerloop[i][j]) {
			new_face->m_innerloop[i][j] = in->m_innerloop[i][j]->Duplicate();
		    }
	    new_face->m_innerloop.insert(new_face->m_innerloop.end(), innerloops.begin(), innerloops.end());
	    out.Append(new_face);
	} else {
	    for (int i = 0; i < outerloop.Count(); i++)
		if (outerloop[i]) {
		    delete outerloop[i];
		}
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


HIDDEN int
is_face_inside_brep(const TrimmedFace *tface, const ON_Brep *brep, ON_SimpleArray<Subsurface *> &surf_tree)
{
    // returns:
    //  -1: whether the face is inside/outside is unknown
    //   0: the face is outside the brep
    //   1: the face is inside the brep
    //   2: the face is (completely) on the brep's surface
    //      (because the overlap parts will be split out as separated trimmed
    //       faces, if one point on a trimmed face is on the brep's surface,
    //       the whole trimmed face should be on the surface)

    if (tface == NULL || brep == NULL) {
	return -1;
    }

    const ON_BrepFace *bface = tface->m_face;
    if (bface == NULL) {
	return -1;
    }

    ON_BoundingBox brep2box = brep->BoundingBox();
    brep2box.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    brep2box.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bface->BoundingBox().Intersection(brep2box)) {
	return 0;
    }

    if (tface->m_outerloop.Count() == 0) {
	bu_log("is_face_inside_brep(): the input TrimmedFace is not trimmed.\n");
	return -1;
    }

    ON_PolyCurve polycurve;
    if (!is_loop_valid(tface->m_outerloop, ON_ZERO_TOLERANCE, &polycurve)) {
	return -1;
    }

    // Get a point inside the TrimmedFace, and then call is_point_inside_brep().
    // First, try the center of its 2D domain.
    const int try_count = 10;
    ON_BoundingBox bbox =  polycurve.BoundingBox();
    bool found = false;
    ON_2dPoint test_pt2d;
    ON_RandomNumberGenerator rng;

    try {
	for (int i = 0; i < try_count; i++) {
	    // Get a random point inside the outerloop's bounding box.
	    double x = rng.RandomDouble(bbox.m_min.x, bbox.m_max.x);
	    double y = rng.RandomDouble(bbox.m_min.y, bbox.m_max.y);
	    test_pt2d = ON_2dPoint(x, y);
	    if (is_point_inside_loop(test_pt2d, tface->m_outerloop)) {
		unsigned int j = 0;
		// The test point should not be inside an innerloop
		for (j = 0; j < tface->m_innerloop.size(); j++) {
		    try {
			if (!is_point_outside_loop(test_pt2d, tface->m_innerloop[j])) {
			    break;
			}
		    } catch (InvalidGeometry &e) {
			bu_log("%s", e.what());
		    }
		}
		if (j == tface->m_innerloop.size()) {
		    // We find a valid test point
		    found = true;
		    break;
		}
	    }
	}
    } catch (InvalidGeometry &e) {
	bu_log("%s", e.what());
    }

    if (!found) {
	bu_log("Cannot find a point inside this trimmed face. Aborted.\n");
	return -1;
    }

    ON_3dPoint test_pt3d = tface->m_face->PointAt(test_pt2d.x, test_pt2d.y);
    if (DEBUG_BREP_BOOLEAN) {
	bu_log("valid test point: (%g, %g, %g)\n", test_pt3d.x, test_pt3d.y, test_pt3d.z);
    }

    if (is_point_on_brep_surface(test_pt3d, brep, surf_tree)) {
	return 2;
    }

    return is_point_inside_brep(test_pt3d, brep, surf_tree) ? 1 : 0;
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
		ON_SimpleArray<ON_Curve *> curve_uv, curve_st;
		for (int k = 0; k < events.Count(); k++) {
		    if (events[k].m_type == ON_SSX_EVENT::ssx_tangent
			|| events[k].m_type == ON_SSX_EVENT::ssx_transverse) {
			if (get_subcurve_inside_faces(brep1, brep2, i, j, &events[k]) < 0) {
			    continue;
			}
			SSICurve c1, c2;
			c1.m_curve = events[k].m_curveA;
			c2.m_curve = events[k].m_curveB;
			curves_array[i].Append(c1);
			curves_array[face_count1 + j].Append(c2);
			curves_array.SetCount(curves_array.Count() + 2);
			// Set m_curveA and m_curveB to NULL, in case that they are
			// deleted by ~ON_SSX_EVENT().
			events[k].m_curveA = events[k].m_curveB = NULL;
		    }
		}

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

HIDDEN ON_ClassArray<ON_SimpleArray<TrimmedFace *> >
get_evaluated_faces(const ON_Brep *brep1, const ON_Brep *brep2, op_type operation)
{
    ON_SimpleArray<Subsurface *> surf_tree1, surf_tree2;

    ON_ClassArray<ON_SimpleArray<SSICurve> > curves_array =
	get_face_intersection_curves(surf_tree1, surf_tree2, brep1, brep2, operation);

    int face_count1 = brep1->m_F.Count();
    int face_count2 = brep2->m_F.Count();

    ON_SimpleArray<TrimmedFace *> original_faces;
    for (int i = 0; i < face_count1 + face_count2; i++) {
	const ON_BrepFace &face = i < face_count1 ? brep1->m_F[i] : brep2->m_F[i - face_count1];
	const ON_Brep *brep = i < face_count1 ? brep1 : brep2;
	const ON_SimpleArray<int> &loop_index = face.m_li;

	TrimmedFace *first = new TrimmedFace();
	first->m_face = &face;

	for (int j = 0; j < loop_index.Count(); j++) {
	    const ON_BrepLoop &loop = brep->m_L[loop_index[j]];
	    const ON_SimpleArray<int> &trim_index = loop.m_ti;
	    ON_SimpleArray<ON_Curve *> index_loop;
	    for (int k = 0; k < trim_index.Count(); k++) {
		ON_Curve *curve2d = brep->m_C2[brep->m_T[trim_index[k]].m_c2i];
		if (j == 0) {
		    first->m_outerloop.Append(curve2d->Duplicate());
		} else {
		    index_loop.Append(curve2d->Duplicate());
		}
	    }
	    if (j != 0) {
		first->m_innerloop.push_back(index_loop);
	    }
	}
	original_faces.Append(first);
    }

    if (original_faces.Count() != face_count1 + face_count2) {
	throw GeometryGenerationError("ON_Boolean() Error: TrimmedFace"
				      " generation failed.\n");
    }

    // split the surfaces with the intersection curves
    ON_ClassArray<ON_SimpleArray<TrimmedFace *> > trimmed_faces;
    for (int i = 0; i < original_faces.Count(); i++) {
	TrimmedFace *first = original_faces[i];
	ON_ClassArray<LinkedCurve> linked_curves = link_curves(curves_array[i]);

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
	    splitted[j]->m_belong_to_final = TrimmedFace::NOT_BELONG;
	    splitted[j]->m_rev = false;
	    int ret_inside_test = is_face_inside_brep(splitted[j], another_brep, surf_tree);
	    if (ret_inside_test == 1) {
		if (DEBUG_BREP_BOOLEAN) {
		    bu_log("The trimmed face is inside the other brep.\n");
		}
		if (operation == BOOLEAN_INTERSECT || operation == BOOLEAN_XOR || (operation == BOOLEAN_DIFF && i >= face_count1)) {
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		}
		if (operation == BOOLEAN_DIFF || operation == BOOLEAN_XOR) {
		    splitted[j]->m_rev = true;
		}
	    } else if (ret_inside_test == 0) {
		if (DEBUG_BREP_BOOLEAN) {
		    bu_log("The trimmed face is outside the other brep.\n");
		}
		if (operation == BOOLEAN_UNION || operation == BOOLEAN_XOR || (operation == BOOLEAN_DIFF && i < face_count1)) {
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		}
	    } else if (ret_inside_test == 2) {
		if (DEBUG_BREP_BOOLEAN) {
		    bu_log("The trimmed face is on the other brep's surfaces.\n");
		}
		if (operation == BOOLEAN_UNION || operation == BOOLEAN_INTERSECT) {
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		}
		// TODO: Actually only one of them is needed in the final brep structure
	    } else {
		if (DEBUG_BREP_BOOLEAN) {
		    bu_log("Whether the trimmed face is inside/outside is unknown.\n");
		}
		splitted[j]->m_belong_to_final = TrimmedFace::UNKNOWN;
	    }
	}
    }
    for (int i = 0; i < surf_tree1.Count(); i++) {
	delete surf_tree1[i];
    }

    for (int i = 0; i < surf_tree2.Count(); i++) {
	delete surf_tree2[i];
    }

    return trimmed_faces;
}

int
ON_Boolean(ON_Brep *evaluated_brep, const ON_Brep *brep1, const ON_Brep *brep2, op_type operation)
{
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
	    return 0;
	}
	trimmed_faces = get_evaluated_faces(brep1, brep2, operation);
    } catch (InvalidBooleanOperation &e) {
	bu_log("%s", e.what());
	return -1;
    } catch (GeometryGenerationError &e) {
	bu_log("%s", e.what());
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

    // Check IsValid() and output the message.
    ON_wString ws;
    ON_TextLog log(ws);
    evaluated_brep->IsValid(&log);
    bu_log(ON_String(ws).Array());

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
