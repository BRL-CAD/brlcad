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


int
curve_intersect(const ON_NurbsCurve *curveA,
		const ON_NurbsCurve *curveB,
		ON_3dPointArray *intersect,
		ON_SimpleArray<std::pair<int, int> > *CV,
		ON_SimpleArray<std::pair<double, double> > *parameter)
{
    int countA = curveA->CVCount();
    int countB = curveB->CVCount();
    for (int i = 0; i < countA - 1; i++) {
	ON_3dPoint fromA, toA;
	curveA->GetCV(i, fromA);
	curveA->GetCV(i + 1, toA);
	ON_Line lineA(fromA, toA);
	for (int j = 0; j < countB - 1; j++) {
	    ON_3dPoint fromB, toB;
	    curveB->GetCV(j, fromB);
	    curveB->GetCV(j + 1, toB);
	    ON_Line lineB(fromB, toB);
	    double tA, tB;
	    if (ON_Intersect(lineA, lineB, &tA, &tB) != true)
		continue;
	    if (tA >= 0.0 && tA <= 1.0 && tB >= 0.0 && tB <= 1.0) {
		intersect->Append(lineA.PointAt(tA));
		CV->Append(std::make_pair(i, j));
		parameter->Append(std::make_pair(tA, tB));
	    }
	}
    }
    return 0;
}


int
split_curve(ON_NurbsCurve *out1, ON_NurbsCurve *out2, const ON_NurbsCurve *curve, const int CVindex, const double t)
{
    if (out1 == NULL || out2 == NULL)
	return -1;

    // Split the curve using polyline curves.
    ON_3dPointArray pts1, pts2;
    for (int i = 0; i <= CVindex; i++) {
	ON_3dPoint point;
	curve->GetCV(i, point);
	pts1.Append(point);
    }
    ON_3dPoint start, end;
    curve->GetCV(CVindex, start);
    curve->GetCV(CVindex + 1, end);
    ON_Line line(start, end);
    pts1.Append(line.PointAt(t));
    pts2.Append(line.PointAt(t));
    for (int i = CVindex + 1; i < curve->CVCount(); i++) {
	ON_3dPoint point;
	curve->GetCV(i, point);
	pts2.Append(point);
    }

    ON_PolylineCurve poly1(pts1), poly2(pts2);
    poly1.GetNurbForm(*out1);
    poly2.GetNurbForm(*out2);
    return 0;
}


struct TrimmedFace {
    ON_SimpleArray<ON_NurbsCurve*> outerloop;
    ON_SimpleArray<ON_NurbsCurve*> innerloop;
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
    ON_3dPoint m_pt;
    int m_seg;
    int m_t;
    int m_type;
    int m_rank;
    int m_seg_for_rank;
    int m_t_for_rank;
    bool m_in_out;
    std::list<ON_3dPoint>::iterator m_pos;
};


int
compare_seg_t(IntersectPoint* const *a, IntersectPoint* const *b)
{
    if ((*a)->m_seg != (*b)->m_seg)
	return (*a)->m_seg - (*b)->m_seg;
    return (*a)->m_t - (*b)->m_t;
}


int
compare_for_rank(IntersectPoint* const *a, IntersectPoint* const *b)
{
    if ((*a)->m_seg_for_rank != (*b)->m_seg_for_rank)
	return (*a)->m_seg_for_rank - (*b)->m_seg_for_rank;
    return (*a)->m_t_for_rank - (*b)->m_t_for_rank;
}


int
split_trimmed_face(ON_SimpleArray<TrimmedFace*> &out, const TrimmedFace *in, const ON_SimpleArray<ON_NurbsCurve*> &curves)
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

    int CVCount_sum = 0;
    for (int i = 0; i < in->outerloop.Count(); i++) {
	for (int j = 0; j < curves.Count(); j++) {
	    ON_3dPointArray intersect_pt;
	    ON_SimpleArray<std::pair<int, int> > CV;
	    ON_SimpleArray<std::pair<double, double> > parameter;
	    curve_intersect(in->outerloop[i], curves[j], &intersect_pt, &CV, &parameter);
	    for (int k = 0; k < intersect_pt.Count(); k++) {
		IntersectPoint tmp_pt;
		tmp_pt.m_pt = intersect_pt[k];
		tmp_pt.m_seg = CVCount_sum + CV[k].first;
		tmp_pt.m_t = (int)parameter[k].first;
		tmp_pt.m_type = j;
		tmp_pt.m_seg_for_rank = CV[k].second;
		tmp_pt.m_t_for_rank = (int)parameter[k].second;
		intersect.Append(tmp_pt);
	    }
	    if (intersect_pt.Count())
		have_intersect[j] = true;
	}
	CVCount_sum += in->outerloop[i]->CVCount();
    }

    // deal with the situations where there is no intersection
    for (int i = 0; i < curves.Count(); i++) {
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
    sorted_pointers.QuickSort(compare_seg_t);

    for (int i = 0; i < intersect.Count(); i++) {
	// We assume that the starting point is outside.
	if (intersect[i].m_rank % 2 == 0) {
	    intersect[i].m_in_out = false; // in
	} else {
	    intersect[i].m_in_out = true; // out
	}
    }

    std::list<ON_3dPoint> outerloop;
    int count = 0;
    int isect_iter = 0;
    for (int i = 0; i < in->outerloop.Count(); i++) {
	const ON_NurbsCurve *curve_on_loop = in->outerloop[i];
	for (int j = 0; j < curve_on_loop->CVCount(); j++) {
	    ON_3dPoint cvpt;
	    curve_on_loop->GetCV(j, cvpt);
	    outerloop.push_back(cvpt);
	    count++;
	    for (; isect_iter < sorted_pointers.Count() && sorted_pointers[isect_iter]->m_seg < count; isect_iter++) {
		outerloop.push_back(sorted_pointers[isect_iter]->m_pt);
		sorted_pointers[isect_iter]->m_pos = --outerloop.end();
	    }
	}
    }
    outerloop.push_back(in->outerloop[0]->PointAtStart());

    std::stack<int> s;
    s.push(0);
    for (int i = 1; i < sorted_pointers.Count(); i++) {
	if (s.empty()) {
	    s.push(i);
	    continue;
	}
	IntersectPoint *p = sorted_pointers[s.top()];
	IntersectPoint *q = sorted_pointers[i];
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
	if (compare_seg_t(&p, &q) > 0)
	    std::swap(p, q);
	ON_3dPointArray newloop;
	std::list<ON_3dPoint> newsegment;
	std::list<ON_3dPoint>::iterator iter;
	for (iter = p->m_pos; iter != q->m_pos; iter++) {
	    newloop.Append(*iter);
	}
	newloop.Append(q->m_pt);
	if (p->m_seg_for_rank < q->m_seg_for_rank) {
	    for (int j = p->m_seg_for_rank + 1; j <= q->m_seg_for_rank; j++) {
		ON_3dPoint cvpt;
		curves[p->m_type]->GetCV(j, cvpt);
		newloop.Append(cvpt);
		newsegment.push_back(cvpt);
	    }
	} else if (p->m_seg_for_rank > q->m_seg_for_rank) {
	    for (int j = p->m_seg_for_rank; j > q->m_seg_for_rank; j--) {
		ON_3dPoint cvpt;
		curves[p->m_type]->GetCV(j, cvpt);
		newloop.Append(cvpt);
		newsegment.push_back(cvpt);
	    }
	}
	newloop.Append(p->m_pt);

	// Append a surface with newloop as its outerloop
	ON_PolylineCurve polycurve(newloop);
	ON_NurbsCurve *nurbscurve = ON_NurbsCurve::New();
	polycurve.GetNurbForm(*nurbscurve);
	TrimmedFace *newface = new TrimmedFace();
	newface->face = in->face;
	newface->outerloop.Append(nurbscurve);

	// adjust the outerloop
	std::list<ON_3dPoint>::iterator first = p->m_pos;
	outerloop.erase(++first, q->m_pos);
	first = p->m_pos;
	outerloop.insert(++first, newsegment.begin(), newsegment.end());
    }

    // WIP
    // The follows commented out are out of date.
    /* if (sum == 0) {
	// no intersection points with the outerloop
	ON_BoundingBox bbox_outerloop;
	for (int i = 0; i < in->outerloop.Count(); i++) {
	    bbox_outerloop.Union(in->outerloop[i].BoundingBox());
	}
	if (bbox_outerloop.Includes(curve->BoundingBox())) {
	    if (curve->IsClosed()) {
		TrimmedFace *newface = in->Duplicate();
		newface->innerloop.Append(*curve);
		out.Append(newface);
		newface = new TrimmedFace();
		newface->face = in->face;
		newface->outerloop.Append(*curve);
		out.Append(newface);
		return 0;
	    }
	}
    } else if (sum >= 2) {
	// intersect the outerloop (TODO)
	// Now we only consider sum == 2 and the two intersection points are
	// not on the same curve of the outer loop.
	int start = -1;
	for (int i = 0; i < intersect.Count(); i++) {
	    if (intersect[i].Count() != 0) {
		start = i;
		break;
	    }
	}
	TrimmedFace *newface = new TrimmedFace();
	newface->face = in->face;
	ON_NurbsCurve curve1, curve2;
	split_curve(&curve1, &curve2, &(in->outerloop[start]), CV[start][0].first, parameter[start][0][0]);
	newface->outerloop.Append(curve2);
	int i;
	for (i = start + 1; i != start; i = (i+1)%intersect.Count()) {
	    if (intersect[i].Count() == 0) {
		newface->outerloop.Append(in->outerloop[i]);
	    } else {
		break;
	    }
	}
	ON_NurbsCurve curve3, curve4;
	split_curve(&curve3, &curve4, &(in->outerloop[i]), CV[i][0].first, parameter[i][0][0]);
	newface->outerloop.Append(curve3);
	newface->outerloop.Append(*curve);
	out.Append(newface);
	newface = new TrimmedFace();
	newface->face = in->face;
	newface->outerloop.Append(curve4);
	for (; i != start; i = (i+1)%intersect.Count()) {
	    if (intersect[i].Count() == 0) {
		newface->outerloop.Append(in->outerloop[i]);
	    }
	}
	newface->outerloop.Append(curve1);
	newface->outerloop.Append(*curve);
	out.Append(newface);
	return 0;
    }*/

    if (out.Count() == 0) {
	out.Append(in->Duplicate());
    }
    bu_log("Split to %d faces.\n", out.Count());
    return 0;
}


void
add_elements(ON_Brep *brep, ON_BrepFace &face, ON_SimpleArray<ON_NurbsCurve*> &loop, ON_BrepLoop::TYPE loop_type)
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
	} else if (loop[k]->CVCount() == 2) {
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
	    delete c3d;
	    c3d = loop[k]->Duplicate();
	    c3d->ChangeDimension(3);
	    for (int l = 0; l < loop[k]->CVCount(); l++) {
		ON_3dPoint pt2d;
		loop[k]->GetCV(l, pt2d);
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
    ON_SimpleArray<ON_NurbsCurve *> *curvesarray = new ON_SimpleArray<ON_NurbsCurve *> [facecount1 + facecount2];

    // calculate intersection curves
    for (int i = 0; i < facecount1; i++) {
	for (int j = 0; j < facecount2; j++) {
	    ON_ClassArray<ON_SSX_EVENT> events;
	    if (ON_Intersect(brepA->m_S[brepA->m_F[i].m_si],
			     brepB->m_S[brepB->m_F[j].m_si],
			     events) <= 0)
		continue;
	    ON_SimpleArray<ON_NurbsCurve *> curve_uv, curve_st;
	    for (int k = 0; k < events.Count(); k++) {
		ON_NurbsCurve *nurbscurve = ON_NurbsCurve::New();
		events[k].m_curveA->GetNurbForm(*nurbscurve);
		curve_uv.Append(nurbscurve);
		nurbscurve = ON_NurbsCurve::New();
		events[k].m_curveA->GetNurbForm(*nurbscurve);
		curve_st.Append(nurbscurve);
	    }
	    curvesarray[i].Append(curve_uv.Count(), curve_uv.Array());
	    curvesarray[facecount1 + j].Append(curve_st.Count(), curve_st.Array());
	}
    }

    // split the surfaces with the intersection curves
    for (int i = 0; i < facecount1 + facecount2; i++) {
	ON_SimpleArray<ON_NurbsCurve*> innercurves, outercurves;
	const ON_BrepFace &face = i < facecount1 ? brepA->m_F[i] : brepB->m_F[i - facecount1];
	const ON_Brep *brep = i < facecount1 ? brepA : brepB;
	const ON_SimpleArray<int> &loopindex = face.m_li;
	for (int j = 0; j < loopindex.Count(); j++) {
	    const ON_BrepLoop &loop = brep->m_L[loopindex[j]];
	    const ON_SimpleArray<int> &trimindex = loop.m_ti;
	    for (int k = 0; k < trimindex.Count(); k++) {
		ON_Curve *curve2d = brepA->m_C2[brep->m_T[trimindex[k]].m_c2i];
		ON_NurbsCurve *nurbscurve = ON_NurbsCurve::New();
		if (!curve2d->GetNurbForm(*nurbscurve))
		    continue;
		if (j == 0) {
		    outercurves.Append(nurbscurve);
		} else {
		    innercurves.Append(nurbscurve);
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
