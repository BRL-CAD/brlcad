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


struct TrimmedFace {
    ON_SimpleArray<ON_Curve*> outerloop;
    ON_SimpleArray<ON_Curve*> innerloop;
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
    int m_t;		// param on the loop curve
    int m_type;		// which intersection curve
    int m_rank;		// rank on the chain
    int m_t_for_rank;	// param on the SSI curve
    bool m_in_out;	// dir is going inside(0)/outside(1)
    int m_pos;		// between curve[m_pos] and curve[m_pos+1]
			// after the outerloop is splitted
};


int
compare_t(IntersectPoint* const *a, IntersectPoint* const *b)
{
    if ((*a)->m_seg != (*b)->m_seg)
	return (*a)->m_seg - (*b)->m_seg;
    return (*a)->m_t - (*b)->m_t;
}


int
compare_for_rank(IntersectPoint* const *a, IntersectPoint* const *b)
{
    return (*a)->m_t_for_rank - (*b)->m_t_for_rank;
}


int
split_trimmed_face(ON_SimpleArray<TrimmedFace*> &out, const TrimmedFace *in, const ON_SimpleArray<ON_Curve*> &curves)
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

    ON_SimpleArray<IntersectPoint> intersect;
    ON_SimpleArray<bool> have_intersect(curves.Count());
    for (int i = 0; i < curves.Count(); i++)
	have_intersect[i] = false;

    for (int i = 0; i < in->outerloop.Count(); i++) {
	for (int j = 0; j < curves.Count(); j++) {
	    ON_SimpleArray<ON_X_EVENT> x_event;
	    ON_Intersect(in->outerloop[i], curves[j], x_event);
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
    for (int i = 0; i < curves.Count(); i++) {
	// XXX: There might be a special case that a curve has no intersection
	// with the outerloop, and its bounding box is inside the outer loop's
	// bbox, but it's actually outside the outer loop.
	// We should use a similar machanism like OverlapEvent::IsCurveCompletelyIn()
	// (See libbrep/intersect.cpp)
	if (!have_intersect[i]) {
	    ON_BoundingBox bbox_outerloop;
	    for (int j = 0; j < in->outerloop.Count(); j++) {
		bbox_outerloop.Union(in->outerloop[j]->BoundingBox());
	    }
	    if (bbox_outerloop.Includes(curves[i]->BoundingBox())) {
		if (curves[i]->IsClosed()) {
		    TrimmedFace *newface = in->Duplicate();
		    newface->innerloop.Append(curves[i]);
		    out.Append(newface);
		    newface = new TrimmedFace();
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
	    intersect[i].m_in_out = false; // in
	} else {
	    intersect[i].m_in_out = true; // out
	}
    }

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
	    curve_on_loop->Split(isect_pt->m_t, left, curve_on_loop);
	    if (left != NULL)
		outerloop.Append(left);
	    else
		bu_log("Split failed.\n");
	    sorted_pointers[isect_iter]->m_pos = outerloop.Count() - 1;
	}
	outerloop.Append(curve_on_loop);
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
	if (q->m_t < p->m_t || q->m_pos < p->m_pos) {
	    bu_log("stack error or sort failure.\n");
	    continue;
	}
	if (q->m_type != p->m_type) {
	    s.push(i);
	    continue;
	} else if (q->m_rank - p->m_rank == 1 && q->m_in_out == false && p->m_in_out == true) {
	    s.pop();
	} else if (p->m_rank - q->m_rank == 1 && p->m_in_out == false && q->m_in_out == true) {
	    s.pop();
	} else {
	    s.push(i);
	    continue;
	}

	// need to form a new loop
	ON_SimpleArray<ON_Curve*> newloop;
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

	// Append a trimmed face with newloop as its outerloop
	TrimmedFace *newface = new TrimmedFace();
	newface->face = in->face;
	newface->outerloop.Append(newloop.Count(), newloop.Array());
    }

    if (out.Count() == 0) {
	out.Append(in->Duplicate());
    }
    bu_log("Split to %d faces.\n", out.Count());
    return 0;
}


void
add_elements(ON_Brep *brep, ON_BrepFace &face, ON_SimpleArray<ON_Curve*> &loop, ON_BrepLoop::TYPE loop_type)
{
    if (!loop.Count())
	return;

    ON_BrepLoop &breploop = brep->NewLoop(loop_type, face);
    int start_index = brep->m_V.Count();
    for (int k = 0; k < loop.Count(); k++) {
	int ti = brep->AddTrimCurve(loop[k]);
	ON_2dPoint start = loop[k]->PointAtStart(), end = loop[k]->PointAtEnd();
	ON_BrepVertex& start_v = k > 0 ? brep->m_V[brep->m_V.Count() - 1] :
	    brep->NewVertex(face.SurfaceOf()->PointAt(start.x, start.y), 0.0);
	ON_BrepVertex& end_v = loop[k]->IsClosed() ? start_v :
	    brep->NewVertex(face.SurfaceOf()->PointAt(end.x, end.y), 0.0);
	if (k == loop.Count() - 1) {
	    if (!loop[k]->IsClosed())
		brep->m_V.Remove(brep->m_V.Count() - 1);
	    end_v = brep->m_V[start_index];
	}
	int start_idx = start_v.m_vertex_index;
	int end_idx = end_v.m_vertex_index;

	ON_NurbsCurve *c3d = ON_NurbsCurve::New();
	// First, try the ON_Surface::Pushup() method.
	// If Pushup() does not succeed, use sampling method.
	ON_Curve *curve_pt = face.SurfaceOf()->Pushup(*(loop[k]), 1e-3);
	if (curve_pt) {
	    curve_pt->GetNurbForm(*c3d);
	    delete curve_pt;
	} else if (loop[k]->SpanCount() == 2) {
	    // A closed curve with two control points
	    // TODO: Sometimes we need a singular trim.
	    ON_3dPointArray ptarray(101);
	    for (int l = 0; l <= 100; l++) {
		ON_3dPoint pt2d;
		pt2d = loop[k]->PointAt(loop[k]->Domain().NormalizedParameterAt(l/100.0));
		ptarray.Append(face.SurfaceOf()->PointAt(pt2d.x, pt2d.y));
	    }
	    ON_PolylineCurve polycurve(ptarray);
	    polycurve.GetNurbForm(*c3d);
	} else {
	    loop[k]->GetNurbForm(*c3d);
	    c3d->ChangeDimension(3);
	    for (int l = 0; l < c3d->SpanCount(); l++) {
		ON_3dPoint pt2d;
		c3d->GetCV(l, pt2d);
		ON_3dPoint pt3d = face.SurfaceOf()->PointAt(pt2d.x, pt2d.y);
		c3d->SetCV(l, pt3d);
	    }
	}
	brep->AddEdgeCurve(c3d);
	ON_BrepEdge &edge = brep->NewEdge(brep->m_V[start_idx], brep->m_V[end_idx],
	    brep->m_C3.Count() - 1, (const ON_Interval *)0, MAX_FASTF);
	ON_BrepTrim &trim = brep->NewTrim(edge, 0, breploop, ti);
	trim.m_tolerance[0] = trim.m_tolerance[1] = MAX_FASTF;

	// TODO: Deal with split seam trims to pass ON_Brep::IsValid()
    }
    if (brep->m_V.Count() < brep->m_V.Capacity())
	brep->m_V[brep->m_V.Count()].m_ei.Empty();
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
			     events) <= 0)
		continue;
	    ON_SimpleArray<ON_Curve*> curve_uv, curve_st;
	    for (int k = 0; k < events.Count(); k++) {
		curve_uv.Append(events[k].m_curveA);
		curve_st.Append(events[k].m_curveB);
		// Set m_curveA and m_curveB to NULL, in case that they are
		// deleted by ~ON_SSX_EVENT().
		events[k].m_curveA = events[k].m_curveB = NULL;
	    }
	    curvesarray[i].Append(curve_uv.Count(), curve_uv.Array());
	    curvesarray[facecount1 + j].Append(curve_st.Count(), curve_st.Array());
	}
    }

    // split the surfaces with the intersection curves
    for (int i = 0; i < facecount1 + facecount2; i++) {
	ON_SimpleArray<ON_Curve*> innercurves, outercurves;
	const ON_BrepFace &face = i < facecount1 ? brepA->m_F[i] : brepB->m_F[i - facecount1];
	const ON_Brep *brep = i < facecount1 ? brepA : brepB;
	const ON_SimpleArray<int> &loopindex = face.m_li;
	for (int j = 0; j < loopindex.Count(); j++) {
	    const ON_BrepLoop &loop = brep->m_L[loopindex[j]];
	    const ON_SimpleArray<int> &trimindex = loop.m_ti;
	    for (int k = 0; k < trimindex.Count(); k++) {
		ON_Curve *curve2d = brepA->m_C2[brep->m_T[trimindex[k]].m_c2i];
		if (j == 0) {
		    outercurves.Append(curve2d->Duplicate());
		} else {
		    innercurves.Append(curve2d->Duplicate());
		}
	    }
	}
	ON_SimpleArray<TrimmedFace*> trimmedfaces;
	TrimmedFace *first = new TrimmedFace();
	first->face = &face;
	first->innerloop = innercurves;
	first->outerloop = outercurves;
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
	    ON_BrepLoop &loop = brepO->m_L[brepO->m_L.Count() - 1];
	    add_elements(brepO, new_face, trimmedfaces[j]->innerloop, ON_BrepLoop::inner);

	    new_surf->SetDomain(0, loop.m_pbox.m_min.x, loop.m_pbox.m_max.x);
	    new_surf->SetDomain(1, loop.m_pbox.m_min.y, loop.m_pbox.m_max.y);
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
