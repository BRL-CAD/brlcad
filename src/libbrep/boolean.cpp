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
#include "bio.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"
#include "raytrace.h"

// Whether to output the debug messages about b-rep booleans.
#define DEBUG_BREP_BOOLEAN 0

// Using connectivity graphs can reduce the number of inside/outside tests,
// which is considered time-consuming in some research papers. But in practice
// we find that the performance of inside/outside tests is not so critical,
// and the implementation of connectivity graphs is still in beta (some cases
// with SSI overlaps seem to be wrong), so we mark this flag as 0
#define USE_CONNECTIVITY_GRAPH 0

// tol value used in ON_Intersect()s. We use a smaller tolerance than the
// default one 0.001.
#define INTERSECTION_TOL 1e-4

// tol value used in ON_3dVector::IsParallelTo(). We use a smaller tolerance
// than the default one ON_PI/180.
#define ANGLE_TOL ON_PI/1800.0


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


#if USE_CONNECTIVITY_GRAPH
struct SSICurveInfo {
    int m_fi1;	// index of the this face
    int m_fi2;	// index of the the other face
    int m_ci;	// index in the events array

    // Default constructor. Set all to an invalid value.
    SSICurveInfo()
    {
	m_fi1 = m_fi2 = m_ci = -1;
    }
};
#endif


// A structure to represent the curve segments generated from surface-surface
// intersections, including some information needed by the connectivity graph
struct SSICurve {
    ON_Curve* m_curve;
#if USE_CONNECTIVITY_GRAPH
    SSICurveInfo m_info;
#endif

    SSICurve()
    {
	m_curve = NULL;
    }

    SSICurve(ON_Curve* curve)
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


void AppendToPolyCurve(ON_Curve* curve, ON_PolyCurve& polycurve);
// We link the SSICurves that share an endpoint, and form this new structure,
// which has many similar behaviors as ON_Curve, e.g. PointAt(), Reverse().
struct LinkedCurve {
private:
    ON_Curve* m_curve;	// an explicit storage of the whole curve
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
	if (m_curve)
	    delete m_curve;
	m_curve = NULL;
    }

    LinkedCurve& operator= (const LinkedCurve& _lc)
    {
	m_curve = _lc.m_curve ? _lc.m_curve->Duplicate() : NULL;
	m_ssi_curves = _lc.m_ssi_curves;
	return *this;
    }

    ON_3dPoint PointAtStart() const
    {
	if (m_ssi_curves.Count())
	    return m_ssi_curves[0].m_curve->PointAtStart();
	else
	    return ON_3dPoint::UnsetPoint;
    }

    ON_3dPoint PointAtEnd() const
    {
	if (m_ssi_curves.Count())
	    return m_ssi_curves.Last()->m_curve->PointAtEnd();
	else
	    return ON_3dPoint::UnsetPoint;
    }

    bool IsClosed() const
    {
	if (m_ssi_curves.Count() == 0)
	    return false;
	return PointAtStart().DistanceTo(PointAtEnd()) < ON_ZERO_TOLERANCE;
    }

    bool IsValid() const
    {
	// Check whether the curve has "gaps".
	for (int i = 1; i < m_ssi_curves.Count(); i++) {
	    if (m_ssi_curves[i].m_curve->PointAtStart().DistanceTo(m_ssi_curves[i-1].m_curve->PointAtEnd()) >= ON_ZERO_TOLERANCE) {
		bu_log("The LinkedCurve is not valid.\n");
		return false;
	    }
	}
	return true;
    }

    bool Reverse()
    {
	ON_SimpleArray<SSICurve> newarray;
	for (int i = m_ssi_curves.Count() - 1; i >= 0; i--) {
	    if (!m_ssi_curves[i].m_curve->Reverse())
		return false;
	    newarray.Append(m_ssi_curves[i]);
	}
	m_ssi_curves = newarray;
	return true;
    }

    void Append(const LinkedCurve& lc)
    {
	m_ssi_curves.Append(lc.m_ssi_curves.Count(), lc.m_ssi_curves.Array());
    }

    void Append(const SSICurve& sc)
    {
	m_ssi_curves.Append(sc);
    }

    void AppendCurvesToArray(ON_SimpleArray<ON_Curve*>& arr) const
    {
	for (int i = 0; i < m_ssi_curves.Count(); i++)
	    arr.Append(m_ssi_curves[i].m_curve->Duplicate());
    }

#if USE_CONNECTIVITY_GRAPH
    void AppendSSIInfoToArray(ON_SimpleArray<SSICurveInfo>& arr) const
    {
	for (int i = 0; i < m_ssi_curves.Count(); i++)
	    arr.Append(m_ssi_curves[i].m_info);
    }
#endif

    const ON_Curve* Curve()
    {
	if (m_curve != NULL)
	    return m_curve;
	if (m_ssi_curves.Count() == 0 || !IsValid())
	    return NULL;
	ON_PolyCurve* polycurve = new ON_PolyCurve;
	for (int i = 0; i < m_ssi_curves.Count(); i++)
	    AppendToPolyCurve(m_ssi_curves[i].m_curve->Duplicate(), *polycurve);
	m_curve = polycurve;
	return m_curve;
    }

    const ON_3dPoint PointAt(double t)
    {
	const ON_Curve* c = Curve();
	if (c == NULL)
	    return ON_3dPoint::UnsetPoint;
	return c->PointAt(t);
    }

    const ON_Interval Domain()
    {
	const ON_Curve* c = Curve();
	if (c == NULL)
	    return ON_Interval::EmptyInterval;
	return c->Domain();
    }

    ON_Curve* SubCurve(double t1, double t2)
    {
	const ON_Curve* c = Curve();
	if (c == NULL)
	    return NULL;
	return sub_curve(c, t1, t2);
    }
};


struct TrimmedFace {
    // curve segments in the face's outer loop
    ON_SimpleArray<ON_Curve*> m_outerloop;
    // several inner loops, each has some curves
    std::vector<ON_SimpleArray<ON_Curve*> > m_innerloop;
    const ON_BrepFace *m_face;
    enum {
	UNKNOWN = -1,
	NOT_BELONG = 0,
	BELONG = 1
    } m_belong_to_final;
    bool m_rev;
#if USE_CONNECTIVITY_GRAPH
    // Connectivity graph support
    ON_SimpleArray<TrimmedFace*> m_neighbors;
    // which parts of its parent's outerloop are used, described in multiple
    // pairs of IntersectPoints (multiple intervals)
    ON_ClassArray<std::pair<IntersectPoint, IntersectPoint> > m_parts;
    // which SSI curves are used.
    ON_SimpleArray<SSICurveInfo> m_ssi_info;
#endif

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
	for (int i = 0; i < m_outerloop.Count(); i++)
	    if (m_outerloop[i])
		out->m_outerloop.Append(m_outerloop[i]->Duplicate());
	out->m_innerloop = m_innerloop;
	for (unsigned int i = 0; i < m_innerloop.size(); i++)
	    for (int j = 0; j < m_innerloop[i].Count(); j++)
		if (m_innerloop[i][j])
		    out->m_innerloop[i][j] = m_innerloop[i][j]->Duplicate();
#if USE_CONNECTIVITY_GRAPH
	out->m_parts = m_parts;
	out->m_ssi_info = m_ssi_info;
	// Don't copy the neighbors
#endif
	return out;
    }
};

HIDDEN int
compare_t(const IntersectPoint* a, const IntersectPoint* b)
{
    // Use for sorting an array. Use strict FP comparison.
    if (a->m_seg != b->m_seg)
	return a->m_seg - b->m_seg;
    return a->m_t > b->m_t ? 1 : (a->m_t < b->m_t ? -1 : 0);
}


HIDDEN int
compare_for_rank(IntersectPoint* const *a, IntersectPoint* const *b)
{
    // Use for sorting an array. Use strict FP comparison.
    return (*a)->m_t_for_rank > (*b)->m_t_for_rank ? 1 : ((*a)->m_t_for_rank < (*b)->m_t_for_rank ? -1 : 0);
}


HIDDEN void
AppendToPolyCurve(ON_Curve* curve, ON_PolyCurve& polycurve)
{
    // use this function rather than ON_PolyCurve::Append() to avoid
    // getting nested polycurves, which makes ON_Brep::IsValid() to fail.

    ON_PolyCurve* nested = ON_PolyCurve::Cast(curve);
    if (nested != NULL) {
	// The input curve is a polycurve
	const ON_CurveArray& segments = nested->SegmentCurves();
	for (int i = 0; i < segments.Count(); i++)
	    AppendToPolyCurve(segments[i]->Duplicate(), polycurve);
	delete nested;
    } else
	polycurve.Append(curve);
}


HIDDEN bool
IsLoopValid(const ON_SimpleArray<ON_Curve*>& loop, double tolerance, ON_PolyCurve* polycurve = NULL)
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
	    if (polycurve)
		delete_curve = true;
	    else
		ret = false;
	}
    }

    // Check the loop is continuous and closed or not.
    if (ret) {
	if (loop[0] != NULL)
	    AppendToPolyCurve(loop[0]->Duplicate(), *polycurve);
	for (int i = 1 ; i < loop.Count(); i++) {
	    if (loop[i] && loop[i - 1] && loop[i]->PointAtStart().DistanceTo(loop[i-1]->PointAtEnd()) < ON_ZERO_TOLERANCE)
		AppendToPolyCurve(loop[i]->Duplicate(), *polycurve);
	    else {
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

    if (delete_curve)
	delete polycurve;

    return ret;
}


HIDDEN int
IsPointInsideLoop(const ON_2dPoint& pt, const ON_SimpleArray<ON_Curve*>& loop)
{
    // returns:
    //   -1: the input is not a valid loop
    //   0:  the point is not inside the loop or on boundary
    //   1:  the point is inside the loop or on boundary
    // note:
    //   If you want to know whether this point is on boundary, please call
    //   IsPointOnLoop().

    ON_PolyCurve polycurve;
    if (!IsLoopValid(loop, ON_ZERO_TOLERANCE, &polycurve))
	return -1;

    ON_BoundingBox bbox = polycurve.BoundingBox();
    if (!bbox.IsPointIn(pt))
	return 0;

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
	if (polycurve.TangentAt(x_event[i].m_b[0]).IsParallelTo(linecurve.m_line.Direction(), ANGLE_TOL))
	    count++;
    }

    return count % 2 ? 1 : 0;
}


HIDDEN int
IsPointOnLoop(const ON_2dPoint& pt, const ON_SimpleArray<ON_Curve*>& loop)
{
    // returns:
    //   -1: the input is not a valid loop
    //   0:  the point is not on the boundary of the loop
    //   1:  the point is on the boundary of the loop

    ON_PolyCurve polycurve;
    if (!IsLoopValid(loop, ON_ZERO_TOLERANCE, &polycurve))
	return -1;

    ON_ClassArray<ON_PX_EVENT> px_event;
    return ON_Intersect(ON_3dPoint(pt), polycurve, px_event, INTERSECTION_TOL) ? 1 : 0;
}


HIDDEN int
get_subcurve_inside_faces(const ON_Brep* brep1, const ON_Brep* brep2, int fi1, int fi2, ON_SSX_EVENT* Event)
{
    // The ON_SSX_EVENT from SSI is the intersection of two whole surfaces.
    // We need to get the part that lies inside both trimmed patches.
    // (brep1's face[fi1] and brep2's face[fi2])
    // returns 0 for success, -1 for error.

    if (Event == NULL)
	return -1;

    if (Event->m_curve3d == NULL || Event->m_curveA == NULL || Event->m_curveB == NULL)
	return -1;

    // 1. Get the outerloops.
    ON_SimpleArray<ON_Curve*> outerloop1, outerloop2;
    if (fi1 < 0 || fi1 >= brep1->m_F.Count()) {
	bu_log("get_subcurve_inside_faces(): invalid fi1 (%d).\n", fi1);
	return -1;
    }
    if (fi2 < 0 || fi2 >= brep2->m_F.Count()) {
	bu_log("get_subcurve_inside_faces(): invalid fi2 (%d).\n", fi2);
	return -1;
    }
    const ON_BrepLoop& loop1 = brep1->m_L[brep1->m_F[fi1].m_li[0]];
    const ON_BrepLoop& loop2 = brep2->m_L[brep2->m_F[fi2].m_li[0]];

    for (int i = 0; i < loop1.TrimCount(); i++) {
	outerloop1.Append(brep1->m_C2[brep1->m_T[loop1.m_ti[i]].m_c2i]);
    }
    for (int i = 0; i < loop2.TrimCount(); i++) {
	outerloop2.Append(brep2->m_C2[brep2->m_T[loop2.m_ti[i]].m_c2i]);
    }

    // 2.1 Intersect the curves in Event with outerloop1, and get the parts
    // inside. (Represented with param intervals on the curve's domain [0, 1])
    ON_SimpleArray<double> divT;
    ON_SimpleArray<ON_Interval> intervals1, intervals2;
    ON_SimpleArray<ON_X_EVENT> x_event1, x_event2;
    for (int i = 0; i < outerloop1.Count(); i++)
	ON_Intersect(Event->m_curveA, outerloop1[i], x_event1, INTERSECTION_TOL);
    for (int i = 0; i < x_event1.Count(); i++) {
	divT.Append(x_event1[i].m_a[0]);
	if (x_event1[i].m_type == ON_X_EVENT::ccx_overlap)
	    divT.Append(x_event1[i].m_a[1]);
    }
    divT.QuickSort(ON_CompareIncreasing);
    if (divT.Count() != 0) {
	if (!ON_NearZero(divT[0] - Event->m_curveA->Domain().Min()))
	    divT.Insert(0, Event->m_curveA->Domain().Min());
	if (!ON_NearZero(*divT.Last() - Event->m_curveA->Domain().Max()))
	    divT.Append(Event->m_curveA->Domain().Max());
	for (int i = 0; i < divT.Count() - 1; i++) {
	    ON_Interval interval(divT[i], divT[i + 1]);
	    if (ON_NearZero(interval.Length()))
		continue;
	    ON_2dPoint pt = Event->m_curveA->PointAt(interval.Mid());
	    if (IsPointOnLoop(pt, outerloop1) == 0 && IsPointInsideLoop(pt, outerloop1) == 1)
		intervals1.Append(interval);
	}
    } else
	intervals1.Append(Event->m_curveA->Domain());   // No intersection

    // 2.2 Intersect the curves in Event with outerloop2, and get the parts
    // inside. (Represented with param intervals on the curve's domain [0, 1])
    divT.Empty();
    for (int i = 0; i < outerloop2.Count(); i++)
	ON_Intersect(Event->m_curveB, outerloop2[i], x_event2, INTERSECTION_TOL);
    for (int i = 0; i < x_event2.Count(); i++) {
	divT.Append(x_event2[i].m_a[0]);
	if (x_event2[i].m_type == ON_X_EVENT::ccx_overlap)
	    divT.Append(x_event2[i].m_a[1]);
    }
    divT.QuickSort(ON_CompareIncreasing);
    if (divT.Count() != 0) {
	if (!ON_NearZero(divT[0] - Event->m_curveB->Domain().Min()))
	    divT.Insert(0, Event->m_curveB->Domain().Min());
	if (!ON_NearZero(*divT.Last() - Event->m_curveB->Domain().Max()))
	    divT.Append(Event->m_curveB->Domain().Max());
	for (int i = 0; i < divT.Count() - 1; i++) {
	    ON_Interval interval(divT[i], divT[i + 1]);
	    if (ON_NearZero(interval.Length()))
		continue;
	    ON_2dPoint pt = Event->m_curveB->PointAt(interval.Mid());
	    if (IsPointOnLoop(pt, outerloop2) == 0 && IsPointInsideLoop(pt, outerloop2) == 1) {
		// According to openNURBS's definition, the domain of m_curve3d,
		// m_curveA, m_curveB in an ON_SSX_EVENT should be the same.
		// (See ON_SSX_EVENT::IsValid()).
		// So we don't need to pull the interval back to A
		intervals2.Append(interval);
	    }
	}
    } else
	intervals2.Append(Event->m_curveB->Domain());   // No intersection

    // 3. Merge the intervals and get the final result.
    ON_Interval merged_interval1, merged_interval2;
    for (int i = 0; i < intervals1.Count(); i++) {
	merged_interval1.Union(intervals1[i]);
    }
    for (int i = 0; i < intervals2.Count(); i++) {
	merged_interval2.Union(intervals2[i]);
    }

    if (!merged_interval1.IsValid() || !merged_interval2.IsValid()) {
	return -1;
    }

    ON_Interval shared_interval;
    if (!shared_interval.Intersection(merged_interval1, merged_interval2))
	return -1;

    if (DEBUG_BREP_BOOLEAN)
	bu_log("shared_interval: [%g, %g]\n", shared_interval.Min(), shared_interval.Max());

    // 4. Replace with the sub-curves.
    ON_Curve *tmp_curve;
    tmp_curve = sub_curve(Event->m_curve3d, shared_interval.Min(), shared_interval.Max());
    delete Event->m_curve3d;
    Event->m_curve3d = tmp_curve;
    tmp_curve = sub_curve(Event->m_curveA, shared_interval.Min(), shared_interval.Max());
    delete Event->m_curveA;
    Event->m_curveA = tmp_curve;
    tmp_curve = sub_curve(Event->m_curveB, shared_interval.Min(), shared_interval.Max());
    delete Event->m_curveB;
    Event->m_curveB = tmp_curve;

    if (Event->m_curve3d == NULL || Event->m_curveA == NULL || Event->m_curveB == NULL)
	return -1;
    return 0;
}


HIDDEN void
link_curves(const ON_SimpleArray<SSICurve>& in, ON_ClassArray<LinkedCurve>& out)
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
	    if (tmp[i].m_ssi_curves.Count() == 0 || tmp[i].IsClosed())
		break;

	    if (tmp[j].m_ssi_curves.Count() == 0 || tmp[j].IsClosed() || j == i)
		continue;

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
	    } else
		continue;

	    if (c1 != NULL && c2 != NULL) {
		LinkedCurve newcurve;
		newcurve.Append(*c1);
		if (dis > ON_ZERO_TOLERANCE)
		    newcurve.Append(SSICurve(new ON_LineCurve(c1->PointAtEnd(), c2->PointAtStart())));
		newcurve.Append(*c2);
		tmp[i] = newcurve;
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
    for (int i = 0; i < tmp.Count(); i++)
	if (tmp[i].m_ssi_curves.Count() != 0)
	    out.Append(tmp[i]);

    if (DEBUG_BREP_BOOLEAN)
	bu_log("link_curves(): %d curves remaining.\n", out.Count());
}


HIDDEN int
split_trimmed_face(ON_SimpleArray<TrimmedFace*> &out, const TrimmedFace *in, ON_ClassArray<LinkedCurve> &curves)
{
    /* We followed the algorithms described in:
     * S. Krishnan, A. Narkhede, and D. Manocha. BOOLE: A System to Compute
     * Boolean Combinations of Sculptured Solids. Technical Report TR95-008,
     * Department of Computer Science, University of North Carolina, 1995.
     * Appendix B: Partitioning a Simple Polygon using Non-Intersecting
     * Chains.
     */

    if (curves.Count() == 0) {
	// No curve, no splitting
	out.Append(in->Duplicate());
	return 0;
    }

    // Get the intersection points between the SSI curves and the outerloop.
    ON_SimpleArray<IntersectPoint> intersect;
    ON_SimpleArray<bool> have_intersect(curves.Count());
    for (int i = 0; i < curves.Count(); i++)
	have_intersect[i] = false;

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
		if (x_event.Count())
		    have_intersect[j] = true;
	    }
	}
    }

#if USE_CONNECTIVITY_GRAPH
    // Keep track of information about which SSI curves are used into the final
    // trimmed face, so that we can know which trimmed face on the other face
    // shares an SSI curve with it (and they become neighbors in the connectivity
    // graph).
    ON_ClassArray<LinkedCurve> curves_from_ssi;
#endif

    // deal with the situations where there is no intersection
    std::vector<ON_SimpleArray<ON_Curve*> > innerloops;
    for (int i = 0; i < curves.Count(); i++) {
	if (!have_intersect[i]) {
	    // The start point cannot be on the boundary of the loop, because
	    // there is no intersections between curves[i] and the loop.
	    if (IsPointInsideLoop(curves[i].PointAtStart(), in->m_outerloop) == 1) {
		if (curves[i].IsClosed()) {
		    ON_SimpleArray<ON_Curve*> iloop;
		    curves[i].AppendCurvesToArray(iloop);
		    innerloops.push_back(iloop);
		    TrimmedFace *newface = new TrimmedFace();
		    newface->m_face = in->m_face;
		    curves[i].AppendCurvesToArray(newface->m_outerloop);
#if USE_CONNECTIVITY_GRAPH
		    // It doesn't share its parent's outerloop
		    newface->m_parts.Empty();
		    // It used the SSI curve (curves[i])
		    curves[i].AppendSSIInfoToArray(newface->m_ssi_info);
		    curves_from_ssi.Append(curves[i]);
#endif
		    out.Append(newface);
		}
	    }
	}
    }

    // rank these intersection points
    // during the time using pts_on_curves(), must make sure that
    // the capacity of intersect[] never change.
    ON_ClassArray<ON_SimpleArray<IntersectPoint*> > pts_on_curves(curves.Count());
    for (int i = 0; i < intersect.Count(); i++) {
	pts_on_curves[intersect[i].m_type].Append(&(intersect[i]));
    }
    for (int i = 0; i < curves.Count(); i++) {
	pts_on_curves[i].QuickSort(compare_for_rank);
	for (int j = 0; j < pts_on_curves[i].Count(); j++)
	    pts_on_curves[i][j]->m_rank = j;
    }

    ON_SimpleArray<IntersectPoint> intersect_append;

    // Determine whether it's going inside or outside (IntersectPoint::m_in_out).
    for (int i = 0; i < curves.Count(); i++) {
	for (int j = 0;  j < pts_on_curves[i].Count(); j++) {
	    IntersectPoint* ipt = pts_on_curves[i][j];
	    if (pts_on_curves[i].Count() < 2)
		ipt->m_in_out = IntersectPoint::UNSET;
	    else {
		ON_3dPoint left = j == 0 ? curves[i].PointAtStart() :
		    curves[i].PointAt((ipt->m_t_for_rank+pts_on_curves[i][j-1]->m_t_for_rank)*0.5);
		ON_3dPoint right = j == pts_on_curves[i].Count() - 1 ? curves[i].PointAtEnd() :
		    curves[i].PointAt((ipt->m_t_for_rank+pts_on_curves[i][j+1]->m_t_for_rank)*0.5);
		// If the point is on the boundary, we treat it with the same
		// way as it's outside.
		// For example, the left side is inside, and the right's on
		// boundary, that point should be IntersectPoint::OUT, the
		// same as the right's outside the loop.
		// Other cases are similar.
		int left_in = IsPointInsideLoop(left, in->m_outerloop) == 1 && IsPointOnLoop(left, in->m_outerloop) == 0;
		int right_in = IsPointInsideLoop(right, in->m_outerloop) == 1 && IsPointOnLoop(right, in->m_outerloop) == 0;
		if (left_in < 0 || right_in < 0) {
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
			intersect_append.Last()->m_rank = ipt->m_rank+1;
			for (int k = j + 1; k < pts_on_curves[i].Count(); k++)
			    pts_on_curves[i][k]->m_rank++;
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
    ON_SimpleArray<ON_Curve*> outerloop;    // segments of the outerloop
#if USE_CONNECTIVITY_GRAPH
    // the start point and end point of an outerloop segment
    // outerloop_start_end[] and outerloop[] should have the same size, and
    // outerloop_start_end[i] should be corresponding to outerloop[i] exactly.
    ON_ClassArray<std::pair<IntersectPoint, IntersectPoint> > outerloop_start_end;
#endif
    int isect_iter = 0;
    for (int i = 0; i < in->m_outerloop.Count(); i++) {
	ON_Curve *curve_on_loop = in->m_outerloop[i]->Duplicate();
	if (curve_on_loop == NULL) {
	    bu_log("ON_Curve::Duplicate() failed.\n");
	    continue;
	}
	for (; isect_iter < intersect.Count() && intersect[isect_iter].m_seg == i; isect_iter++) {
	    const IntersectPoint& isect_pt = intersect[isect_iter];
	    ON_Curve* left = NULL;
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
#if USE_CONNECTIVITY_GRAPH
		IntersectPoint start;
		if (outerloop_start_end.Count() == 0 || outerloop_start_end.Last()->second.m_seg != i) {
		    // It should use the start point of in->outerloop[i]
		    start.m_seg = i;
		    start.m_t = in->m_outerloop[i]->Domain().Min();
		} else {
		    // Continuous to the last one
		    start = outerloop_start_end.Last()->second;
		}
		outerloop_start_end.Append(std::make_pair(start, isect_pt));
#endif
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
#if USE_CONNECTIVITY_GRAPH
	    IntersectPoint start, end;
	    if (outerloop_start_end.Count() == 0 || outerloop_start_end.Last()->second.m_seg != i) {
		// It should use the start point of in->outerloop[i]
		start.m_seg = i;
		start.m_t = in->m_outerloop[i]->Domain().Min();
	    } else {
		// Continuous to the last one
		start = outerloop_start_end.Last()->second;
	    }
	    end.m_seg = i;
	    end.m_t = in->m_outerloop[i]->Domain().Max();
	    outerloop_start_end.Append(std::make_pair(start, end));
#endif
	}
    }

    // Append the first element at the last to handle some special cases.
    if (intersect.Count()) {
	intersect.Append(intersect[0]);
	intersect.Last()->m_seg += in->m_outerloop.Count();
	for (int i = 0; i <= intersect[0].m_pos; i++) {
	    ON_Curve* dup = outerloop[i]->Duplicate();
	    if (dup != NULL) {
		outerloop.Append(dup);
	    }
	    else {
		bu_log("ON_Curve::Duplicate() failed.\n");
		outerloop.Append(NULL);
	    }
#if USE_CONNECTIVITY_GRAPH
	    outerloop_start_end.Append(outerloop_start_end[i]);
#endif
	}
	intersect.Last()->m_pos = outerloop.Count() - 1;
    }

    if (DEBUG_BREP_BOOLEAN)
	for (int i = 0; i < intersect.Count(); i++)
	    bu_log("intersect[%d](count = %d): m_type = %d, m_rank = %d, m_in_out = %d\n", i, intersect.Count(), intersect[i].m_type, intersect[i].m_rank, intersect[i].m_in_out);

    std::stack<int> s;

    for (int i = 0; i < intersect.Count(); i++) {
#if USE_CONNECTIVITY_GRAPH
	// Check the validity of outerloop_start_end
	if (outerloop_start_end.Count() != outerloop.Count()) {
	    bu_log("split_trimmed_face() Error [i = %d]: outerloop_start_end.Count() != outerloop.Count()\n", i);
	    return -1;
	}
#endif

	// Ignore UNSET IntersectPoints.
	if (intersect[i].m_in_out == IntersectPoint::UNSET)
	    continue;

	if (s.empty()) {
	    s.push(i);
	    continue;
	}

	const IntersectPoint& p = intersect[s.top()];
	const IntersectPoint& q = intersect[i];

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
	ON_SimpleArray<ON_Curve*> newloop;
#if USE_CONNECTIVITY_GRAPH
	ON_ClassArray<std::pair<IntersectPoint, IntersectPoint> > newloop_start_end;
#endif
	int curve_count = q.m_pos - p.m_pos;
	for (int j = p.m_pos + 1; j <= q.m_pos; j++) {
	    // No need to duplicate the curve, because the pointer
	    // in the array 'outerloop' will be moved out later.
	    newloop.Append(outerloop[j]);
#if USE_CONNECTIVITY_GRAPH
	    newloop_start_end.Append(outerloop_start_end[j]);
#endif
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
	ON_Curve* seg_on_SSI = curves[p.m_type].SubCurve(t1, t2);
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

	ON_Curve* rev_seg_on_SSI = seg_on_SSI->Duplicate();
	if (!rev_seg_on_SSI || !rev_seg_on_SSI->Reverse()) {
	    bu_log("Reverse failed.\n");
	    continue;
	} else {
	    // Update the outerloop
	    outerloop[p.m_pos + 1] = rev_seg_on_SSI;
	    int k = p.m_pos + 2;
	    for (int j = q.m_pos + 1; j < outerloop.Count(); j++)
		outerloop[k++] = outerloop[j];
	    while (k < outerloop.Count()) {
		outerloop[outerloop.Count()-1] = NULL;
		outerloop.Remove();
	    }
#if USE_CONNECTIVITY_GRAPH
	    // Update outerloop_start_end
	    IntersectPoint invalid_point;
	    invalid_point.m_seg = -1;
	    outerloop_start_end[p.m_pos + 1] = std::make_pair(invalid_point, invalid_point);
	    k = p.m_pos + 2;
	    for (int j = q.m_pos + 1; j < outerloop_start_end.Count(); j++)
		outerloop_start_end[k++] = outerloop_start_end[j];
	    while (k < outerloop_start_end.Count()) {
		outerloop_start_end.Remove();
	    }
	    // Update curves_from_ssi
	    curves_from_ssi.Append(curves[p.m_type]);
#endif
	    // Update m_pos
	    for (int j = i + 1; j < intersect.Count(); j++)
		intersect[j].m_pos -= curve_count - 1;
	}

	// Append a trimmed face with newloop as its outerloop
	// Don't add a face if the outerloop is not valid (e.g. degenerated).
	if (IsLoopValid(newloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *newface = new TrimmedFace();
	    newface->m_face = in->m_face;
	    newface->m_outerloop.Append(newloop.Count(), newloop.Array());
#if USE_CONNECTIVITY_GRAPH
	    for (int j = 0; j < newloop_start_end.Count(); j++)
		if (newloop_start_end[j].first.m_seg != -1)
		    newface->m_parts.Append(newloop_start_end[j]);
	    curves[p.m_type].AppendSSIInfoToArray(newface->m_ssi_info);
#endif
	    out.Append(newface);
	} else {
	    for (int j = 0; j < newloop.Count(); j++)
		delete newloop[j];
	}
    }

    // Remove the duplicated segments before the first intersection point.
    if (intersect.Count()) {
	for (int i = 0; i <= intersect[0].m_pos; i++) {
	    delete outerloop[0];
	    outerloop[0] = NULL;
	    outerloop.Remove(0);
#if USE_CONNECTIVITY_GRAPH
	    outerloop_start_end.Remove(0);
#endif
	}
    }

    if (out.Count() == 0) {
	out.Append(in->Duplicate());
    } else {
	// The remaining part after splitting some parts out.
	if (IsLoopValid(outerloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *newface = new TrimmedFace();
	    newface->m_face = in->m_face;
	    newface->m_outerloop = outerloop;
	    // First copy the array with pointers, and then change
	    // the pointers into copies.
	    newface->m_innerloop = in->m_innerloop;
	    for (unsigned int i = 0; i < in->m_innerloop.size(); i++)
		for (int j = 0; j < in->m_innerloop[i].Count(); j++)
		    if (in->m_innerloop[i][j])
			newface->m_innerloop[i][j] = in->m_innerloop[i][j]->Duplicate();
	    newface->m_innerloop.insert(newface->m_innerloop.end(), innerloops.begin(), innerloops.end());
#if USE_CONNECTIVITY_GRAPH
	    for (int i = 0; i < outerloop_start_end.Count(); i++)
		if (outerloop_start_end[i].first.m_seg != -1)
		    newface->m_parts.Append(outerloop_start_end[i]);
	    for (int i = 0; i < curves_from_ssi.Count(); i++)
		curves_from_ssi[i].AppendSSIInfoToArray(newface->m_ssi_info);
#endif
	    out.Append(newface);
	} else {
	    for (int i = 0; i < outerloop.Count(); i++)
		if (outerloop[i])
		    delete outerloop[i];
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

    return 0;
}


bool IsSameSurface(const ON_Surface* surfA, const ON_Surface* surfB)
{
    // Approach: Get their NURBS forms, and compare their CVs.
    // If their CVs are all the same (location and weight), they are
    // regarded as the same surface.

    if (surfA == NULL || surfB == NULL)
	return false;
/*
    // Deal with two planes, if that's what we have - in that case
    // the determination can be more general than the CV comparison
    ON_Plane surfA_plane, surfB_plane;
    if (surfA->IsPlanar(&surfA_plane) && surfB->IsPlanar(&surfB_plane)) {
	ON_3dVector surfA_normal = surfA_plane.Normal();
	ON_3dVector surfB_normal = surfB_plane.Normal();
	if (surfA_normal.IsParallelTo(surfB_normal) == 1) {
	    if (surfA_plane.DistanceTo(surfB_plane.Origin()) < ON_ZERO_TOLERANCE) {
		return true;
	    } else {
		return false;
	    }
	} else {
	    return false;
	}
    }
*/

    ON_NurbsSurface nurbsSurfaceA, nurbsSurfaceB;
    if (!surfA->GetNurbForm(nurbsSurfaceA) || !surfB->GetNurbForm(nurbsSurfaceB))
	return false;

    if (nurbsSurfaceA.Degree(0) != nurbsSurfaceB.Degree(0)
	|| nurbsSurfaceA.Degree(1) != nurbsSurfaceB.Degree(1))
	return false;

    if (nurbsSurfaceA.CVCount(0) != nurbsSurfaceB.CVCount(0)
	|| nurbsSurfaceA.CVCount(1) != nurbsSurfaceB.CVCount(1))
	return false;

    for (int i = 0; i < nurbsSurfaceA.CVCount(0); i++)
	for (int j = 0; j < nurbsSurfaceB.CVCount(1); j++) {
	    ON_4dPoint cvA, cvB;
	    nurbsSurfaceA.GetCV(i, j, cvA);
	    nurbsSurfaceB.GetCV(i, j, cvB);
	    if (cvA != cvB)
		return false;
	}

    if (nurbsSurfaceA.KnotCount(0) != nurbsSurfaceB.KnotCount(0)
	|| nurbsSurfaceA.KnotCount(1) != nurbsSurfaceB.KnotCount(1))
	return false;

    for (int i = 0; i < nurbsSurfaceA.KnotCount(0); i++)
	if (!ON_NearZero(nurbsSurfaceA.m_knot[0][i] - nurbsSurfaceB.m_knot[0][i]))
	    return false;

    for (int i = 0; i < nurbsSurfaceA.KnotCount(1); i++)
	if (!ON_NearZero(nurbsSurfaceA.m_knot[1][i] - nurbsSurfaceB.m_knot[1][i]))
	    return false;

    return true;
}


HIDDEN void
add_elements(ON_Brep *brep, ON_BrepFace &face, const ON_SimpleArray<ON_Curve*> &loop, ON_BrepLoop::TYPE loop_type)
{
    if (!loop.Count())
	return;

    ON_BrepLoop &breploop = brep->NewLoop(loop_type, face);
    const ON_Surface* srf = face.SurfaceOf();

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
			ON_BrepTrim& trim = brep->m_T[breploop.m_ti[i]];
			if (ON_BrepTrim::boundary != trim.m_type)
			    continue;
			iso2 = srf->IsIsoparametric(trim);
			if (iso2 != iso_type)
			    continue;
			s1 = side_interval.NormalizedParameterAt(trim.PointAtStart()[endpt_index]);
			if (fabs(s1 - 1.0) > side_tol)
			    continue;
			s0 = side_interval.NormalizedParameterAt(trim.PointAtEnd()[endpt_index]);
			if (fabs(s0) > side_tol)
			    continue;

			// Check 3D distances - not included in ON_Brep::IsValid().
			// So with this check, we only add seam trims if their end points
			// are the same within ON_ZERO_TOLERANCE. This will cause IsValid()
			// reporting "they should be seam trims connected to the same edge",
			// because the 2D tolerance (side_tol) are hardcoded to 1.0e-4.
			// We still add this check because we treat two vertexes to be the
			// same only if their distance < ON_ZERO_TOLERANCE. (Maybe 3D dist
			// should also be added to ON_Brep::IsValid()?)
			if (srf->PointAt(trim.PointAtStart().x, trim.PointAtStart().y).DistanceTo(
			    srf->PointAt(loop[k]->PointAtEnd().x, loop[k]->PointAtEnd().y)) >= ON_ZERO_TOLERANCE)
			    continue;

			if (srf->PointAt(trim.PointAtEnd().x, trim.PointAtEnd().y).DistanceTo(
			    srf->PointAt(loop[k]->PointAtStart().x, loop[k]->PointAtStart().y)) >= ON_ZERO_TOLERANCE)
			    continue;

			// We add another checking, which is not included in ON_Brep::IsValid()
			// - they should be iso boundaries of the surface.
			double s2 = srf->Domain(1-endpt_index).NormalizedParameterAt(loop[k]->PointAtStart()[1-endpt_index]);
			double s3 = srf->Domain(1-endpt_index).NormalizedParameterAt(trim.PointAtStart()[1-endpt_index]);
			if ((fabs(s2 - 1.0) < side_tol && fabs(s3) < side_tol) ||
			    (fabs(s2) < side_tol && fabs(s3 - 1.0) < side_tol)) {
			    // Find a trim that should share the same edge
			    int ti = brep->AddTrimCurve(loop[k]);
			    ON_BrepTrim& newtrim = brep->NewTrim(brep->m_E[trim.m_ei], true, breploop, ti);
			    // newtrim.m_type = ON_BrepTrim::seam;
			    newtrim.m_tolerance[0] = newtrim.m_tolerance[1] = MAX_FASTF;
			    seamed = true;
			    break;
			}
		    }
		    if (seamed)
			continue;
		}
	    }
	}

	ON_Curve* c3d = NULL;
	// First, try the ON_Surface::Pushup() method.
	// If Pushup() does not succeed, use sampling method.
	c3d = face.SurfaceOf()->Pushup(*(loop[k]), INTERSECTION_TOL);
	if (!c3d) {
	    ON_3dPointArray ptarray(101);
	    for (int l = 0; l <= 100; l++) {
		ON_3dPoint pt2d;
		pt2d = loop[k]->PointAt(loop[k]->Domain().ParameterAt(l/100.0));
		ptarray.Append(face.SurfaceOf()->PointAt(pt2d.x, pt2d.y));
	    }
	    c3d = new ON_PolylineCurve(ptarray);
	}

	if (c3d->BoundingBox().Diagonal().Length() < ON_ZERO_TOLERANCE) {
	    // The trim is singular
	    int i;
	    ON_3dPoint vtx = c3d->PointAtStart();
	    for (i = brep->m_V.Count() - 1; i >= 0; i--)
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE)
		    break;
	    if (i < 0) {
		i = brep->m_V.Count();
		brep->NewVertex(c3d->PointAtStart(), 0.0);
	    }
	    int ti = brep->AddTrimCurve(loop[k]);
	    ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[i], breploop, srf->IsIsoparametric(*loop[k]), ti);
	    trim.m_tolerance[0] = trim.m_tolerance[1] = MAX_FASTF;
	    delete c3d;
	    continue;
	}

	ON_2dPoint start = loop[k]->PointAtStart(), end = loop[k]->PointAtEnd();
	int start_idx, end_idx;

	// Get the start vertex index
	if (k > 0)
	    start_idx = brep->m_T.Last()->m_vi[1];
	else {
	    ON_3dPoint vtx = face.SurfaceOf()->PointAt(start.x, start.y);
	    int i;
	    for (i = 0; i < brep->m_V.Count(); i++)
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE)
		    break;
	    start_idx = i;
	    if (i == brep->m_V.Count()) {
		brep->NewVertex(vtx, 0.0);
	    }
	}

	// Get the end vertex index
	if (c3d->IsClosed())
	    end_idx = start_idx;
	else {
	    ON_3dPoint vtx = face.SurfaceOf()->PointAt(end.x, end.y);
	    int i;
	    for (i = 0; i < brep->m_V.Count(); i++)
		if (brep->m_V[i].Point().DistanceTo(vtx) < ON_ZERO_TOLERANCE)
		    break;
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


#if USE_CONNECTIVITY_GRAPH
HIDDEN int
build_connectivity_graph(const ON_Brep* brep, ON_SimpleArray<TrimmedFace*>& trimmedfaces, int start_idx = -1, int end_idx = -1)
{
    if (start_idx == -1)
	start_idx = 0;

    if (end_idx == -1)
	end_idx = trimmedfaces.Count() - 1;

    int facecount = brep->m_F.Count();
    if (end_idx - start_idx + 1 != facecount) {
	bu_log("build_connectivity_graph() Error: length of [start_idx, end_idx] not equal to the face count.\n");
	return -1;
    }

    // Index of the edges of a face
    ON_ClassArray<ON_SimpleArray<int> > edge_index(facecount);

    for (int i = 0; i < facecount; i++) {
	ON_SimpleArray<int> ei;
	const ON_BrepFace& face = brep->m_F[i];
	for (int j = 0; j < face.m_li.Count(); j++) {
	    const ON_BrepLoop& loop = brep->m_L[face.m_li[j]];
	    for (int k = 0; k < loop.m_ti.Count(); k++) {
		const ON_BrepTrim& trim = brep->m_T[loop.m_ti[k]];
		if (trim.m_ei != -1) {
		    // The trim is not singular. It's corresponding to an edge
		    ei.Append(trim.m_ei);
		}
	    }
	}
	edge_index.Append(ei);
    }

    if (edge_index.Count() != facecount) {
	bu_log("build_connectivity_graph() Error: edge_index.Count() != brep->m_F.Count()\n");
	return -1;
    }

    // Find the faces that share an edge
    for (int i = 0; i < facecount; i++) {
	for (int j = i + 1; j < facecount; j++) {
	    bool shared = false;
	    // Using O(n^2) searching. Because the problem size is usually very
	    // small, this is an ideal choice.
	    for (int k = 0; k < edge_index[i].Count(); k++) {
		if (edge_index[j].Search(edge_index[i][k]) != -1) {
		    shared = true;
		    break;
		}
	    }
	    if (shared) {
		// They share an edge, so they're neighbors.
		// Search the array to avoid duplication
		if (trimmedfaces[start_idx+i]->m_neighbors.Search(trimmedfaces[start_idx+j]) == -1) {
		    trimmedfaces[start_idx+i]->m_neighbors.Append(trimmedfaces[start_idx+j]);
		}
		if (trimmedfaces[start_idx+j]->m_neighbors.Search(trimmedfaces[start_idx+i]) == -1) {
		    trimmedfaces[start_idx+j]->m_neighbors.Append(trimmedfaces[start_idx+i]);
		}
	    }
	}
    }

    return 0;
}
#endif	// #if USE_CONNECTIVITY_GRAPH


HIDDEN bool
IsPointOnBrepSurface(const ON_3dPoint& pt, const ON_Brep* brep, ON_SimpleArray<Subsurface*>& surf_tree)
{
    // Decide whether a point is on a brep's surface.
    // Basic approach: use PSI on the point with all the surfaces.

    if (brep == NULL || pt.IsUnsetPoint()) {
	bu_log("IsPointOnBrepSurface(): brep == NULL || pt.IsUnsetPoint()\n");
	return false;
    }

    if (surf_tree.Count() != brep->m_S.Count()) {
	bu_log("IsPointOnBrepSurface(): surf_tree.Count() != brep->m_S.Count()\n");
	return false;
    }

    ON_BoundingBox bbox = brep->BoundingBox();
    bbox.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    bbox.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bbox.IsPointIn(pt))
	return false;

    for (int i = 0; i < brep->m_F.Count(); i++) {
	const ON_BrepFace& face = brep->m_F[i];
	const ON_Surface* surf = face.SurfaceOf();
	ON_ClassArray<ON_PX_EVENT> px_event;
	if (!ON_Intersect(pt, *surf, px_event, INTERSECTION_TOL, 0, 0, surf_tree[face.m_si]))
	    continue;

	// Get the trimming curves of the face, and determine whether the
	// points are inside the outerloop
	ON_SimpleArray<ON_Curve*> outerloop;
	const ON_BrepLoop& loop = brep->m_L[face.m_li[0]];  // outerloop only
	for (int j = 0; j < loop.m_ti.Count(); j++) {
	    outerloop.Append(brep->m_C2[brep->m_T[loop.m_ti[j]].m_c2i]);
	}
	ON_2dPoint pt2d(px_event[0].m_b[0], px_event[0].m_b[1]);
	if (IsPointInsideLoop(pt2d, outerloop) == 1 || IsPointOnLoop(pt2d, outerloop) == 1)
	    return true;
    }

    return false;
}


HIDDEN bool
IsPointInsideBrep(const ON_3dPoint& pt, const ON_Brep* brep, ON_SimpleArray<Subsurface*>& surf_tree)
{
    // Decide whether a point is inside a brep's surface.
    // Basic approach: intersect a ray with the brep, and count the number of
    // intersections (like the raytrace)
    // Returns true (inside) or false (outside) provided the pt is not on the
    // surfaces. (See also IsPointOnBrepSurface())

    if (brep == NULL || pt.IsUnsetPoint()) {
	bu_log("IsPointInsideBrep(): brep == NULL || pt.IsUnsetPoint()\n");
	return false;
    }

    if (surf_tree.Count() != brep->m_S.Count()) {
	bu_log("IsPointInsideBrep(): surf_tree.Count() != brep->m_S.Count()\n");
	return false;
    }

    ON_BoundingBox bbox = brep->BoundingBox();
    bbox.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    bbox.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bbox.IsPointIn(pt))
	return false;

    ON_3dVector diag = bbox.Diagonal()*1.5;  // Make it even longer
    ON_LineCurve line(pt, pt + diag);	// pt + diag should be outside, if pt
					// is inside the bbox

    ON_3dPointArray isect_pt;
    for (int i = 0; i < brep->m_F.Count(); i++) {
	const ON_BrepFace& face = brep->m_F[i];
	const ON_Surface* surf = face.SurfaceOf();
	ON_SimpleArray<ON_X_EVENT> x_event;
	if (!ON_Intersect(&line, surf, x_event, INTERSECTION_TOL, 0.0, 0, 0, 0, 0, 0, surf_tree[face.m_si]))
	    continue;

	// Get the trimming curves of the face, and determine whether the
	// points are inside the outerloop
	ON_SimpleArray<ON_Curve*> outerloop;
	const ON_BrepLoop& loop = brep->m_L[face.m_li[0]];  // outerloop only
	for (int j = 0; j < loop.m_ti.Count(); j++) {
	    outerloop.Append(brep->m_C2[brep->m_T[loop.m_ti[j]].m_c2i]);
	}
	for (int j = 0; j < x_event.Count(); j++) {
	    ON_2dPoint pt2d(x_event[j].m_b[0], x_event[j].m_b[1]);
	    if (IsPointInsideLoop(pt2d, outerloop) == 1 || IsPointOnLoop(pt2d, outerloop) == 1)
		isect_pt.Append(x_event[j].m_B[0]);
	    if (x_event[j].m_type == ON_X_EVENT::ccx_overlap) {
		pt2d = ON_2dPoint(x_event[j].m_b[2], x_event[j].m_b[3]);
		if (IsPointInsideLoop(pt2d, outerloop) == 1 || IsPointOnLoop(pt2d, outerloop) == 1)
		    isect_pt.Append(x_event[j].m_B[1]);
	    }
	}
    }

    // Remove duplications
    ON_3dPointArray pt_no_dup;
    for (int i = 0; i < isect_pt.Count(); i++) {
	int j;
	for (j = 0; j < pt_no_dup.Count(); j++) {
	    if (isect_pt[i].DistanceTo(pt_no_dup[j]) < INTERSECTION_TOL)
		break;
	}
	if (j == pt_no_dup.Count()) {
	    // No duplication, append to the array
	    pt_no_dup.Append(isect_pt[i]);
	}
    }

    return pt_no_dup.Count() % 2 != 0;
}


HIDDEN int
IsFaceInsideBrep(const TrimmedFace* tface, const ON_Brep* brep, ON_SimpleArray<Subsurface*>& surf_tree)
{
    // returns:
    //  -1: whether the face is inside/outside is unknown
    //   0: the face is outside the brep
    //   1: the face is inside the brep
    //   2: the face is (completely) on the brep's surface
    //      (because the overlap parts will be split out as separated trimmed
    //       faces, if one point on a trimmed face is on the brep's surface,
    //       the whole trimmed face should be on the surface)

    if (tface == NULL || brep == NULL)
	return -1;

    const ON_BrepFace* bface = tface->m_face;
    if (bface == NULL)
	return -1;

    ON_BoundingBox brep_bbox = brep->BoundingBox();
    brep_bbox.m_min -= ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    brep_bbox.m_max += ON_3dVector(INTERSECTION_TOL, INTERSECTION_TOL, INTERSECTION_TOL);
    if (!bface->BoundingBox().Intersection(brep_bbox))
	return 0;

    if (tface->m_outerloop.Count() == 0) {
	bu_log("IsFaceInsideBrep(): the input TrimmedFace is not trimmed.\n");
	return -1;
    }

    ON_PolyCurve polycurve;
    if (!IsLoopValid(tface->m_outerloop, ON_ZERO_TOLERANCE, &polycurve)) {
	return -1;
    }

    // Get a point inside the TrimmedFace, and then call IsPointInsideBrep().
    // First, try the center of its 2D domain.
    const int try_count = 10;
    ON_BoundingBox bbox =  polycurve.BoundingBox();
    bool found = false;
    ON_2dPoint test_pt2d;
    ON_RandomNumberGenerator rng;
    for (int i = 0; i < try_count; i++) {
	// Get a random point inside the outerloop's bounding box.
	double x = rng.RandomDouble(bbox.m_min.x, bbox.m_max.x);
	double y = rng.RandomDouble(bbox.m_min.y, bbox.m_max.y);
	test_pt2d = ON_2dPoint(x, y);
	if (IsPointInsideLoop(test_pt2d, tface->m_outerloop) == 1
	    && IsPointOnLoop(test_pt2d, tface->m_outerloop) == 0) {
	    unsigned int j = 0;
	    // The test point should not be inside an innerloop
	    for (j = 0; j < tface->m_innerloop.size(); j++) {
		if (IsPointInsideLoop(test_pt2d, tface->m_innerloop[j]) == 1
		    || IsPointOnLoop(test_pt2d, tface->m_innerloop[j]) == 1)
		    break;
	    }
	    if (j == tface->m_innerloop.size()) {
		// We find a valid test point
		found = true;
		break;
	    }
	}
    }

    if (!found) {
	bu_log("Cannot find a point inside this trimmed face. Aborted.\n");
	return -1;
    }

    ON_3dPoint test_pt3d = tface->m_face->PointAt(test_pt2d.x, test_pt2d.y);
    if (DEBUG_BREP_BOOLEAN)
	bu_log("valid test point: (%g, %g, %g)\n", test_pt3d.x, test_pt3d.y, test_pt3d.z);

    if (IsPointOnBrepSurface(test_pt3d, brep, surf_tree))
	return 2;

    return IsPointInsideBrep(test_pt3d, brep, surf_tree) ? 1 : 0;
}


int
ON_Boolean(ON_Brep* brepO, const ON_Brep* brepA, const ON_Brep* brepB, op_type operation)
{
    /* Deal with the trivial cases up front */
    if (brepA->BoundingBox().MinimumDistanceTo(brepB->BoundingBox()) > ON_ZERO_TOLERANCE) {
	switch(operation) {
	    case BOOLEAN_UNION:
		brepO->Append(*brepA);
		brepO->Append(*brepB);
		break;
	    case BOOLEAN_DIFF:
		brepO->Append(*brepA);
		break;
	    case BOOLEAN_INTERSECT:
		return 0;
		break;
	    default:
		bu_log("Error - unknown boolean operation\n");
		return -1;
	}

	brepO->ShrinkSurfaces();
	brepO->Compact();
	return 0;
    }

    std::set<int> A_unused, B_unused;
    std::set<int> A_finalform, B_finalform;
    int facecount1 = brepA->m_F.Count();
    int facecount2 = brepB->m_F.Count();

    /* Depending on the operation type and the bounding box behaviors, we
     * can sometimes decide immediately whether a face will end up in the
     * final brep or will have no role in the intersections - do that
     * categorization up front */
    for (int i = 0; i < facecount1 + facecount2; i++) {
	const ON_BrepFace &face = i < facecount1 ? brepA->m_F[i] : brepB->m_F[i - facecount1];
	const ON_Brep *brep = i < facecount1 ? brepB : brepA;
	std::set<int> *unused = i < facecount1 ? &A_unused : &B_unused;
	std::set<int> *intact = i < facecount1 ? &A_finalform : &B_finalform;
	int curr_index = i < facecount1 ? i : i - facecount1;
	if (face.BoundingBox().MinimumDistanceTo(brep->BoundingBox()) > ON_ZERO_TOLERANCE) {
	    switch(operation) {
		case BOOLEAN_UNION:
		    intact->insert(curr_index);
		    break;
		case BOOLEAN_DIFF:
		    if (i < facecount1) intact->insert(curr_index);
		    if (i >= facecount1) unused->insert(curr_index);
		    break;
		case BOOLEAN_INTERSECT:
		    unused->insert(curr_index);
		    break;
		default:
		    bu_log("Error - unknown boolean operation\n");
		    break;
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
    std::set<int> A_cat2;
    std::set<int> B_cat2;
    for (int i = 0; i < facecount1; i++) {
	if (A_unused.find(i) == A_unused.end() && A_finalform.find(i) == A_finalform.end()) {
	    for (int j = 0; j < facecount2; j++) {
		if (B_unused.find(j) == B_unused.end() &&  B_finalform.find(j) == B_finalform.end()) {
		    bu_log("Checking: %d, %d\n", i, j);
		    // If the two faces don't interact according to their bounding boxes,
		    // they won't be a source of events - otherwise, they must be checked.
		    fastf_t disjoint = brepA->m_F[i].BoundingBox().MinimumDistanceTo(brepB->m_F[j].BoundingBox());
		    if (!(disjoint > ON_ZERO_TOLERANCE)) {
			intersection_candidates.insert(std::pair<int, int>(i,j));
			A_cat2.insert(i);
			B_cat2.insert(j);
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

    // Is it possible to do a Face/Face intersection test on cat2 cases, aware of the boolean operation, that would
    // be able to create the final faces from the A and B input faces and the intersection curves without
    // needing to do an inside/outside test on the resultant pieces?  Something like:
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
    //
    // A case to think about here is the case of two spheres intersecting - depending on A, the exact trimming
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
    //
    // Also worth thinking about - would it be possible to then do edge comparisons to
    // determine which of the "fully used/fully non-used" faces are needed?


    bu_log("Summary of brep status: \n A_unused: %d\n B_unused: %d\n A_finalform: %d\n B_finalform %d\nintersection_candidates(%d):\n", A_unused.size(), B_unused.size(), A_finalform.size(), B_finalform.size(), intersection_candidates.size());

    int surfcount1 = brepA->m_S.Count();
    int surfcount2 = brepB->m_S.Count();
    ON_ClassArray<ON_SimpleArray<SSICurve> > curvesarray(facecount1 + facecount2);

    ON_SimpleArray<Subsurface*> surf_treeA, surf_treeB;
    for (int i = 0; i < surfcount1; i++)
	surf_treeA.Append(new Subsurface(brepA->m_S[i]->Duplicate()));
    for (int i = 0; i < surfcount2; i++)
	surf_treeB.Append(new Subsurface(brepB->m_S[i]->Duplicate()));

#if 0
    std::multimap<int, ON_Curve *> added_2d_curves_A;
    std::multimap<int, ON_Curve *> added_2d_curves_B;
    for (std::set<std::pair<int, int> >::iterator it=intersection_candidates.begin(); it != intersection_candidates.end(); ++it) {
	bu_log("     (%d,%d)\n", (*it).first, (*it).second);
	ON_ClassArray<ON_SSX_EVENT> events;
	int surf_index1 = brepA->m_F[(*it).first].m_si;
	int surf_index2 = brepB->m_F[(*it).second].m_si;
	//intersection_events_A.insert(std::pair<int, struct Face_Intersection_Event * >(((*it).first), face_event));
	//intersection_events_B.insert(std::pair<int, struct Face_Intersection_Event * >(((*it).second), face_event));
	ON_Surface *surfA = brepA->m_S[surf_index1];
	ON_Surface *surfB = brepB->m_S[surf_index2];
	int results = ON_Intersect(surfA, surfB, events, INTERSECTION_TOL, 0.0, 0.0, NULL,
		NULL, NULL, NULL, surf_treeA[surf_index1], surf_treeB[surf_index2]);
	if (results)
	    bu_log("Intersecting: (%d,%d)\n", (*it).first, (*it).second);
    }
#endif
    // calculate intersection curves
    for (int i = 0; i < facecount1; i++) {
	for (int j = 0; j < facecount2; j++) {
	    ON_Surface *surfA, *surfB;
	    ON_ClassArray<ON_SSX_EVENT> events;
	    int results = 0;
	    surfA = brepA->m_S[brepA->m_F[i].m_si];
	    surfB = brepB->m_S[brepB->m_F[j].m_si];
	    if (IsSameSurface(surfA, surfB))
		continue;

	    // If the two faces don't interact according to their bounding boxes,
	    // they won't be a source of events
	    fastf_t disjoint = brepA->m_F[i].BoundingBox().MinimumDistanceTo(brepB->m_F[j].BoundingBox());
	    if (disjoint > ON_ZERO_TOLERANCE) {
		continue;
	    }

	    // Look for coplanar faces, which don't require the full surface
	    // intersection test
	    ON_Plane surfA_plane, surfB_plane;
	    int coplanar = 0;
	    if (surfA->IsPlanar(&surfA_plane) && surfB->IsPlanar(&surfB_plane)) {
		/* We already checked for disjoint above, so the only remaining question is the normals */
		if (surfA_plane.Normal().IsParallelTo(surfB_plane.Normal())) {
		    ON_SSX_EVENT Event;
		    Event.m_curve3d = Event.m_curveA = Event.m_curveB = NULL;
		    Event.m_type = ON_SSX_EVENT::ssx_overlap;
		    events.Append(Event);
		    coplanar = 1;
		    results = 1;
		    bu_log("Faces brepA->%d and brepB->%d are coplanar\n", i, j);
		}
	    }

	    // Possible enhancement: Some faces may share the same surface.
	    // We can store the result of SSI to avoid re-computation.
	    if (!coplanar) {
		results = ON_Intersect(surfA,
			surfB,
			events,
			INTERSECTION_TOL,
			0.0,
			0.0,
			NULL,
			NULL,
			NULL,
			NULL,
			surf_treeA[brepA->m_F[i].m_si],
			surf_treeB[brepB->m_F[j].m_si]);
	    }
	    if (results <= 0)
		continue;
	    ON_SimpleArray<ON_Curve*> curve_uv, curve_st;
	    for (int k = 0; k < events.Count(); k++) {
		if (events[k].m_type == ON_SSX_EVENT::ssx_tangent
		    || events[k].m_type == ON_SSX_EVENT::ssx_transverse) {
		    if (get_subcurve_inside_faces(brepA, brepB, i, j, &events[k]) < 0)
			continue;
		    SSICurve c1, c2;
		    c1.m_curve = events[k].m_curveA;
		    c2.m_curve = events[k].m_curveB;
#if USE_CONNECTIVITY_GRAPH
		    c1.m_info.m_fi1 = c2.m_info.m_fi2 = i;
		    c1.m_info.m_fi2 = c2.m_info.m_fi1 = facecount1 + j;
		    c1.m_info.m_ci = c2.m_info.m_ci = k;
#endif
		    curvesarray[i].Append(c1);
		    curvesarray[facecount1 + j].Append(c2);
		    // Set m_curveA and m_curveB to NULL, in case that they are
		    // deleted by ~ON_SSX_EVENT().
		    events[k].m_curveA = events[k].m_curveB = NULL;
		}
	    }
	}
    }

    ON_SimpleArray<TrimmedFace*> original_faces;
    for (int i = 0; i < facecount1 + facecount2; i++) {
	const ON_BrepFace &face = i < facecount1 ? brepA->m_F[i] : brepB->m_F[i - facecount1];
	const ON_Brep *brep = i < facecount1 ? brepA : brepB;
	const ON_SimpleArray<int> &loopindex = face.m_li;

	TrimmedFace *first = new TrimmedFace();
	first->m_face = &face;

	for (int j = 0; j < loopindex.Count(); j++) {
	    const ON_BrepLoop &loop = brep->m_L[loopindex[j]];
	    const ON_SimpleArray<int> &trimindex = loop.m_ti;
	    ON_SimpleArray<ON_Curve*> iloop;
	    for (int k = 0; k < trimindex.Count(); k++) {
		ON_Curve *curve2d = brep->m_C2[brep->m_T[trimindex[k]].m_c2i];
		if (j == 0) {
		    first->m_outerloop.Append(curve2d->Duplicate());
		} else {
		    iloop.Append(curve2d->Duplicate());
		}
	    }
	    if (j != 0)
		first->m_innerloop.push_back(iloop);
	}
#if USE_CONNECTIVITY_GRAPH
	if (first->m_outerloop.Count()) {
	    IntersectPoint start, end;
	    start.m_seg = 0;
	    start.m_t = first->m_outerloop[0]->Domain().Min();
	    end.m_seg = first->m_outerloop.Count() - 1;
	    end.m_t = (*first->m_outerloop.Last())->Domain().Max();
	    first->m_parts.Append(std::make_pair(start, end));
	}
#endif
	original_faces.Append(first);
    }

    if (original_faces.Count() != facecount1 + facecount2) {
	bu_log("ON_Boolean() Error: TrimmedFace generation failed.\n");
	return -1;
    }

#if USE_CONNECTIVITY_GRAPH
    build_connectivity_graph(brepA, original_faces, 0, facecount1 - 1);
    build_connectivity_graph(brepB, original_faces, facecount1, original_faces.Count() - 1);

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("The connectivity graph for the first brep structure.");
	for (int i = 0; i < facecount1; i++) {
	    bu_log("\nFace[%d]'s neighbors:", i);
	    for (int j = 0; j < original_faces[i]->m_neighbors.Count(); j++) {
		bu_log(" %d", original_faces.Search(original_faces[i]->m_neighbors[j]));
	    }
	}
	bu_log("\nThe connectivity graph for the second brep structure.");
	for (int i = 0; i < facecount2; i++) {
	    bu_log("\nFace[%d]'s neighbors:", i);
	    for (int j = 0; j < original_faces[facecount1 + i]->m_neighbors.Count(); j++) {
		bu_log(" %d", original_faces.Search(original_faces[facecount1 + i]->m_neighbors[j]) - facecount1);
	    }
	}
	bu_log("\n");
    }
#endif	// #if USE_CONNECTIVITY_GRAPH

    // split the surfaces with the intersection curves
    ON_ClassArray<ON_SimpleArray<TrimmedFace*> > trimmedfaces;
    for (int i = 0; i < original_faces.Count(); i++) {
	TrimmedFace* first = original_faces[i];
	ON_ClassArray<LinkedCurve> linked_curves;
	link_curves(curvesarray[i], linked_curves);
	ON_SimpleArray<TrimmedFace*> splitted;
	split_trimmed_face(splitted, first, linked_curves);
	trimmedfaces.Append(splitted);

	// Delete the curves passed in.
	// Only the copies of them will be used later.
	for (int j = 0; j < linked_curves.Count(); j++)
	    for (int k = 0; k < linked_curves[j].m_ssi_curves.Count(); k++)
		if (linked_curves[j].m_ssi_curves[k].m_curve) {
		    delete linked_curves[j].m_ssi_curves[k].m_curve;
		    linked_curves[j].m_ssi_curves[k].m_curve = NULL;
		}
    }

    if (trimmedfaces.Count() != original_faces.Count()) {
	bu_log("ON_Boolean() Error: trimmedfaces.Count() != original_faces.Count()\n");
	return -1;
    }

#if USE_CONNECTIVITY_GRAPH
    // Update the connectivity graph after surface partitioning
    for (int i = 0; i < original_faces.Count(); i++) {
	for (int j = 0; j < trimmedfaces[i].Count(); j++) {
	    TrimmedFace* t_face = trimmedfaces[i][j];
	    if (t_face->m_parts.Count() == 0)
		continue;
	    for (int k = 0; k < original_faces[i]->m_neighbors.Count(); k++) {
		int neighbor_index = original_faces.Search(original_faces[i]->m_neighbors[k]);
		if (neighbor_index == -1)
		    continue;
		for (int l = 0; l < trimmedfaces[neighbor_index].Count(); l++) {
		    // Search all of its parent's neighbors' children, and check
		    // whether they share a common part of the original outerloop.
		    // If their "parts" intersect (overlap) in some way, they are
		    // neighbors again. (Neighboring parents' children become new
		    // neighbors: parent[i]'s child[j] with parent[i]'s neighbor[k]'s
		    // child[l])
		    TrimmedFace* another_face = trimmedfaces[neighbor_index][l];
		    if (another_face->m_parts.Count() == 0)
			continue;
		    // Find an intersection between all their "parts".
		    for (int i1 = 0; i1 < t_face->m_parts.Count(); i1++) {
			for (int i2 = 0; i2 < another_face->m_parts.Count(); i2++) {
			    IntersectPoint& start1 = t_face->m_parts[i1].first;
			    IntersectPoint& start2 = another_face->m_parts[i2].first;
			    IntersectPoint& end1 = t_face->m_parts[i1].second;
			    IntersectPoint& end2 = another_face->m_parts[i2].second;
			    if (compare_t(&start1, &end1) > 0) {
				// start > end, swap them
				std::swap(start1, end1);
			    }
			    if (compare_t(&start2, &end2) > 0) {
				std::swap(start2, end2);
			    }
			    const IntersectPoint& start_max = compare_t(&start1, &start2) < 0 ? start2 : start1;
			    const IntersectPoint& end_min = compare_t(&end1, &end2) < 0 ? end1 : end2;
			    if (compare_t(&start_max, &end_min) < 0) {
				// The intervals intersect
				if (t_face->m_neighbors.Search(another_face) == -1)
				    t_face->m_neighbors.Append(another_face);
				if (another_face->m_neighbors.Search(t_face) == -1)
				    another_face->m_neighbors.Append(t_face);
				break;
			    }
			}
		    }
		}
	    }
	}
    }

    if (DEBUG_BREP_BOOLEAN) {
	bu_log("The new connectivity graph for the first brep structure.");
	for (int i = 0; i < facecount1; i++) {
	    for (int j = 0; j < trimmedfaces[i].Count(); j++) {
		bu_log("\nFace[%p]'s neighbors:", trimmedfaces[i][j]);
		for (int k = 0; k < trimmedfaces[i][j]->m_neighbors.Count(); k++)
		    bu_log(" %p", trimmedfaces[i][j]->m_neighbors[k]);
	    }
	}
	bu_log("\nThe new connectivity graph for the second brep structure.");
	for (int i = 0; i < facecount2; i++) {
	    for (int j = 0; j < trimmedfaces[facecount1 + i].Count(); j++) {
		bu_log("\nFace[%p]'s neighbors:", trimmedfaces[facecount1 + i][j]);
		for (int k = 0; k < trimmedfaces[facecount1 + i][j]->m_neighbors.Count(); k++)
		    bu_log(" %p", trimmedfaces[facecount1 + i][j]->m_neighbors[k]);
	    }
	}
	bu_log("\n");
    }
#endif	// #if USE_CONNECTIVITY_GRAPH

    for (int i = 0; i < original_faces.Count(); i++) {
	delete original_faces[i];
	original_faces[i] = NULL;
    }

    for (int i = 0; i < trimmedfaces.Count(); i++) {
	/* Perform inside-outside test to decide whether the trimmed face should
	 * be used in the final b-rep structure or not.
	 * Different operations should be dealt with accordingly.
	 * Use connectivity graphs (optional) which represents the topological
	 * structure of the b-rep. This can reduce time-consuming inside-outside
	 * tests.
	 */
	const ON_SimpleArray<TrimmedFace*>& splitted = trimmedfaces[i];
	const ON_Brep* another_brep = i >= facecount1 ? brepA : brepB;
	ON_SimpleArray<Subsurface*>& surf_tree = i >= facecount1 ? surf_treeA : surf_treeB;
	for (int j = 0; j < splitted.Count(); j++) {
	    if (splitted[j]->m_belong_to_final != TrimmedFace::UNKNOWN) {
		// Visited before, don't need to test again
		continue;
	    }
	    splitted[j]->m_belong_to_final = TrimmedFace::NOT_BELONG;
	    splitted[j]->m_rev = false;
	    int ret_inside_test = IsFaceInsideBrep(splitted[j], another_brep, surf_tree);
	    if (ret_inside_test == 1) {
		if (DEBUG_BREP_BOOLEAN)
		    bu_log("The trimmed face is inside the other brep.\n");
		if (operation == BOOLEAN_INTERSECT || operation == BOOLEAN_XOR || (operation == BOOLEAN_DIFF && i >= facecount1))
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		if (operation == BOOLEAN_DIFF || operation == BOOLEAN_XOR)
		    splitted[j]->m_rev = true;
	    } else if (ret_inside_test == 0) {
		if (DEBUG_BREP_BOOLEAN)
		    bu_log("The trimmed face is outside the other brep.\n");
		if (operation == BOOLEAN_UNION || operation == BOOLEAN_XOR || (operation == BOOLEAN_DIFF && i < facecount1))
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
	    } else if (ret_inside_test == 2) {
		if (DEBUG_BREP_BOOLEAN)
		    bu_log("The trimmed face is on the other brep's surfaces.\n");
		if (operation == BOOLEAN_UNION || operation == BOOLEAN_INTERSECT)
		    splitted[j]->m_belong_to_final = TrimmedFace::BELONG;
		// TODO: Actually only one of them is needed in the final brep structure
	    } else {
		if (DEBUG_BREP_BOOLEAN)
		    bu_log("Whether the trimmed face is inside/outside is unknown.\n");
		splitted[j]->m_belong_to_final = TrimmedFace::UNKNOWN;
	    }
#if USE_CONNECTIVITY_GRAPH
	    // BFS the connectivity graph and marked all connected trimmed faces.
	    std::queue<TrimmedFace*> q;
	    q.push(splitted[j]);
	    while (!q.empty()) {
		TrimmedFace* front = q.front();
		q.pop();
		for (int k = 0; k < front->m_neighbors.Count(); k++) {
		    if (front->m_neighbors[k]->m_belong_to_final == TrimmedFace::UNKNOWN) {
			front->m_neighbors[k]->m_belong_to_final = splitted[j]->m_belong_to_final;
			q.push(front->m_neighbors[k]);
		    }
		}
	    }
#endif
	}
    }

    for (int i = 0; i < trimmedfaces.Count(); i++) {
	const ON_SimpleArray<TrimmedFace*>& splitted = trimmedfaces[i];
	const ON_Surface* surf = splitted.Count() ? splitted[0]->m_face->SurfaceOf() : NULL;
	bool added = false;
	for (int j = 0; j < splitted.Count(); j++) {
	    TrimmedFace* t_face = splitted[j];
	    if (t_face->m_belong_to_final == TrimmedFace::BELONG) {
		// Add the surfaces, faces, loops, trims, vertices, edges, etc.
		// to the brep structure.
		if (!added) {
		    ON_Surface *new_surf = surf->Duplicate();
		    brepO->AddSurface(new_surf);
		    added = true;
		}
		ON_BrepFace& new_face = brepO->NewFace(brepO->m_S.Count() - 1);

		add_elements(brepO, new_face, t_face->m_outerloop, ON_BrepLoop::outer);
		// ON_BrepLoop &loop = brepO->m_L[brepO->m_L.Count() - 1];
		for (unsigned int k = 0; k < t_face->m_innerloop.size(); k++)
		    add_elements(brepO, new_face, t_face->m_innerloop[k], ON_BrepLoop::inner);

		brepO->SetTrimIsoFlags(new_face);
		const ON_BrepFace& original_face = i >= facecount1 ? brepB->m_F[i - facecount1] : brepA->m_F[i];
		if (original_face.m_bRev ^ t_face->m_rev)
		    brepO->FlipFace(new_face);

#if USE_CONNECTIVITY_GRAPH
		// Generate the connectivity graph for the new solid (Remove
		// the nodes that don't belong to it)
		for (int k = 0; k < t_face->m_neighbors.Count(); k++)
		    if (t_face->m_neighbors[k]->m_belong_to_final != TrimmedFace::BELONG) {
			t_face->m_neighbors.Remove(k);
			k--;
		    }

		for (int k = 0; k < t_face->m_ssi_info.Count(); k++) {
		    SSICurveInfo info = t_face->m_ssi_info[k];
		    if (info.m_fi1 != i) {
			bu_log("Error: info.m_fi1(%d) != i(%d)\n", info.m_fi1, i);
			continue;
		    }
		    for (int l = 0; l < trimmedfaces[info.m_fi2].Count(); l++) {
			TrimmedFace* another_face = trimmedfaces[info.m_fi2][l];
			if (another_face->m_belong_to_final != TrimmedFace::BELONG)
			    continue;
			int m;
			for (m = 0; m < another_face->m_ssi_info.Count(); m++)
			    if (another_face->m_ssi_info[m].m_fi2 == i && another_face->m_ssi_info[m].m_ci == info.m_ci)
				break;
			if (m != another_face->m_ssi_info.Count()) {
			    if (t_face->m_neighbors.Search(another_face) == -1)
				t_face->m_neighbors.Append(another_face);
			    if (another_face->m_neighbors.Search(t_face) == -1)
				another_face->m_neighbors.Append(t_face);
			}
		    }
		}
#endif
	    }
	}
    }

#if USE_CONNECTIVITY_GRAPH
    if (DEBUG_BREP_BOOLEAN) {
	bu_log("The new connectivity graph for the final brep structure.");
	for (int i = 0; i < facecount1 + facecount2; i++) {
	    for (int j = 0; j < trimmedfaces[i].Count(); j++) {
		if (trimmedfaces[i][j]->m_belong_to_final != TrimmedFace::BELONG)
		    continue;
		bu_log("\nFace[%p]'s neighbors:", trimmedfaces[i][j]);
		for (int k = 0; k < trimmedfaces[i][j]->m_neighbors.Count(); k++)
		    bu_log(" %p", trimmedfaces[i][j]->m_neighbors[k]);
	    }
	}
    }
#endif

    for (int i = 0; i < facecount1 + facecount2; i++)
	for (int j = 0; j < trimmedfaces[i].Count(); j++)
	    if (trimmedfaces[i][j]) {
		delete trimmedfaces[i][j];
		trimmedfaces[i][j] = NULL;
	    }

    brepO->ShrinkSurfaces();
    brepO->Compact();

    // Check IsValid() and output the message.
    ON_wString ws;
    ON_TextLog log(ws);
    brepO->IsValid(&log);
    bu_log(ON_String(ws).Array());

    for (int i = 0; i < surf_treeA.Count(); i++)
	delete surf_treeA[i];
    for (int i = 0; i < surf_treeB.Count(); i++)
	delete surf_treeB[i];
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
