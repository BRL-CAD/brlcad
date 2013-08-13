/*                  B O O L E A N . C P P
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
/** @file boolean.cpp
 *
 * Evaluate NURBS booleans (union, intersection and difference).
 *
 */

#include "common.h"

#include <assert.h>
#include <vector>
#include <stack>
#include <algorithm>
#include "bio.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"
#include "raytrace.h"

#define DEBUG_BREP_BOOLEAN 1
#define INTERSECTION_TOL 0.001


struct TrimmedFace {
    ON_SimpleArray<ON_Curve*> outerloop;
    std::vector<ON_SimpleArray<ON_Curve*> > innerloop;
    const ON_BrepFace *face;
    TrimmedFace *Duplicate() const
    {
	TrimmedFace *out = new TrimmedFace();
	out->face = face;
	out->outerloop = outerloop;
	out->innerloop = innerloop;
	return out;
    }
};


struct IntersectPoint {
    ON_3dPoint m_pt;	// 3D intersection point
    int m_seg;		// which curve of the loop
    double m_t;		// param on the loop curve
    int m_type;		// which intersection curve
    int m_rank;		// rank on the chain
    double m_t_for_rank;// param on the SSI curve
    enum {
	IN,
	OUT
    } m_in_out;		// dir is going inside/outside
    int m_pos;		// between curve[m_pos] and curve[m_pos+1]
			// after the outerloop is split
};


HIDDEN int
compare_t(IntersectPoint* const *a, IntersectPoint* const *b)
{
    if ((*a)->m_seg != (*b)->m_seg)
	return (*a)->m_seg - (*b)->m_seg;
    return (*a)->m_t - (*b)->m_t;
}


HIDDEN int
compare_for_rank(IntersectPoint* const *a, IntersectPoint* const *b)
{
    return (*a)->m_t_for_rank - (*b)->m_t_for_rank;
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
	    AppendToPolyCurve(segments[i], polycurve);
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
    if (ret && !polycurve->IsClosed()) {
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
	if (polycurve.TangentAt(x_event[i].m_a[0]).IsParallelTo(linecurve.m_line.Direction()))
	    count++;
    }

    return count % 2 ? 1 : 0;
}


HIDDEN void
link_curves(const ON_SimpleArray<ON_Curve*>& in, ON_SimpleArray<ON_Curve*>& out)
{
    // There might be two reasons why we need to link these curves.
    // 1) They are from intersections with two different surfaces.
    // 2) They are not continuous in the other surface's UV domain.

    ON_SimpleArray<ON_Curve*> tmp;
    for (int i = 0; i < in.Count(); i++) {
	tmp.Append(in[i]->Duplicate());
    }

    // As usual, we use a greedy approach.
    for (int i = 0; i < tmp.Count(); i++) {
	for (int j = 0; j < tmp.Count(); j++) {
	    if (tmp[i] == NULL || tmp[i]->IsClosed())
		break;

	    if (tmp[j] == NULL || tmp[j]->IsClosed() || j == i)
		continue;

	    ON_Curve *c1 = NULL, *c2 = NULL;
	    double dis;
	    // Link curves that share an end point.
	    if ((dis = tmp[i]->PointAtStart().DistanceTo(tmp[j]->PointAtEnd())) < INTERSECTION_TOL) {
		// end -- start -- end -- start
		c1 = tmp[j];
		c2 = tmp[i];
	    } else if ((dis = tmp[i]->PointAtEnd().DistanceTo(tmp[j]->PointAtStart())) < INTERSECTION_TOL) {
		// start -- end -- start -- end
		c1 = tmp[i];
		c2 = tmp[j];
	    } else if ((dis = tmp[i]->PointAtStart().DistanceTo(tmp[j]->PointAtStart())) < INTERSECTION_TOL) {
		// end -- start -- start -- end
		if (tmp[i]->Reverse()) {
		    c1 = tmp[i];
		    c2 = tmp[j];
		}
	    } else if ((dis = tmp[i]->PointAtEnd().DistanceTo(tmp[j]->PointAtEnd())) < INTERSECTION_TOL) {
		// start -- end -- end -- start
		if (tmp[j]->Reverse()) {
		    c1 = tmp[i];
		    c2 = tmp[j];
		}
	    } else
		continue;

	    if (c1 != NULL && c2 != NULL) {
		ON_PolyCurve* polycurve = new ON_PolyCurve;
		AppendToPolyCurve(c1, *polycurve);
		if (dis > ON_ZERO_TOLERANCE)
		    AppendToPolyCurve(new ON_LineCurve(c1->PointAtEnd(), c2->PointAtStart()), *polycurve);
		AppendToPolyCurve(c2, *polycurve);
		tmp[i] = polycurve;
		tmp[j] = NULL;
	    }

	    // Check whether tmp[i] is closed within a tolerance
	    if (tmp[i]->PointAtStart().DistanceTo(tmp[i]->PointAtEnd()) < INTERSECTION_TOL && !tmp[i]->IsClosed()) {
		// make IsClosed() true
		c1 = tmp[i];
		c2 = new ON_LineCurve(tmp[i]->PointAtEnd(), tmp[i]->PointAtStart());
		ON_PolyCurve* polycurve = new ON_PolyCurve;
		AppendToPolyCurve(c1, *polycurve);
		AppendToPolyCurve(c2, *polycurve);
		tmp[i] = polycurve;
	    }
	}
    }

    // Append the remaining curves to out.
    for (int i = 0; i < tmp.Count(); i++)
	if (tmp[i] != NULL)
	    out.Append(tmp[i]);

    if (DEBUG_BREP_BOOLEAN)
	bu_log("link_curves(): %d curves remaining.\n", out.Count());
}


HIDDEN int
split_trimmed_face(ON_SimpleArray<TrimmedFace*> &out, const TrimmedFace *in, const ON_SimpleArray<ON_Curve*> &curves_in)
{
    /* We followed the algorithms described in:
     * S. Krishnan, A. Narkhede, and D. Manocha. BOOLE: A System to Compute
     * Boolean Combinations of Sculptured Solids. Technical Report TR95-008,
     * Department of Computer Science, University of North Carolina, 1995.
     * Appendix B: Partitioning a Simple Polygon using Non-Intersecting
     * Chains.
     */

    if (curves_in.Count() == 0) {
	// No curve, no splitting
	out.Append(in->Duplicate());
	return 0;
    }

    ON_SimpleArray<ON_Curve*> curves;
    link_curves(curves_in, curves);

    ON_SimpleArray<IntersectPoint> intersect;
    ON_SimpleArray<bool> have_intersect(curves.Count());
    for (int i = 0; i < curves.Count(); i++)
	have_intersect[i] = false;

    for (int i = 0; i < in->outerloop.Count(); i++) {
	for (int j = 0; j < curves.Count(); j++) {
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(in->outerloop[i], curves[j], x_event, INTERSECTION_TOL);
	    for (int k = 0; k < x_event.Count(); k++) {
		if (x_event[k].m_type == ON_X_EVENT::ccx_overlap)
		    continue;	// Deal with it later
		IntersectPoint tmp_pt;
		tmp_pt.m_pt = x_event[k].m_A[0];
		tmp_pt.m_seg = i;
		tmp_pt.m_t = x_event[k].m_a[0];
		tmp_pt.m_type = j;
		tmp_pt.m_t_for_rank = x_event[k].m_b[0];
		intersect.Append(tmp_pt);
	    }
	    if (x_event.Count())
		have_intersect[j] = true;
	}
    }

    // deal with the situations where there is no intersection
    std::vector<ON_SimpleArray<ON_Curve*> > innerloops;
    for (int i = 0; i < curves.Count(); i++) {
	if (!have_intersect[i]) {
	    // The start point cannot be on the boundary of the loop, because
	    // there is no intersections between curves[i] and the loop.
	    if (IsPointInsideLoop(curves[i]->PointAtStart(), in->outerloop)) {
		bu_log("*********inside loop.\n");
		if (curves[i]->IsClosed()) {
		    ON_SimpleArray<ON_Curve*> iloop;
		    iloop.Append(curves[i]);
		    innerloops.push_back(iloop);
		    TrimmedFace *newface = new TrimmedFace();
		    newface->face = in->face;
		    newface->outerloop.Append(curves[i]);
		    out.Append(newface);
		}
	    }
	}
    }

    // rank these intersection points
    ON_SimpleArray<ON_SimpleArray<IntersectPoint*>*> pts_on_curves(curves.Count());
    ON_SimpleArray<IntersectPoint*> sorted_pointers;
    for (int i = 0; i < curves.Count(); i++)
	pts_on_curves[i] = new ON_SimpleArray<IntersectPoint*>();
    for (int i = 0; i < intersect.Count(); i++) {
	pts_on_curves[intersect[i].m_type]->Append(&(intersect[i]));
	sorted_pointers.Append(&(intersect[i]));
    }
    for (int i = 0; i < curves.Count(); i++) {
	pts_on_curves[i]->QuickSort(compare_for_rank);
	for (int j = 0; j < pts_on_curves[i]->Count(); j++)
	    (*pts_on_curves[i])[j]->m_rank = j;
    }
    for (int i = 0; i < curves.Count(); i++) {
	delete pts_on_curves[i];
    }
    sorted_pointers.QuickSort(compare_t);

    for (int i = 0; i < intersect.Count(); i++) {
	// We assume that the starting point is outside.
	if (intersect[i].m_rank % 2 == 0) {
	    intersect[i].m_in_out = IntersectPoint::IN;
	} else {
	    intersect[i].m_in_out = IntersectPoint::OUT;
	}
    }

    // Split the outer loop.
    ON_SimpleArray<ON_Curve*> outerloop;
    int isect_iter = 0;
    for (int i = 0; i < in->outerloop.Count(); i++) {
	ON_Curve *curve_on_loop = in->outerloop[i]->Duplicate();
	if (curve_on_loop == NULL) {
	    bu_log("ON_Curve::Duplicate() failed.\n");
	    continue;
	}
	for (; isect_iter < sorted_pointers.Count() && sorted_pointers[isect_iter]->m_seg == i; isect_iter++) {
	    const IntersectPoint* isect_pt = sorted_pointers[isect_iter];
	    ON_Curve* left = NULL;
	    if (curve_on_loop) {
		if (ON_NearZero(isect_pt->m_t - curve_on_loop->Domain().Max())) {
		    // Don't call Split(), which may fail when the point is
		    // at the ends.
		    left = curve_on_loop;
		    curve_on_loop = NULL;
		} else if (!ON_NearZero(isect_pt->m_t, curve_on_loop->Domain().Min()))
		    curve_on_loop->Split(isect_pt->m_t, left, curve_on_loop);
	    }
	    if (left != NULL)
		outerloop.Append(left);
	    else {
		bu_log("Split failed.\n");
		if (curve_on_loop) {
		    bu_log("Domain: [%lf, %lf]\n", curve_on_loop->Domain().Min(), curve_on_loop->Domain().Max());
		    bu_log("m_t: %lf\n", isect_pt->m_t);
		}
	    }
	    sorted_pointers[isect_iter]->m_pos = outerloop.Count() - 1;
	}
	if (curve_on_loop)
	    outerloop.Append(curve_on_loop);
    }

    // Append the first element at the last to handle some special cases.
    if (sorted_pointers.Count()) {
	intersect.Append(*sorted_pointers[0]);
	intersect.Last()->m_seg += in->outerloop.Count();
	sorted_pointers.Append(intersect.Last());
	for (int i = 0; i <= sorted_pointers[0]->m_pos; i++) {
	    ON_Curve* dup = outerloop[i]->Duplicate();
	    if (dup != NULL) {
		outerloop.Append(dup);
	    }
	    else {
		bu_log("ON_Curve::Duplicate() failed.\n");
		outerloop.Append(outerloop[i]);
	    }
	}
	intersect.Last()->m_pos = outerloop.Count() - 1;
    }

    std::stack<int> s;
    s.push(0);

    for (int i = 1; i < sorted_pointers.Count(); i++) {
	if (s.empty()) {
	    s.push(i);
	    continue;
	}
	IntersectPoint *p = sorted_pointers[s.top()];
	IntersectPoint *q = sorted_pointers[i];
	if (compare_t(&p, &q) > 0 || q->m_pos < p->m_pos) {
	    bu_log("stack error or sort failure.\n");
	    continue;
	}
	if (q->m_type != p->m_type) {
	    s.push(i);
	    continue;
	} else if (q->m_rank - p->m_rank == 1 && q->m_in_out == IntersectPoint::OUT && p->m_in_out == IntersectPoint::IN) {
	    s.pop();
	} else if (p->m_rank - q->m_rank == 1 && p->m_in_out == IntersectPoint::OUT && q->m_in_out == IntersectPoint::IN) {
	    s.pop();
	} else {
	    s.push(i);
	    continue;
	}

	// need to form a new loop
	ON_SimpleArray<ON_Curve*> newloop;
	int curve_count = q->m_pos - p->m_pos;
	for (int j = p->m_pos + 1; j <= q->m_pos; j++) {
	    newloop.Append(outerloop[j]);
	}

	if (p->m_type != q->m_type) {
	    bu_log("Error: p->type != q->type\n");
	    continue;
	}

	// The curves on the outer loop is from p to q, so the curves on the
	// SSI curve should be from q to p (to form a loop)
	double t1 = p->m_t_for_rank, t2 = q->m_t_for_rank;
	bool need_reverse = true;
	if (t1 > t2) {
	    std::swap(t1, t2);
	    need_reverse = false;
	}
	ON_Curve* seg_on_SSI = sub_curve(curves[p->m_type], t1, t2);
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
	} else {
	    // Update the outerloop
	    outerloop[p->m_pos + 1] = rev_seg_on_SSI;
	    int k = p->m_pos + 2;
	    for (int j = q->m_pos + 1; j < outerloop.Count(); j++)
		outerloop[k++] = outerloop[j];
	    while (k < outerloop.Count()) {
		outerloop[outerloop.Count()-1] = NULL;
		outerloop.Remove();
	    }
	    // Update m_pos
	    for (int j = i + 1; j < sorted_pointers.Count(); j++)
		sorted_pointers[j]->m_pos -= curve_count - 1;
	}

	// Append a trimmed face with newloop as its outerloop
	// Don't add a face if the outerloop is not valid (e.g. degenerated).
	if (IsLoopValid(newloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *newface = new TrimmedFace();
	    newface->face = in->face;
	    newface->outerloop.Append(newloop.Count(), newloop.Array());
	    out.Append(newface);
	}
    }

    // Remove the duplicated segments before the first intersection point.
    if (sorted_pointers.Count()) {
	for (int i = 0; i <= sorted_pointers[0]->m_pos; i++)
	    outerloop.Remove(0);
    }

    if (out.Count() == 0) {
	out.Append(in->Duplicate());
    } else {
	// The remaining part after splitting some parts out.
	if (IsLoopValid(outerloop, ON_ZERO_TOLERANCE)) {
	    TrimmedFace *newface = new TrimmedFace();
	    newface->face = in->face;
	    newface->outerloop = outerloop;
	    newface->innerloop = in->innerloop;
	    newface->innerloop.insert(newface->innerloop.end(), innerloops.begin(), innerloops.end());
	    out.Append(newface);
	}
    }

    bu_log("Split to %d faces.\n", out.Count());
    if (DEBUG_BREP_BOOLEAN) {
	for (int i = 0; i < out.Count(); i++) {
	    bu_log("Trimmed Face %d:\n", i);
	    bu_log("outerloop:\n");
	    ON_wString wstr;
	    ON_TextLog textlog(wstr);
	    textlog.PushIndent();
	    for (int j = 0; j < out[i]->outerloop.Count(); j++) {
		textlog.Print("Curve %d\n", j);
		out[i]->outerloop[j]->Dump(textlog);
	    }
	    bu_log(ON_String(wstr).Array());

	    for (unsigned int j = 0; j < out[i]->innerloop.size(); j++) {
		bu_log("innerloop %d:\n", j);
		ON_wString wstr2;
		ON_TextLog textlog2(wstr2);
		textlog2.PushIndent();
		for (int k = 0; k < out[i]->innerloop[j].Count(); k++) {
		    textlog2.Print("Curve %d\n", k);
		    out[i]->innerloop[j][k]->Dump(textlog2);
		}
		bu_log(ON_String(wstr2).Array());
	    }
	}
    }

    return 0;
}


HIDDEN void
add_elements(ON_Brep *brep, ON_BrepFace &face, ON_SimpleArray<ON_Curve*> &loop, ON_BrepLoop::TYPE loop_type)
{
    if (!loop.Count())
	return;

    ON_BrepLoop &breploop = brep->NewLoop(loop_type, face);
    int start_count = brep->m_V.Count();
    for (int k = 0; k < loop.Count(); k++) {
	ON_Curve* c3d = NULL;
	// First, try the ON_Surface::Pushup() method.
	// If Pushup() does not succeed, use sampling method.
	c3d = face.SurfaceOf()->Pushup(*(loop[k]), INTERSECTION_TOL);
	if (c3d) {
	    brep->AddEdgeCurve(c3d);
	} else {
	    // TODO: Sometimes we need a singular trim.
	    ON_3dPointArray ptarray(101);
	    for (int l = 0; l <= 100; l++) {
		ON_3dPoint pt2d;
		pt2d = loop[k]->PointAt(loop[k]->Domain().ParameterAt(l/100.0));
		ptarray.Append(face.SurfaceOf()->PointAt(pt2d.x, pt2d.y));
	    }
	    c3d = new ON_PolylineCurve(ptarray);
	    brep->AddEdgeCurve(c3d);
	}

	int ti = brep->AddTrimCurve(loop[k]);
	ON_2dPoint start = loop[k]->PointAtStart(), end = loop[k]->PointAtEnd();
	int start_idx, end_idx;
	if (k > 0)
	    start_idx = brep->m_V.Count() - 1;
	else {
	    start_idx = brep->m_V.Count();
	    brep->NewVertex(face.SurfaceOf()->PointAt(start.x, start.y), 0.0);
	}
	if (c3d->IsClosed())
	    end_idx = start_idx;
	else {
	    end_idx = brep->m_V.Count();
	    brep->NewVertex(face.SurfaceOf()->PointAt(end.x, end.y), 0.0);
	}
	if (k == loop.Count() - 1) {
	    if (!c3d->IsClosed()) {
		brep->m_V.Remove();
		end_idx = start_count;
	    } else if (k) {
		brep->m_V.Remove();
		start_idx = end_idx = start_count;
		if (brep->m_E.Last() && brep->m_T.Last()) {
		    brep->m_E.Last()->m_vi[1] = start_count;
		    brep->m_T.Last()->m_vi[1] = start_count;
		    brep->m_V[start_count].m_ei.Append(brep->m_E.Count() - 1);
		}
	    }
	}

	ON_BrepEdge &edge = brep->NewEdge(brep->m_V[start_idx], brep->m_V[end_idx],
	    brep->m_C3.Count() - 1, (const ON_Interval *)0, MAX_FASTF);
	ON_BrepTrim &trim = brep->NewTrim(edge, 0, breploop, ti);
	trim.m_tolerance[0] = trim.m_tolerance[1] = MAX_FASTF;

	// TODO: Deal with split seam trims to pass ON_Brep::IsValid()
    }
    //if (brep->m_V.Count() < brep->m_V.Capacity())
	//brep->m_V[brep->m_V.Count()].m_ei.Empty();
}


int
ON_Boolean(ON_Brep* brepO, const ON_Brep* brepA, const ON_Brep* brepB, int UNUSED(operation))
{
    int facecount1 = brepA->m_F.Count();
    int facecount2 = brepB->m_F.Count();
    ON_SimpleArray<ON_Curve*> *curvesarray = new ON_SimpleArray<ON_Curve*> [facecount1 + facecount2];

    // calculate intersection curves
    for (int i = 0; i < facecount1; i++) {
	for (int j = 0; j < facecount2; j++) {
	    ON_ClassArray<ON_SSX_EVENT> events;
	    if (ON_Intersect(brepA->m_S[brepA->m_F[i].m_si],
			     brepB->m_S[brepB->m_F[j].m_si],
			     events,
			     INTERSECTION_TOL) <= 0)
		continue;
	    ON_SimpleArray<ON_Curve*> curve_uv, curve_st;
	    for (int k = 0; k < events.Count(); k++) {
		if (events[k].m_type == ON_SSX_EVENT::ssx_overlap
		    || events[k].m_type == ON_SSX_EVENT::ssx_tangent
		    || events[k].m_type == ON_SSX_EVENT::ssx_transverse) {
		    curve_uv.Append(events[k].m_curveA);
		    curve_st.Append(events[k].m_curveB);
		    // Set m_curveA and m_curveB to NULL, in case that they are
		    // deleted by ~ON_SSX_EVENT().
		    events[k].m_curveA = events[k].m_curveB = NULL;
		}
	    }
	    curvesarray[i].Append(curve_uv.Count(), curve_uv.Array());
	    curvesarray[facecount1 + j].Append(curve_st.Count(), curve_st.Array());
	}
    }

    // split the surfaces with the intersection curves
    for (int i = 0; i < facecount1 + facecount2; i++) {
	const ON_BrepFace &face = i < facecount1 ? brepA->m_F[i] : brepB->m_F[i - facecount1];
	const ON_Brep *brep = i < facecount1 ? brepA : brepB;
	const ON_SimpleArray<int> &loopindex = face.m_li;

	ON_SimpleArray<TrimmedFace*> trimmedfaces;
	TrimmedFace *first = new TrimmedFace();
	first->face = &face;

	for (int j = 0; j < loopindex.Count(); j++) {
	    const ON_BrepLoop &loop = brep->m_L[loopindex[j]];
	    const ON_SimpleArray<int> &trimindex = loop.m_ti;
	    ON_SimpleArray<ON_Curve*> iloop;
	    for (int k = 0; k < trimindex.Count(); k++) {
		ON_Curve *curve2d = brep->m_C2[brep->m_T[trimindex[k]].m_c2i];
		if (j == 0) {
		    first->outerloop.Append(curve2d->Duplicate());
		} else {
		    iloop.Append(curve2d->Duplicate());
		}
	    }
	    if (j != 0)
		first->innerloop.push_back(iloop);
	}

	split_trimmed_face(trimmedfaces, first, curvesarray[i]);

	/* TODO: Perform inside-outside test to decide whether the trimmed face
	 * should be used in the final b-rep structure or not.
	 * Different operations should be dealt with accordingly.
	 * Another solution is to use connectivity graphs which represents the
	 * topological structure of the b-rep. This can reduce time-consuming
	 * inside-outside tests.
	 * Here we just use all of these trimmed faces.
	 */
	for (int j = 0; j < trimmedfaces.Count(); j++) {
	    // Add the surfaces, faces, loops, trims, vertices, edges, etc.
	    // to the brep structure.
	    ON_Surface *new_surf = face.SurfaceOf()->Duplicate();
	    int surfindex = brepO->AddSurface(new_surf);
	    ON_BrepFace& new_face = brepO->NewFace(surfindex);

	    add_elements(brepO, new_face, trimmedfaces[j]->outerloop, ON_BrepLoop::outer);
	    // ON_BrepLoop &loop = brepO->m_L[brepO->m_L.Count() - 1];
	    for (unsigned int k = 0; k < trimmedfaces[j]->innerloop.size(); k++)
		add_elements(brepO, new_face, trimmedfaces[j]->innerloop[k], ON_BrepLoop::inner);

	    brepO->SetTrimIsoFlags(new_face);
	    brepO->FlipFace(new_face);
	}
    }

    // Check IsValid() and output the message.
    ON_wString ws;
    ON_TextLog log(ws);
    brepO->IsValid(&log);
    char *s = new char [ws.Length() + 1];
    for (int k = 0; k < ws.Length(); k++) {
	s[k] = ws[k];
    }
    s[ws.Length()] = 0;
    bu_log("%s", s);
    delete s;

    // WIP
    delete [] curvesarray;
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
