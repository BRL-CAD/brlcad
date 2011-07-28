/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2011 United States Government as represented by
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
/** @file opennurbs_ext.cpp
 *
 * Implementation of routines openNURBS left out.
 *
 */

#include "common.h"

#include "bio.h"
#include <assert.h>
#include <vector>

#include "tnt.h"
#include "jama_lu.h"

#include "vmath.h"
#include "bu.h"

#include "brep.h"
#include "dvec.h"

/* our interface header */
#include "./opennurbs_ext.h"


#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

#define BBOX_GROW 0.0

/// grows 3D BBox along each axis by this factor
#define BBOX_GROW_3D 0.1

/// arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#define TOL 0.000001

/// another arbitrary calculation tolerance (need to try VDIVIDE_TOL or VUNITIZE_TOL to tighten the bounds)
#define TOL2 0.00001


extern int brep_getSurfacePoint(const ON_3dPoint&, ON_2dPoint&, brlcad::BBNode*);

bool
ON_NearZero(double x, double tolerance) {
    return (x > -tolerance) && (x < tolerance);
}


namespace brlcad {

inline void
distribute(const int count, const ON_3dVector* v, double x[], double y[], double z[])
{
    for (int i = 0; i < count; i++) {
	x[i] = v[i].x;
	y[i] = v[i].y;
	z[i] = v[i].z;
    }
}


//--------------------------------------------------------------------------------
// CurveTree
CurveTree::CurveTree(ON_BrepFace* face) :
    m_face(face)
{
    m_root = initialLoopBBox();

    for (int li = 0; li < face->LoopCount(); li++) {
	bool innerLoop = (li > 0) ? true : false;
	ON_BrepLoop* loop = face->Loop(li);
	// for each trim
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    int adj_face_index = -99;
	    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[ti]];

	    if (trim.m_ei != -1) { // does not lie on a portion of a singular surface side
		ON_BrepEdge& edge = face->Brep()->m_E[trim.m_ei];
		switch (trim.m_type) {
		case ON_BrepTrim::unknown:
		    bu_log("ON_BrepTrim::unknown on Face:%d\n", face->m_face_index);
		    break;
		case ON_BrepTrim::boundary:
		    //bu_log("ON_BrepTrim::boundary on Face:%d\n", face->m_face_index);
		    break;
		case ON_BrepTrim::mated:
		    if (edge.m_ti.Count() == 2) {
			if (face->m_face_index == face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf()) {
			    adj_face_index = face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf();
			} else {
			    adj_face_index = face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf();
			}
		    } else {
			bu_log("Mated Edge should have 2 adjacent faces, right?  Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
		    }
		    break;
		case ON_BrepTrim::seam:
		    if (edge.m_ti.Count() == 2) {
			if ((face->m_face_index == face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf()) && (face->m_face_index == face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf())) {
			    adj_face_index = face->m_face_index;
			} else {
			    bu_log("Seamed Edge should have 1 faces sharing the trim so trim index should be one, right? Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
			    bu_log("Face(%d) has %d, %d trim indexes\n", face->m_face_index, face->Brep()->m_T[edge.m_ti[0]].FaceIndexOf(), face->Brep()->m_T[edge.m_ti[1]].FaceIndexOf());
			}
		    } else if (edge.m_ti.Count() == 1) {
			adj_face_index = face->m_face_index;
		    } else {
			bu_log("Seamed Edge should have 1 faces sharing the trim so trim index should be one, right? Face(%d) has %d trim indexes\n", face->m_face_index, edge.m_ti.Count());
		    }
		    break;
		case ON_BrepTrim::singular:
		    bu_log("ON_BrepTrim::singular on Face:%d\n", face->m_face_index);
		    break;
		case ON_BrepTrim::crvonsrf:
		    bu_log("ON_BrepTrim::crvonsrf on Face:%d\n", face->m_face_index);
		    break;
		case ON_BrepTrim::ptonsrf:
		    bu_log("ON_BrepTrim::ptonsrf on Face:%d\n", face->m_face_index);
		    break;
		case ON_BrepTrim::slit:
		    bu_log("ON_BrepTrim::slit on Face:%d\n", face->m_face_index);
		    break;
		default:
		    bu_log("ON_BrepTrim::default on Face:%d\n", face->m_face_index);
		}
	    }
	    const ON_Curve* trimCurve = trim.TrimCurveOf();
	    double min, max;
	    (void) trimCurve->GetDomain(&min, &max);
	    ON_Interval t(min, max);

	    TRACE("need to subdivide");
	    // divide on param interval

	    if (!trimCurve->IsLinear()) {
		int knotcnt = trimCurve->SpanCount();
		double *knots = new double[knotcnt + 1];

		trimCurve->GetSpanVector(knots);
		std::list<double> splitlist;
		for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
		    ON_Interval range(knots[knot_index - 1], knots[knot_index]);

		    getHVTangents(trimCurve, range, splitlist);
		}
		for (std::list<double>::iterator l = splitlist.begin(); l != splitlist.end(); l++) {
		    double xmax = *l;
		    if (!NEAR_EQUAL(xmax, min, TOL)) {
			m_root->addChild(subdivideCurve(trimCurve, adj_face_index, min, xmax, innerLoop, 0));
		    }
		    min = xmax;
		}
		delete [] knots;
	    } else {
		int knotcnt = trimCurve->SpanCount();
		double *knots = new double[knotcnt + 1];

		trimCurve->GetSpanVector(knots);
		for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
		    double xmax = knots[knot_index];
		    if (!NEAR_EQUAL(xmax, min, TOL)) {
			m_root->addChild(subdivideCurve(trimCurve, adj_face_index, min, xmax, innerLoop, 0));
		    }
		    min = xmax;
		}
		delete [] knots;
	    }

	    if (!NEAR_EQUAL(max, min, TOL)) {
		m_root->addChild(subdivideCurve(trimCurve, adj_face_index, min, max, innerLoop, 0));
	    }
	}
    }
    getLeaves(m_sortedX);
    m_sortedX.sort(sortX);
    getLeaves(m_sortedY);
    m_sortedY.sort(sortY);

    return;
}


CurveTree::~CurveTree()
{
    delete m_root;
}


BRNode*
CurveTree::getRootNode() const
{
    return m_root;
}


int
CurveTree::depth()
{
    return m_root->depth();
}


ON_2dPoint
CurveTree::getClosestPointEstimate(const ON_3dPoint& pt)
{
    return m_root->getClosestPointEstimate(pt);
}


ON_2dPoint
CurveTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v)
{
    return m_root->getClosestPointEstimate(pt, u, v);
}


void
CurveTree::getLeaves(std::list<BRNode*>& out_leaves)
{
    m_root->getLeaves(out_leaves);
}

void
CurveTree::getLeavesAbove(std::list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v)
{
    point_t bmin, bmax;
    double dist;
    for (std::list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
	BRNode* br = dynamic_cast<BRNode*>(*i);
	br->GetBBox(bmin, bmax);

	dist = TOL;//0.03*DIST_PT_PT(bmin, bmax);
	if (bmax[X]+dist < u[0])
	    continue;
	if (bmin[X]-dist < u[1]) {
	    if (bmax[Y]+dist > v[0]) {
		out_leaves.push_back(br);
	    }
	}
    }
}

void
CurveTree::getLeavesAbove(std::list<BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol)
{
    point_t bmin, bmax;
    for (std::list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
		BRNode* br = dynamic_cast<BRNode*>(*i);
		br->GetBBox(bmin, bmax);

		if (bmax[X]+tol < pt.x)
			continue;
		if (bmin[X]-tol < pt.x) {
			if (bmax[Y]+tol > pt.y) {
			out_leaves.push_back(br);
			}
		}
    }
}

void
CurveTree::getLeavesRight(std::list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v)
{
    point_t bmin, bmax;
    double dist;
    for (std::list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
	BRNode* br = dynamic_cast<BRNode*>(*i);
	br->GetBBox(bmin, bmax);

	dist = TOL;//0.03*DIST_PT_PT(bmin, bmax);
	if (bmax[Y]+dist < v[0])
	    continue;
	if (bmin[Y]-dist < v[1]) {
	    if (bmax[X]+dist > u[0]) {
		out_leaves.push_back(br);
	    }
	}
    }
}

void
CurveTree::getLeavesRight(std::list<BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol)
{
    point_t bmin, bmax;
    for (std::list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
	BRNode* br = dynamic_cast<BRNode*>(*i);
	br->GetBBox(bmin, bmax);

	if (bmax[Y]+tol < pt.y)
	    continue;
	if (bmin[Y]-tol < pt.y) {
	    if (bmax[X]+tol > pt.x) {
		out_leaves.push_back(br);
	    }
	}
    }
}

fastf_t
CurveTree::getVerticalTangent(const ON_Curve *curve, fastf_t min, fastf_t max)
{
    fastf_t mid;
    ON_3dVector tangent;
    bool tanmin;

    // first lets check end points
    tangent = curve->TangentAt(max);
    if (NEAR_ZERO(tangent.x, TOL2))
	return max;
    tangent = curve->TangentAt(min);
    if (NEAR_ZERO(tangent.x, TOL2))
	return min;

    tanmin = (tangent[X] < 0.0);
    while ((max-min) > TOL2) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (NEAR_ZERO(tangent[X], TOL2)) {
	    return mid;
	}
	if ((tangent[X] < 0.0) == tanmin) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}


fastf_t
CurveTree::getHorizontalTangent(const ON_Curve *curve, fastf_t min, fastf_t max)
{
    fastf_t mid;
    ON_3dVector tangent;
    bool tanmin;

    // first lets check end points
    tangent = curve->TangentAt(max);
    if (NEAR_ZERO(tangent.y, TOL2))
	return max;
    tangent = curve->TangentAt(min);
    if (NEAR_ZERO(tangent.y, TOL2))
	return min;

    tanmin = (tangent[Y] < 0.0);
    while ((max-min) > TOL2) {
	mid = (max + min)/2.0;
	tangent = curve->TangentAt(mid);
	if (NEAR_ZERO(tangent[Y], TOL2)) {
	    return mid;
	}
	if ((tangent[Y] < 0.0) == tanmin) {
	    min = mid;
	} else {
	    max = mid;
	}
    }
    return min;
}


bool
CurveTree::getHVTangents(const ON_Curve* curve, ON_Interval& t, std::list<fastf_t>& list)
{
    bool tanx1, tanx2, tanx_changed;
    bool tany1, tany2, tany_changed;
    bool tan_changed;
    ON_3dVector tangent1, tangent2;
    ON_3dPoint p1, p2;

    tangent1 = curve->TangentAt(t[0]);
    tangent2 = curve->TangentAt(t[1]);

    tanx1 = (tangent1[X] < 0.0);
    tanx2 = (tangent2[X] < 0.0);
    tany1 = (tangent1[Y] < 0.0);
    tany2 = (tangent2[Y] < 0.0);

    tanx_changed =(tanx1 != tanx2);
    tany_changed =(tany1 != tany2);

    tan_changed = tanx_changed || tany_changed;

    if (tan_changed) {
	if (tanx_changed && tany_changed) {//horz & vert simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0], midpoint);
	    ON_Interval right(midpoint, t[1]);
	    getHVTangents(curve, left, list);
	    getHVTangents(curve, right, list);
	    return true;
	} else if (tanx_changed) {//find horz
	    double x = getVerticalTangent(curve, t[0], t[1]);
	    list.push_back(x);
	} else { //find vert
	    double x = getHorizontalTangent(curve, t[0], t[1]);
	    list.push_back(x);
	}
    } else { // check point slope for change
	bool slopex, slopex_changed;
	bool slopey, slopey_changed;
	bool slope_changed;
	fastf_t xdelta, ydelta;

	p1 = curve->PointAt(t[0]);
	p2 = curve->PointAt(t[1]);

	xdelta = (p2[X] - p1[X]);
	slopex = (xdelta < 0.0);
	ydelta = (p2[Y] - p1[Y]);
	slopey = (ydelta < 0.0);

	if (NEAR_ZERO(xdelta, TOL) ||
	    NEAR_ZERO(ydelta, TOL)) {
	    return true;
	}

	slopex_changed = (slopex != tanx1);
	slopey_changed = (slopey != tany1);

	slope_changed = slopex_changed || slopey_changed;

	if (slope_changed) {  //2 horz or 2 vert changes simply split
	    double midpoint = (t[1]+t[0])/2.0;
	    ON_Interval left(t[0], midpoint);
	    ON_Interval right(midpoint, t[1]);
	    getHVTangents(curve, left, list);
	    getHVTangents(curve, right, list);
	    return true;
	}
    }
    return true;
}


BRNode*
CurveTree::curveBBox(const ON_Curve* curve, int adj_face_index, ON_Interval& t, bool isLeaf, bool innerTrim, const ON_BoundingBox& bb)
{
    BRNode* node;
    int vdot = 1;

    if (isLeaf) {
	TRACE("creating leaf: u(" << u.Min() << ", " << u.Max() << ") v(" << v.Min() << ", " << v.Max() << ")");
	node = new BRNode(curve, adj_face_index, bb, m_face, t, vdot, innerTrim);
    } else {
	node = new BRNode(bb);
    }

    return node;

}


BRNode*
CurveTree::initialLoopBBox()
{
    ON_BoundingBox bb;
    for (int i = 0; i < m_face->LoopCount(); i++) {
	ON_BrepLoop* loop = m_face->Loop(i);
	if (loop->m_type == ON_BrepLoop::outer) {
	    if (loop->GetBBox(bb[0], bb[1], 0)) {
		TRACE("BBox for Loop min<" << bb[0][0] << ", " << bb[0][1] ", " << bb[0][2] << ">");
		TRACE("BBox for Loop max<" << bb[1][0] << ", " << bb[1][1] ", " << bb[1][2] << ">");
	    }
	    break;
	}
    }
    BRNode* node = new BRNode(bb);
    return node;
}


BRNode*
CurveTree::subdivideCurve(const ON_Curve* curve, int adj_face_index, double min, double max, bool innerTrim, int divDepth)
{
    ON_Interval dom = curve->Domain();
    ON_3dPoint points[2];
    points[0] = curve->PointAt(min);
    points[1] = curve->PointAt(max);
    point_t minpt, maxpt;
    VSETALL(minpt, MAX_FASTF);
    VSETALL(maxpt, -MAX_FASTF);
    for (int i = 0; i < 2; i++)
	VMINMAX(minpt, maxpt, ((double*)points[i]));
    points[0]=ON_3dPoint(minpt);
    points[1]=ON_3dPoint(maxpt);
    ON_BoundingBox bb(points[0], points[1]);

    ON_Interval t(min, max);
    if (isLinear(curve, min, max) || divDepth >= BREP_MAX_LN_DEPTH) {
	double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
	point_t pnts[BREP_BB_CRV_PNT_CNT];
	ON_3dPoint pnt;
	for (int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	    pnt = curve->PointAt(min + delta*i);
	    VSET(pnts[i], pnt[0], pnt[1], pnt[2]);
	}
	pnt = curve->PointAt(max);
	VSET(pnts[BREP_BB_CRV_PNT_CNT-1], pnt[0], pnt[1], pnt[2]);

	VSETALL(minpt, MAX_FASTF);
	VSETALL(maxpt, -MAX_FASTF);
	for (int i = 0; i < BREP_BB_CRV_PNT_CNT; i++)
	    VMINMAX(minpt, maxpt, ((double*)pnts[i]));

	VMOVE(pnt, minpt);
	bb.Set(pnt, false);
	VMOVE(pnt, maxpt);
	bb.Set(pnt, true);
	return curveBBox(curve, adj_face_index, t, true, innerTrim, bb);
    }

    // else subdivide
    BRNode* parent = curveBBox(curve, adj_face_index, t, false, innerTrim, bb);
    double mid = (max+min)/2.0;
    BRNode* l = subdivideCurve(curve, adj_face_index, min, mid, innerTrim, divDepth+1);
    BRNode* r = subdivideCurve(curve, adj_face_index, mid, max, innerTrim, divDepth+1);
    parent->addChild(l);
    parent->addChild(r);
    return parent;
}


/**
 * Determine whether a given curve segment is linear
 */
bool
CurveTree::isLinear(const ON_Curve* curve, double min, double max)
{
    ON_3dVector tangent_start = curve->TangentAt(min);
    ON_3dVector tangent_end = curve->TangentAt(max);
    double vdot = tangent_start * tangent_end;
    if (vdot <  BREP_CURVE_FLATNESS)
	return false;

    ON_3dPoint pmin = curve->PointAt(min);
    ON_3dPoint pmax = curve->PointAt(max);

    const ON_Surface* surf = m_face->SurfaceOf();
    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    point_t a, b;
    VSET(a, u[0], v[0], 0.0);
    VSET(b, u[1], v[1], 0.0);
    double dd = DIST_PT_PT(a, b);
    double cd = DIST_PT_PT(pmin, pmax);

    if (cd > BREP_TRIM_SUB_FACTOR*dd)
	return false;

    double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
    ON_3dPoint points[BREP_BB_CRV_PNT_CNT];
    for (int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	points[i] = curve->PointAt(min + delta*i);
    }
    points[BREP_BB_CRV_PNT_CNT-1] = curve->PointAt(max);

    ON_3dVector A;
    ON_3dVector B;
    vdot = 1.0;
    A = points[BREP_BB_CRV_PNT_CNT-1] - points[0];
    A.Unitize();
    for (int i=1;i<BREP_BB_CRV_PNT_CNT-1;i++) {
	B = points[i] - points[0];
	B.Unitize();
	vdot = vdot * (A * B);
	if (vdot < BREP_CURVE_FLATNESS)
	    return false; //already failed
    }

    return vdot >= BREP_CURVE_FLATNESS;
}


//--------------------------------------------------------------------------------
// SurfaceTree
SurfaceTree::SurfaceTree(ON_BrepFace* face, bool removeTrimmed, int depthLimit)
    : m_removeTrimmed(removeTrimmed),
      m_face(face)
{
    // first, build the Curve Tree
    if (removeTrimmed)
	ctree = new CurveTree(m_face);
    else
	ctree = NULL;

    // build the surface bounding volume hierarchy
    const ON_Surface* surf = face->SurfaceOf();
    TRACE("Creating surface tree for: " << face->m_face_index);
    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    // Populate initial corner and normal arrays for use in
    // tree build
    ON_3dPoint corners[9];
    ON_3dVector normals[9];
    double uq = u.Length()*0.25;
    double vq = v.Length()*0.25;

    ///////////////////////////////////////////////////////////////////////
    ON_Plane frames[9];
    surf->FrameAt( u.Min(), v.Min(), frames[0]);
    surf->FrameAt( u.Max(), v.Min(), frames[1]);
    surf->FrameAt( u.Max(), v.Max(), frames[2]);
    surf->FrameAt( u.Min(), v.Max(), frames[3]);
    surf->FrameAt( u.Mid(), v.Mid(), frames[4]);
    surf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
    surf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
    surf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
    surf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);

    corners[0] = frames[0].origin;
    normals[0] = frames[0].zaxis;
    corners[1] = frames[1].origin;
    normals[1] = frames[1].zaxis;
    corners[2] = frames[2].origin;
    normals[2] = frames[2].zaxis;
    corners[3] = frames[3].origin;
    normals[3] = frames[3].zaxis;
    corners[4] = frames[4].origin;
    normals[4] = frames[4].zaxis;
    corners[5] = frames[5].origin;
    normals[5] = frames[5].zaxis;
    corners[6] = frames[6].origin;
    normals[6] = frames[6].zaxis;
    corners[7] = frames[7].origin;
    normals[7] = frames[7].zaxis;
    corners[8] = frames[8].origin;
    normals[8] = frames[8].zaxis;

    m_root = subdivideSurfaceByKnots(surf, u, v, frames, corners, normals, 0, depthLimit);
    m_root->BuildBBox();
    TRACE("u: [" << u[0] << ", " << u[1] << "]");
    TRACE("v: [" << v[0] << ", " << v[1] << "]");
    TRACE("m_root: " << m_root);
}


SurfaceTree::~SurfaceTree()
{
    delete ctree;
    delete m_root;
}


BBNode*
SurfaceTree::getRootNode() const
{
    return m_root;
}


int
SurfaceTree::depth()
{
    return m_root->depth();
}


ON_2dPoint
SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt)
{
    return m_root->getClosestPointEstimate(pt);
}


ON_2dPoint
SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v)
{
    return m_root->getClosestPointEstimate(pt, u, v);
}


void
SurfaceTree::getLeaves(std::list<BBNode*>& out_leaves)
{
    m_root->getLeaves(out_leaves);
}


const ON_Surface *
SurfaceTree::getSurface()
{
    return m_face->SurfaceOf();
}


int
SurfaceTree::getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv, const ON_3dPoint& from, double tolerance) const
{
    std::list<BBNode*> nodes;
    (void)m_root->getLeavesBoundingPoint(from, nodes);

    double min_dist = MAX_FASTF;
    ON_2dPoint curr_uv;
    bool found = false;

    std::list<BBNode*>::iterator i;
    for (i = nodes.begin(); i != nodes.end(); i++) {
	BBNode* node = (*i);
	if (::brep_getSurfacePoint(pt, curr_uv, node)) {
	    ON_3dPoint fp = m_face->SurfaceOf()->PointAt(curr_uv.x, curr_uv.y);
	    double dist = fp.DistanceTo(pt);
	    if (NEAR_ZERO(dist, BREP_SAME_POINT_TOLERANCE)) {
		uv = curr_uv;
		found = true;
		return 1; //close enough to same point so no sense in looking for one closer
	    } else if (NEAR_ZERO(dist, tolerance)) {
		if (dist < min_dist) {
		    uv = curr_uv;
		    min_dist = dist;
		    found = true; //within tolerance but may be a point closer so keep looking
		}
	    }
	}
    }
    if (found) {
	return 1;
    }
    return -1;
}

//static int bb_cnt=0;
BBNode*
SurfaceTree::surfaceBBox(const ON_Surface *localsurf, bool isLeaf, ON_3dPoint *m_corners, ON_3dVector *m_normals, const ON_Interval& u, const ON_Interval& v)
{
    point_t min, max, buffer;
    ON_BoundingBox bbox = localsurf->BoundingBox();

    VSETALL(buffer, BREP_EDGE_MISS_TOLERANCE);
    //VSETALL(min, MAX_FASTF);
    //VSETALL(max, -MAX_FASTF);
    //for (int i = 0; i < 9; i++) {
	//VMINMAX(min, max, ((double*)m_corners[i]));
	//bu_log("in c%d_%d sph %f %f %f %f\n", bb_cnt, i, m_corners[i].x, m_corners[i].y, m_corners[i].z, 1.0);
    //}

	//bu_log("in bb%d rpp %f %f %f %f %f %f\n", bb_cnt, min[0], max[0], min[1], max[1], min[2], max[2]);
    VMOVE(min,bbox.Min());
    VMOVE(max,bbox.Max());

	//bu_log("in b%d rpp %f %f %f %f %f %f\n", bb_cnt++, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
	//bu_log("in bc%d rpp %f %f %f %f %f %f\n", bb_cnt++, min[0], max[0], min[1], max[1], min[2], max[2]);

    // calculate the estimate point on the surface: i.e. use the point
    // on the surface defined by (u.Mid(), v.Mid()) as a heuristic for
    // finding the uv domain bounding a portion of the surface close
    // to an arbitrary model-space point (see
    // getClosestPointEstimate())
    ON_3dPoint estimate;
    ON_3dVector normal;
    estimate = m_corners[4];
    normal = m_normals[4];

    BBNode* node;
    if (isLeaf) {
/*	vect_t delta; */

	VSUB2(min, min, buffer);
	VADD2(max, max, buffer);
/*	VSUB2(delta, max, min);
	VSCALE(delta, delta, BBOX_GROW_3D);
	VSUB2(min, min, delta);
	VADD2(max, max, delta);
*/
	TRACE("creating leaf: u(" << u.Min() << ", " << u.Max() <<
	      ") v(" << v.Min() << ", " << v.Max() << ")");
	node = new BBNode(ctree, ON_BoundingBox(ON_3dPoint(min),
						ON_3dPoint(max)),
			  m_face,
			  u, v);
	node->prepTrims();

    } else {
	node = new BBNode(ctree, ON_BoundingBox(ON_3dPoint(min),
						ON_3dPoint(max)));
    }

    node->m_estimate = estimate;
    node->m_normal = normal;
    node->m_face = m_face;
    return node;
}


BBNode*
initialBBox(CurveTree* ctree, const ON_Surface* surf, const ON_BrepFace* face, const ON_Interval& u, const ON_Interval& v)
{
    ON_BoundingBox bb = surf->BoundingBox();
    BBNode* node = new BBNode(ctree, bb, face, u, v, false, false);
    ON_3dPoint estimate;
    ON_3dVector normal;
    if (!surf->EvNormal(surf->Domain(0).Mid(), surf->Domain(1).Mid(), estimate, normal)) {
	bu_bomb("Could not evaluate estimate point on surface");
    }
    node->m_estimate = estimate;
    node->m_normal = normal;
    node->m_face = face;
    return node;
}

BBNode*
SurfaceTree::subdivideSurfaceByKnots(const ON_Surface *localsurf,
	const ON_Interval& u,
	const ON_Interval& v,
	ON_Plane frames[],
	ON_3dPoint corners[],
	ON_3dVector normals[],
	int divDepth,
	int depthLimit)
{
	const ON_Surface* surf = m_face->SurfaceOf();
	ON_Interval usurf = surf->Domain(0);
	ON_Interval vsurf = surf->Domain(1);
	BBNode* parent = initialBBox(ctree, localsurf, m_face, u, v);
	BBNode* quads[4];
	int spanu_cnt = localsurf->SpanCount(0);
	int spanv_cnt = localsurf->SpanCount(1);
	double *spanu = NULL;
	double *spanv = NULL;
	spanu = new double[spanu_cnt+1];
	spanv = new double[spanv_cnt+1];
	localsurf->GetSpanVector(0,spanu);
	localsurf->GetSpanVector(1,spanv);

	///////////////////////////////////
	double uq = u.Length()*0.25;
	double vq = v.Length()*0.25;
	surf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
	surf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
	surf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
	surf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);

	corners[5] = frames[5].origin;
	normals[5] = frames[5].zaxis;
	corners[6] = frames[6].origin;
	normals[6] = frames[6].zaxis;
	corners[7] = frames[7].origin;
	normals[7] = frames[7].zaxis;
	corners[8] = frames[8].origin;
	normals[8] = frames[8].zaxis;

	if ((spanu_cnt > 1) && (spanv_cnt > 1)) {
		double usplit = spanu[(spanu_cnt+1)/2];
		double vsplit = spanv[(spanv_cnt+1)/2];
		ON_Interval firstu(u.Min(), usplit);
		ON_Interval secondu(usplit, u.Max());
		ON_Interval firstv(v.Min(), vsplit);
		ON_Interval secondv(vsplit, v.Max());

		ON_Surface *north = NULL;
		ON_Surface *south = NULL;
		ON_Surface *q2surf = NULL;
		ON_Surface *q3surf = NULL;
		ON_Surface *q1surf = NULL;
		ON_Surface *q0surf = NULL;

		ON_BoundingBox box = localsurf->BoundingBox();

		int dir = 1;
		localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);
		localsurf->Split(dir, vsplit, south, north);
		south->ClearBoundingBox();
		north->ClearBoundingBox();

		dir = 0;
		south->Split(dir, usplit, q0surf, q1surf);
		delete south;
		q0surf->ClearBoundingBox();
		q1surf->ClearBoundingBox();
		north->Split(dir, usplit, q3surf, q2surf);
		delete north;
		q3surf->ClearBoundingBox();
		q2surf->ClearBoundingBox();

		/*********************************************************************
		 * In order to avoid fairly expensive re-calculation of 3d points at
		 * uv coordinates, all values that are shared between children at
		 * the same depth of a surface subdivision are pre-computed and
		 * passed as paramters.
		 *
		 * The majority of these points are already evaluated in the process
		 * of testing whether a subdivision has produced a leaf node.  These
		 * values are in the normals and corners arrays and have index values
		 * corresponding to the values of the figure on the left below.  There
		 * are four other shared values that are precomputed in the sharedcorners
		 * and sharednormals arrays; their index values in those arrays are
		 * illustrated in the figure on the right:
		 *
		 *
		 *   3-------------------2      +---------2---------+
		 *   |                   |      |                   |
		 *   |    6         8    |      |                   |
		 *   |                   |      |                   |
		 *  V|         4         |      1                   3
		 *   |                   |      |                   |
		 *   |    5         7    |      |                   |
		 *   |                   |      |                   |
		 *   0-------------------1      +---------0---------+
		 *             U                          U
		 *
		 *   Values inherited from      Values pre-prepared in
		 *   parent subdivision         shared arrays
		 *
		 *
		 * When the four subdivisions are made, the parent parameters
		 * are passed to the children as follows (values from the
		 * shared arrays are prefaced with an s):
		 *
		 *    3--------------S2     S2--------------2
		 *    |               |     |               |
		 *    |               |     |               |
		 *  V |       6       |     |       8       |
		 *    |               |     |               |
		 *    |               |     |               |
		 *    S1--------------4     4--------------S3
		 *            U                     U
		 *
		 *        Quadrant 3            Quadrant 2
		 *
		 *    S1--------------4     4--------------S3
		 *    |               |     |               |
		 *    |               |     |               |
		 *  V |       5       |     |       7       |
		 *    |               |     |               |
		 *    |               |     |               |
		 *    0--------------S0     S0--------------1
		 *             U                         U
		 *
		 *        Quadrant 0            Quadrant 1
		 *
		 *
		 **********************************************************************/

		ON_Plane sharedframes[4];
		ON_3dPoint sharedcorners[4];
		ON_3dVector sharednormals[4];
		localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
		localsurf->FrameAt(u.Min(), vsplit, sharedframes[1]);
		localsurf->FrameAt(usplit, v.Max(), sharedframes[2]);
		localsurf->FrameAt(u.Max(), vsplit, sharedframes[3]);
		localsurf->FrameAt(usplit, vsplit, frames[4]);

		sharedcorners[0] = sharedframes[0].origin;
		sharednormals[0] = sharedframes[0].zaxis;
		sharedcorners[1] = sharedframes[1].origin;
		sharednormals[1] = sharedframes[1].zaxis;
		sharedcorners[2] = sharedframes[2].origin;
		sharednormals[2] = sharedframes[2].zaxis;
		sharedcorners[3] = sharedframes[3].origin;
		sharednormals[3] = sharedframes[3].zaxis;
		corners[4] = frames[4].origin;
		normals[4] = frames[4].zaxis;

		ON_Plane *newframes;
		ON_3dPoint *newcorners;
		ON_3dVector *newnormals;
		newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");
		newcorners = (ON_3dPoint *)bu_malloc(9*sizeof(ON_3dPoint), "new corners");
		newnormals = (ON_3dVector *)bu_malloc(9*sizeof(ON_3dVector), "new normals");
		newframes[0] = frames[0];
		newcorners[0] = corners[0];
		newnormals[0] = normals[0];
		newframes[1] = sharedframes[0];
		newcorners[1] = sharedcorners[0];
		newnormals[1] = sharednormals[0];
		newframes[2] = frames[4];
		newcorners[2] = corners[4];
		newnormals[2] = normals[4];
		newframes[3] = sharedframes[1];
		newcorners[3] = sharedcorners[1];
		newnormals[3] = sharednormals[1];
		newframes[4] = frames[5];
		newcorners[4] = corners[5];
		newnormals[4] = normals[5];
		quads[0] = subdivideSurfaceByKnots(q0surf, firstu, firstv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete q0surf;
		newframes[0] = sharedframes[0];
		newcorners[0] = sharedcorners[0];
		newnormals[0] = sharednormals[0];
		newframes[1] = frames[1];
		newcorners[1] = corners[1];
		newnormals[1] = normals[1];
		newframes[2] = sharedframes[3];
		newcorners[2] = sharedcorners[3];
		newnormals[2] = sharednormals[3];
		newframes[3] = frames[4];
		newcorners[3] = corners[4];
		newnormals[3] = normals[4];
		newframes[4] = frames[7];
		newcorners[4] = corners[7];
		newnormals[4] = normals[7];
		quads[1] = subdivideSurfaceByKnots(q1surf, secondu, firstv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete q1surf;
		newframes[0] = frames[4];
		newcorners[0] = corners[4];
		newnormals[0] = normals[4];
		newframes[1] = sharedframes[3];
		newcorners[1] = sharedcorners[3];
		newnormals[1] = sharednormals[3];
		newframes[2] = frames[2];
		newcorners[2] = corners[2];
		newnormals[2] = normals[2];
		newframes[3] = sharedframes[2];
		newcorners[3] = sharedcorners[2];
		newnormals[3] = sharednormals[2];
		newframes[4] = frames[8];
		newcorners[4] = corners[8];
		newnormals[4] = normals[8];
		quads[2] = subdivideSurfaceByKnots(q2surf, secondu, secondv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete q2surf;
		newframes[0] = sharedframes[1];
		newcorners[0] = sharedcorners[1];
		newnormals[0] = sharednormals[1];
		newframes[1] = frames[4];
		newcorners[1] = corners[4];
		newnormals[1] = normals[4];
		newframes[2] = sharedframes[2];
		newcorners[2] = sharedcorners[2];
		newnormals[2] = sharednormals[2];
		newframes[3] = frames[3];
		newcorners[3] = corners[3];
		newnormals[3] = normals[3];
		newframes[4] = frames[6];
		newcorners[4] = corners[6];
		newnormals[4] = normals[6];
		quads[3] = subdivideSurfaceByKnots(q3surf, firstu, secondv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete q3surf;
		bu_free(newframes, "free subsurface frames array");
		bu_free(newcorners, "free subsurface corners array");
		bu_free(newnormals, "free subsurface normals array");

		parent->m_trimmed = true;
		parent->m_checkTrim = false;

		for (int i = 0; i < 4; i++) {
			if (!(quads[i]->m_trimmed)) {
				parent->m_trimmed = false;
			}
			if (quads[i]->m_checkTrim) {
				parent->m_checkTrim = true;
			}
		}
		if (m_removeTrimmed) {
			for (int i = 0; i < 4; i++) {
				if (!(quads[i]->m_trimmed))
				parent->addChild(quads[i]);
			}
		} else {
			for (int i = 0; i < 4; i++) {
				parent->addChild(quads[i]);
			}
		}
	} else if (spanu_cnt > 1) {
		double usplit = spanu[(spanu_cnt+1)/2];
		ON_Interval firstu(u.Min(), usplit);
		ON_Interval secondu(usplit, u.Max());
		//////////////////////////////////////
		ON_Surface *east = NULL;
		ON_Surface *west = NULL;

		ON_BoundingBox box = localsurf->BoundingBox();

		int dir = 0;
		localsurf->Split(dir, usplit, east, west);
		east->ClearBoundingBox();
		west->ClearBoundingBox();

		//////////////////////////////////
		/*********************************************************************
		 * In order to avoid fairly expensive re-calculation of 3d points at
		 * uv coordinates, all values that are shared between children at
		 * the same depth of a surface subdivision are pre-computed and
		 * passed as paramters.
		 *
		 * The majority of these points are already evaluated in the process
		 * of testing whether a subdivision has produced a leaf node.  These
		 * values are in the normals and corners arrays and have index values
		 * corresponding to the values of the figure on the left below.  There
		 * are four other shared values that are precomputed in the sharedcorners
		 * and sharednormals arrays; their index values in those arrays are
		 * illustrated in the figure on the right:
		 *
		 *
		 *   3-------------------2      +---------2---------+
		 *   |                   |      |                   |
		 *   |    6         8    |      |                   |
		 *   |                   |      |                   |
		 *  V|         4         |      1                   3
		 *   |                   |      |                   |
		 *   |    5         7    |      |                   |
		 *   |                   |      |                   |
		 *   0-------------------1      +---------0---------+
		 *             U                          U
		 *
		 *   Values inherited from      Values pre-prepared in
		 *   parent subdivision         shared arrays
		 *
		 *
		 * When the four subdivisions are made, the parent parameters
		 * are passed to the children as follows (values from the
		 * shared arrays are prefaced with an s):
		 *
		 *    3--------------S1     S1--------------2
		 *    |               |     |               |
		 *    |               |     |               |
		 *  V |       4       |     |       5       |
		 *    |               |     |               |
		 *    |               |     |               |
		 *    0--------------S0     S0--------------1
		 *             U                         U
		 *
		 *        East                 West
		 *
		 *
		 **********************************************************************/
		/* When the four subdivisions are made, the parent parameters
		 * are passed to the children as follows (values from the
		 * shared arrays are prefaced with an s):
		 *
		 *    3--------------S2     S2--------------2
		 *    |               |     |               |
		 *    |               |     |               |
		 *  V |       6       |     |       8       |
		 *    |               |     |               |
		 *    |               |     |               |
		 *    S1--------------4     4--------------S3
		 *            U                     U
		 *
		 *        Quadrant 3            Quadrant 2
		 *
		 *    S1--------------4     4--------------S3
		 *    |               |     |               |
		 *    |               |     |               |
		 *  V |       5       |     |       7       |
		 *    |               |     |               |
		 *    |               |     |               |
		 *    0--------------S0     S0--------------1
		 *             U                         U
		 *
		 *        Quadrant 0            Quadrant 1
		 *
		 *
		 **********************************************************************/

		ON_Plane sharedframes[4];
		ON_3dPoint sharedcorners[4];
		ON_3dVector sharednormals[4];
		localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
		localsurf->FrameAt(usplit, v.Max(), sharedframes[1]);

		sharedcorners[0] = sharedframes[0].origin;
		sharednormals[0] = sharedframes[0].zaxis;
		sharedcorners[1] = sharedframes[1].origin;
		sharednormals[1] = sharedframes[1].zaxis;

		ON_Plane *newframes;
		ON_3dPoint *newcorners;
		ON_3dVector *newnormals;
		newframes = (ON_Plane *) bu_malloc(9 * sizeof(ON_Plane),
				"new frames");
		newcorners = (ON_3dPoint *) bu_malloc(9 * sizeof(ON_3dPoint),
				"new corners");
		newnormals = (ON_3dVector *) bu_malloc(9 * sizeof(ON_3dVector),
				"new normals");
		newframes[0] = frames[0];
		newcorners[0] = corners[0];
		newnormals[0] = normals[0];
		newframes[1] = sharedframes[0];
		newcorners[1] = sharedcorners[0];
		newnormals[1] = sharednormals[0];
		newframes[2] = sharedframes[1];
		newcorners[2] = sharedcorners[1];
		newnormals[2] = sharednormals[1];
		newframes[3] = frames[3];
		newcorners[3] = corners[3];
		newnormals[3] = normals[3];
		localsurf->FrameAt(firstu.Mid(), v.Mid(), newframes[4]);

		newcorners[4] = newframes[4].origin;
		newnormals[4] = newframes[4].zaxis;

		//ON_BoundingBox bbox = q0surf->BoundingBox();
		//bu_log("%d - in bbq0 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
		quads[0] = subdivideSurfaceByKnots(east, firstu, v, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete east;

		newframes[0] = sharedframes[0];
		newcorners[0] = sharedcorners[0];
		newnormals[0] = sharednormals[0];
		newframes[1] = frames[1];
		newcorners[1] = corners[1];
		newnormals[1] = normals[1];
		newframes[2] = frames[2];
		newcorners[2] = corners[2];
		newnormals[2] = normals[2];
		newframes[3] = sharedframes[1];
		newcorners[3] = sharedcorners[1];
		newnormals[3] = sharednormals[1];
		localsurf->FrameAt(secondu.Mid(), v.Mid(), newframes[4]);

		newcorners[4] = newframes[4].origin;
		newnormals[4] = newframes[4].zaxis;

		//bbox = q1surf->BoundingBox();
		//bu_log("%d - in bbq1 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
		quads[1] = subdivideSurfaceByKnots(west, secondu, v, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete west;

		bu_free(newframes, "free subsurface frames array");
		bu_free(newcorners, "free subsurface corners array");
		bu_free(newnormals, "free subsurface normals array");

		parent->m_trimmed = true;
		parent->m_checkTrim = false;

		for (int i = 0; i < 2; i++) {
			if (!(quads[i]->m_trimmed)) {
				parent->m_trimmed = false;
			}
			if (quads[i]->m_checkTrim) {
				parent->m_checkTrim = true;
			}
		}
		if (m_removeTrimmed) {
			for (int i = 0; i < 2; i++) {
				if (!(quads[i]->m_trimmed))
				parent->addChild(quads[i]);
			}
		} else {
			for (int i = 0; i < 2; i++) {
				parent->addChild(quads[i]);
			}
		}
	} else if (spanv_cnt > 1) {
		double vsplit = spanv[(spanv_cnt+1)/2];
		ON_Interval firstv(v.Min(), vsplit);
		ON_Interval secondv(vsplit, v.Max());

		//////////////////////////////////////
		ON_Surface *north = NULL;
		ON_Surface *south = NULL;

		ON_BoundingBox box = localsurf->BoundingBox();

		int dir = 1;
		localsurf->Split(dir, vsplit, south, north);
		south->ClearBoundingBox();
		north->ClearBoundingBox();

		//////////////////////////////////
		/*********************************************************************
		 * In order to avoid fairly expensive re-calculation of 3d points at
		 * uv coordinates, all values that are shared between children at
		 * the same depth of a surface subdivision are pre-computed and
		 * passed as paramters.
		 *
		 * The majority of these points are already evaluated in the process
		 * of testing whether a subdivision has produced a leaf node.  These
		 * values are in the normals and corners arrays and have index values
		 * corresponding to the values of the figure on the left below.  There
		 * are four other shared values that are precomputed in the sharedcorners
		 * and sharednormals arrays; their index values in those arrays are
		 * illustrated in the figure on the right:
		 *
		 *
		 *   3-------------------2      +---------2---------+
		 *   |                   |      |                   |
		 *   |    6         8    |      |                   |
		 *   |                   |      |                   |
		 *  V|         4         |      1                   3
		 *   |                   |      |                   |
		 *   |    5         7    |      |                   |
		 *   |                   |      |                   |
		 *   0-------------------1      +---------0---------+
		 *             U                          U
		 *
		 *   Values inherited from      Values pre-prepared in
		 *   parent subdivision         shared arrays
		 *
		 *
		 * When the four subdivisions are made, the parent parameters
		 * are passed to the children as follows (values from the
		 * shared arrays are prefaced with an s):
		 *
		 *    3--------------------2
		 *    |                    |
		 *    |                    |
		 *  V |         5          |
		 *    |                    |
		 *    |                    |
		 *    S0-------------------S1
		 *            U
		 *
		 *        North
		 *
		 *    S0-------------------S1
		 *    |                    |
		 *    |                    |
		 *  V |         4          |
		 *    |                    |
		 *    |                    |
		 *    0--------------------1
		 *             U
		 *
		 *        South
		 *
		 *
		 **********************************************************************/

		ON_Plane sharedframes[2];
		ON_3dPoint sharedcorners[2];
		ON_3dVector sharednormals[2];
		localsurf->FrameAt(u.Min(), vsplit, sharedframes[0]);
		localsurf->FrameAt(u.Max(), vsplit, sharedframes[1]);

		sharedcorners[0] = sharedframes[0].origin;
		sharednormals[0] = sharedframes[0].zaxis;
		sharedcorners[1] = sharedframes[1].origin;
		sharednormals[1] = sharedframes[1].zaxis;

		ON_Plane *newframes;
		ON_3dPoint *newcorners;
		ON_3dVector *newnormals;
		newframes = (ON_Plane *) bu_malloc(9 * sizeof(ON_Plane),
				"new frames");
		newcorners = (ON_3dPoint *) bu_malloc(9 * sizeof(ON_3dPoint),
				"new corners");
		newnormals = (ON_3dVector *) bu_malloc(9 * sizeof(ON_3dVector),
				"new normals");
		newframes[0] = frames[0];
		newcorners[0] = corners[0];
		newnormals[0] = normals[0];
		newframes[1] = frames[1];
		newcorners[1] = corners[1];
		newnormals[1] = normals[1];
		newframes[2] = sharedframes[1];
		newcorners[2] = sharedcorners[1];
		newnormals[2] = sharednormals[1];
		newframes[3] = sharedframes[0];
		newcorners[3] = sharedcorners[0];
		newnormals[3] = sharednormals[0];
		localsurf->FrameAt(u.Mid(), firstv.Mid(), newframes[4]);

		newcorners[4] = newframes[4].origin;
		newnormals[4] = newframes[4].zaxis;
		//ON_BoundingBox bbox = q0surf->BoundingBox();
		//bu_log("%d - in bbq0 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
		quads[0] = subdivideSurfaceByKnots(south, u, firstv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete south;

		newframes[0] = sharedframes[0];
		newcorners[0] = sharedcorners[0];
		newnormals[0] = sharednormals[0];
		newframes[1] = sharedframes[1];
		newcorners[1] = sharedcorners[1];
		newnormals[1] = sharednormals[1];
		newframes[2] = frames[2];
		newcorners[2] = corners[2];
		newnormals[2] = normals[2];
		newframes[3] = frames[3];
		newcorners[3] = corners[3];
		newnormals[3] = normals[3];
		localsurf->FrameAt(u.Mid(), secondv.Mid(), newframes[4]);

		newcorners[4] = newframes[4].origin;
		newnormals[4] = newframes[4].zaxis;
		//bbox = q1surf->BoundingBox();
		//bu_log("%d - in bbq1 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
		quads[1] = subdivideSurfaceByKnots(north, u, secondv, newframes, newcorners, newnormals, divDepth+1, depthLimit);
		delete north;

		bu_free(newframes, "free subsurface frames array");
		bu_free(newcorners, "free subsurface corners array");
		bu_free(newnormals, "free subsurface normals array");

		parent->m_trimmed = true;
		parent->m_checkTrim = false;

		for (int i = 0; i < 2; i++) {
			if (!(quads[i]->m_trimmed)) {
				parent->m_trimmed = false;
			}
			if (quads[i]->m_checkTrim) {
				parent->m_checkTrim = true;
			}
		}
		if (m_removeTrimmed) {
			for (int i = 0; i < 2; i++) {
				if (!(quads[i]->m_trimmed))
				parent->addChild(quads[i]);
			}
		} else {
			for (int i = 0; i < 2; i++) {
				parent->addChild(quads[i]);
			}
		}
	} else {
		//return surfaceBBox(localsurf, true, corners, normals, u, v);
		//parent->addChild(subdivideSurface(localsurf, u, v, frames, corners, normals,0));
	    ((ON_Surface *)localsurf)->ClearBoundingBox();
		return subdivideSurface(localsurf, u, v, frames, corners, normals,0, depthLimit);
	}
	delete [] spanu;
	delete [] spanv;

	parent->BuildBBox();
	return parent;

}

BBNode*
SurfaceTree::subdivideSurface(const ON_Surface *localsurf,
	const ON_Interval& u,
	const ON_Interval& v,
	ON_Plane frames[],
	ON_3dPoint corners[],
	ON_3dVector normals[],
	int divDepth,
	int depthLimit)
{
	const ON_Surface* surf = m_face->SurfaceOf();
	ON_Interval usurf = surf->Domain(0);
	ON_Interval vsurf = surf->Domain(1);

	double uq = u.Length()*0.25;
	double vq = v.Length()*0.25;
	localsurf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
	localsurf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
	localsurf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
	localsurf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);

	corners[5] = frames[5].origin;
	normals[5] = frames[5].zaxis;
	corners[6] = frames[6].origin;
	normals[6] = frames[6].zaxis;
	corners[7] = frames[7].origin;
	normals[7] = frames[7].zaxis;
	corners[8] = frames[8].origin;
	normals[8] = frames[8].zaxis;

	double width,height;
	double ratio = 5.0;
	localsurf->GetSurfaceSize(&width, &height);
	if ((((width/height < ratio) && (width/height > 1.0/ratio))
					&& isFlat(localsurf, frames, normals, corners, u, v)
					&& isStraight(localsurf, frames, normals, corners, u, v)) || (divDepth >= depthLimit)) { //BREP_MAX_FT_DEPTH))) {
		return surfaceBBox(localsurf, true, corners, normals, u, v);
	} else {
		fastf_t uflat = isFlatU(localsurf, frames, normals, corners, u, v);
		fastf_t vflat = isFlatV(localsurf, frames, normals, corners, u, v);
		bool isUFlat = uflat >= BREP_SURFACE_FLATNESS;
		bool isVFlat = vflat >= BREP_SURFACE_FLATNESS;
		BBNode* parent = (divDepth == 0) ? initialBBox(ctree, localsurf, m_face, u, v) :
		surfaceBBox(localsurf, false, corners, normals, u, v);
		BBNode* quads[4];
		ON_Interval first(0, 0.5);
		ON_Interval second(0.5, 1.0);

		if ((!isVFlat || (width/height > ratio)) && (!isUFlat || (height/width > ratio))) {
			ON_Surface *north = NULL;
			ON_Surface *south = NULL;
			ON_Surface *q2surf = NULL;
			ON_Surface *q3surf = NULL;
			ON_Surface *q1surf = NULL;
			ON_Surface *q0surf = NULL;

			ON_BoundingBox box = localsurf->BoundingBox();

			int dir = 1;
			localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);
			south->ClearBoundingBox();
			north->ClearBoundingBox();

			dir = 0;
			south->Split(dir, south->Domain(dir).Mid(), q0surf, q1surf);
			delete south;
			q0surf->ClearBoundingBox();
			q1surf->ClearBoundingBox();
			north->Split(dir, north->Domain(dir).Mid(), q3surf, q2surf);
			delete north;
			q3surf->ClearBoundingBox();
			q2surf->ClearBoundingBox();
			/*********************************************************************
			 * In order to avoid fairly expensive re-calculation of 3d points at
			 * uv coordinates, all values that are shared between children at
			 * the same depth of a surface subdivision are pre-computed and
			 * passed as paramters.
			 *
			 * The majority of these points are already evaluated in the process
			 * of testing whether a subdivision has produced a leaf node.  These
			 * values are in the normals and corners arrays and have index values
			 * corresponding to the values of the figure on the left below.  There
			 * are four other shared values that are precomputed in the sharedcorners
			 * and sharednormals arrays; their index values in those arrays are
			 * illustrated in the figure on the right:
			 *
			 *
			 *   3-------------------2      +---------2---------+
			 *   |                   |      |                   |
			 *   |    6         8    |      |                   |
			 *   |                   |      |                   |
			 *  V|         4         |      1                   3
			 *   |                   |      |                   |
			 *   |    5         7    |      |                   |
			 *   |                   |      |                   |
			 *   0-------------------1      +---------0---------+
			 *             U                          U
			 *
			 *   Values inherited from      Values pre-prepared in
			 *   parent subdivision         shared arrays
			 *
			 *
			 * When the four subdivisions are made, the parent parameters
			 * are passed to the children as follows (values from the
			 * shared arrays are prefaced with an s):
			 *
			 *    3--------------S2     S2--------------2
			 *    |               |     |               |
			 *    |               |     |               |
			 *  V |       6       |     |       8       |
			 *    |               |     |               |
			 *    |               |     |               |
			 *    S1--------------4     4--------------S3
			 *            U                     U
			 *
			 *        Quadrant 3            Quadrant 2
			 *
			 *    S1--------------4     4--------------S3
			 *    |               |     |               |
			 *    |               |     |               |
			 *  V |       5       |     |       7       |
			 *    |               |     |               |
			 *    |               |     |               |
			 *    0--------------S0     S0--------------1
			 *             U                         U
			 *
			 *        Quadrant 0            Quadrant 1
			 *
			 *
			 **********************************************************************/

			ON_Plane sharedframes[4];
			ON_3dPoint sharedcorners[4];
			ON_3dVector sharednormals[4];
			localsurf->FrameAt(u.Mid(), v.Min(), sharedframes[0]);
			localsurf->FrameAt(u.Min(), v.Mid(), sharedframes[1]);
			localsurf->FrameAt(u.Mid(), v.Max(), sharedframes[2]);
			localsurf->FrameAt(u.Max(), v.Mid(), sharedframes[3]);

			sharedcorners[0] = sharedframes[0].origin;
			sharednormals[0] = sharedframes[0].zaxis;
			sharedcorners[1] = sharedframes[1].origin;
			sharednormals[1] = sharedframes[1].zaxis;
			sharedcorners[2] = sharedframes[2].origin;
			sharednormals[2] = sharedframes[2].zaxis;
			sharedcorners[3] = sharedframes[3].origin;
			sharednormals[3] = sharedframes[3].zaxis;

			ON_Plane *newframes;
			ON_3dPoint *newcorners;
			ON_3dVector *newnormals;
			newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");
			newcorners = (ON_3dPoint *)bu_malloc(9*sizeof(ON_3dPoint), "new corners");
			newnormals = (ON_3dVector *)bu_malloc(9*sizeof(ON_3dVector), "new normals");
			newframes[0] = frames[0];
			newcorners[0] = corners[0];
			newnormals[0] = normals[0];
			newframes[1] = sharedframes[0];
			newcorners[1] = sharedcorners[0];
			newnormals[1] = sharednormals[0];
			newframes[2] = frames[4];
			newcorners[2] = corners[4];
			newnormals[2] = normals[4];
			newframes[3] = sharedframes[1];
			newcorners[3] = sharedcorners[1];
			newnormals[3] = sharednormals[1];
			newframes[4] = frames[5];
			newcorners[4] = corners[5];
			newnormals[4] = normals[5];
			quads[0] = subdivideSurface(q0surf, u.ParameterAt(first), v.ParameterAt(first), newframes, newcorners, newnormals, divDepth+1, depthLimit);
			delete q0surf;
			newframes[0] = sharedframes[0];
			newcorners[0] = sharedcorners[0];
			newnormals[0] = sharednormals[0];
			newframes[1] = frames[1];
			newcorners[1] = corners[1];
			newnormals[1] = normals[1];
			newframes[2] = sharedframes[3];
			newcorners[2] = sharedcorners[3];
			newnormals[2] = sharednormals[3];
			newframes[3] = frames[4];
			newcorners[3] = corners[4];
			newnormals[3] = normals[4];
			newframes[4] = frames[7];
			newcorners[4] = corners[7];
			newnormals[4] = normals[7];
			quads[1] = subdivideSurface(q1surf, u.ParameterAt(second), v.ParameterAt(first), newframes, newcorners, newnormals, divDepth+1, depthLimit);
			delete q1surf;
			newframes[0] = frames[4];
			newcorners[0] = corners[4];
			newnormals[0] = normals[4];
			newframes[1] = sharedframes[3];
			newcorners[1] = sharedcorners[3];
			newnormals[1] = sharednormals[3];
			newframes[2] = frames[2];
			newcorners[2] = corners[2];
			newnormals[2] = normals[2];
			newframes[3] = sharedframes[2];
			newcorners[3] = sharedcorners[2];
			newnormals[3] = sharednormals[2];
			newframes[4] = frames[8];
			newcorners[4] = corners[8];
			newnormals[4] = normals[8];
			quads[2] = subdivideSurface(q2surf, u.ParameterAt(second), v.ParameterAt(second), newframes, newcorners, newnormals, divDepth+1, depthLimit);
			delete q2surf;
			newframes[0] = sharedframes[1];
			newcorners[0] = sharedcorners[1];
			newnormals[0] = sharednormals[1];
			newframes[1] = frames[4];
			newcorners[1] = corners[4];
			newnormals[1] = normals[4];
			newframes[2] = sharedframes[2];
			newcorners[2] = sharedcorners[2];
			newnormals[2] = sharednormals[2];
			newframes[3] = frames[3];
			newcorners[3] = corners[3];
			newnormals[3] = normals[3];
			newframes[4] = frames[6];
			newcorners[4] = corners[6];
			newnormals[4] = normals[6];
			quads[3] = subdivideSurface(q3surf, u.ParameterAt(first), v.ParameterAt(second), newframes, newcorners, newnormals, divDepth+1, depthLimit);
			delete q3surf;
			bu_free(newframes, "free subsurface frames array");
			bu_free(newcorners, "free subsurface corners array");
			bu_free(newnormals, "free subsurface normals array");

			parent->m_trimmed = true;
			parent->m_checkTrim = false;

			for (int i = 0; i < 4; i++) {
				if (!(quads[i]->m_trimmed)) {
					parent->m_trimmed = false;
				}
				if (quads[i]->m_checkTrim) {
					parent->m_checkTrim = true;
				}
			}
			if (m_removeTrimmed) {
				for (int i = 0; i < 4; i++) {
					if (!(quads[i]->m_trimmed))
					parent->addChild(quads[i]);
				}
			} else {
				for (int i = 0; i < 4; i++) {
					parent->addChild(quads[i]);
				}
			}
		} else if (!isUFlat || (width/height > ratio)) {
			//////////////////////////////////////
			ON_Surface *east = NULL;
			ON_Surface *west = NULL;

			ON_BoundingBox box = localsurf->BoundingBox();

			int dir = 0;
			localsurf->Split(dir, localsurf->Domain(dir).Mid(), east, west);
			east->ClearBoundingBox();
			west->ClearBoundingBox();

			//////////////////////////////////
			/*********************************************************************
			 * In order to avoid fairly expensive re-calculation of 3d points at
			 * uv coordinates, all values that are shared between children at
			 * the same depth of a surface subdivision are pre-computed and
			 * passed as paramters.
			 *
			 * The majority of these points are already evaluated in the process
			 * of testing whether a subdivision has produced a leaf node.  These
			 * values are in the normals and corners arrays and have index values
			 * corresponding to the values of the figure on the left below.  There
			 * are four other shared values that are precomputed in the sharedcorners
			 * and sharednormals arrays; their index values in those arrays are
			 * illustrated in the figure on the right:
			 *
			 *
			 *   3-------------------2      +---------2---------+
			 *   |                   |      |                   |
			 *   |    6         8    |      |                   |
			 *   |                   |      |                   |
			 *  V|         4         |      1                   3
			 *   |                   |      |                   |
			 *   |    5         7    |      |                   |
			 *   |                   |      |                   |
			 *   0-------------------1      +---------0---------+
			 *             U                          U
			 *
			 *   Values inherited from      Values pre-prepared in
			 *   parent subdivision         shared arrays
			 *
			 *
			 * When the four subdivisions are made, the parent parameters
			 * are passed to the children as follows (values from the
			 * shared arrays are prefaced with an s):
			 *
			 *    3--------------S1     S1--------------2
			 *    |               |     |               |
			 *    |               |     |               |
			 *  V |       4       |     |       5       |
			 *    |               |     |               |
			 *    |               |     |               |
			 *    0--------------S0     S0--------------1
			 *             U                         U
			 *
			 *        East                 West
			 *
			 *
			 **********************************************************************/
			/* When the four subdivisions are made, the parent parameters
			 * are passed to the children as follows (values from the
			 * shared arrays are prefaced with an s):
			 *
			 *    3--------------S2     S2--------------2
			 *    |               |     |               |
			 *    |               |     |               |
			 *  V |       6       |     |       8       |
			 *    |               |     |               |
			 *    |               |     |               |
			 *    S1--------------4     4--------------S3
			 *            U                     U
			 *
			 *        Quadrant 3            Quadrant 2
			 *
			 *    S1--------------4     4--------------S3
			 *    |               |     |               |
			 *    |               |     |               |
			 *  V |       5       |     |       7       |
			 *    |               |     |               |
			 *    |               |     |               |
			 *    0--------------S0     S0--------------1
			 *             U                         U
			 *
			 *        Quadrant 0            Quadrant 1
			 *
			 *
			 **********************************************************************/

			ON_Plane sharedframes[4];
			ON_3dPoint sharedcorners[4];
			ON_3dVector sharednormals[4];
			localsurf->FrameAt(u.Mid(), v.Min(), sharedframes[0]);
			localsurf->FrameAt(u.Mid(), v.Max(), sharedframes[1]);

			sharedcorners[0] = sharedframes[0].origin;
			sharednormals[0] = sharedframes[0].zaxis;
			sharedcorners[1] = sharedframes[1].origin;
			sharednormals[1] = sharedframes[1].zaxis;

			ON_Plane *newframes;
			ON_3dPoint *newcorners;
			ON_3dVector *newnormals;
			newframes = (ON_Plane *) bu_malloc(9 * sizeof(ON_Plane),
					"new frames");
			newcorners = (ON_3dPoint *) bu_malloc(9 * sizeof(ON_3dPoint),
					"new corners");
			newnormals = (ON_3dVector *) bu_malloc(9 * sizeof(ON_3dVector),
					"new normals");
			newframes[0] = frames[0];
			newcorners[0] = corners[0];
			newnormals[0] = normals[0];
			newframes[1] = sharedframes[0];
			newcorners[1] = sharedcorners[0];
			newnormals[1] = sharednormals[0];
			newframes[2] = sharedframes[1];
			newcorners[2] = sharedcorners[1];
			newnormals[2] = sharednormals[1];
			newframes[3] = frames[3];
			newcorners[3] = corners[3];
			newnormals[3] = normals[3];
			localsurf->FrameAt(u.Mid() - uq, v.Mid(), newframes[4]);

			newcorners[4] = newframes[4].origin;
			newnormals[4] = newframes[4].zaxis;
			//ON_BoundingBox bbox = q0surf->BoundingBox();
			//bu_log("%d - in bbq0 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
			quads[0] = subdivideSurface(east, u.ParameterAt(first), v, newframes, newcorners, newnormals, divDepth + 1, depthLimit);
			delete east;

			newframes[0] = sharedframes[0];
			newcorners[0] = sharedcorners[0];
			newnormals[0] = sharednormals[0];
			newframes[1] = frames[1];
			newcorners[1] = corners[1];
			newnormals[1] = normals[1];
			newframes[2] = frames[2];
			newcorners[2] = corners[2];
			newnormals[2] = normals[2];
			newframes[3] = sharedframes[1];
			newcorners[3] = sharedcorners[1];
			newnormals[3] = sharednormals[1];
			localsurf->FrameAt(u.Mid() + uq, v.Mid(), newframes[4]);

			newcorners[4] = newframes[4].origin;
			newnormals[4] = newframes[4].zaxis;
			//bbox = q1surf->BoundingBox();
			//bu_log("%d - in bbq1 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
			quads[1] = subdivideSurface(west, u.ParameterAt(second), v, newframes, newcorners, newnormals, divDepth + 1, depthLimit);
			delete west;

			bu_free(newframes, "free subsurface frames array");
			bu_free(newcorners, "free subsurface corners array");
			bu_free(newnormals, "free subsurface normals array");

			parent->m_trimmed = true;
			parent->m_checkTrim = false;

			for (int i = 0; i < 2; i++) {
				if (!(quads[i]->m_trimmed)) {
					parent->m_trimmed = false;
				}
				if (quads[i]->m_checkTrim) {
					parent->m_checkTrim = true;
				}
			}
			if (m_removeTrimmed) {
				for (int i = 0; i < 2; i++) {
					if (!(quads[i]->m_trimmed))
					parent->addChild(quads[i]);
				}
			} else {
				for (int i = 0; i < 2; i++) {
					parent->addChild(quads[i]);
				}
			}
		} else { /* assume uflat */ //if (!isVFlat || (height/width > 2.0)) {
			//////////////////////////////////////
			ON_Surface *north = NULL;
			ON_Surface *south = NULL;

			ON_BoundingBox box = localsurf->BoundingBox();

			int dir = 1;
			localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);
			south->ClearBoundingBox();
			north->ClearBoundingBox();

			//////////////////////////////////
			/*********************************************************************
			 * In order to avoid fairly expensive re-calculation of 3d points at
			 * uv coordinates, all values that are shared between children at
			 * the same depth of a surface subdivision are pre-computed and
			 * passed as paramters.
			 *
			 * The majority of these points are already evaluated in the process
			 * of testing whether a subdivision has produced a leaf node.  These
			 * values are in the normals and corners arrays and have index values
			 * corresponding to the values of the figure on the left below.  There
			 * are four other shared values that are precomputed in the sharedcorners
			 * and sharednormals arrays; their index values in those arrays are
			 * illustrated in the figure on the right:
			 *
			 *
			 *   3-------------------2      +---------2---------+
			 *   |                   |      |                   |
			 *   |    6         8    |      |                   |
			 *   |                   |      |                   |
			 *  V|         4         |      1                   3
			 *   |                   |      |                   |
			 *   |    5         7    |      |                   |
			 *   |                   |      |                   |
			 *   0-------------------1      +---------0---------+
			 *             U                          U
			 *
			 *   Values inherited from      Values pre-prepared in
			 *   parent subdivision         shared arrays
			 *
			 *
			 * When the four subdivisions are made, the parent parameters
			 * are passed to the children as follows (values from the
			 * shared arrays are prefaced with an s):
			 *
			 *    3--------------------2
			 *    |                    |
			 *    |                    |
			 *  V |         5          |
			 *    |                    |
			 *    |                    |
			 *    S0-------------------S1
			 *            U
			 *
			 *        North
			 *
			 *    S0-------------------S1
			 *    |                    |
			 *    |                    |
			 *  V |         4          |
			 *    |                    |
			 *    |                    |
			 *    0--------------------1
			 *             U
			 *
			 *        South
			 *
			 *
			 **********************************************************************/

			ON_Plane sharedframes[2];
			ON_3dPoint sharedcorners[2];
			ON_3dVector sharednormals[2];
			localsurf->FrameAt(u.Min(), v.Mid(), sharedframes[0]);
			localsurf->FrameAt(u.Max(), v.Mid(), sharedframes[1]);

			sharedcorners[0] = sharedframes[0].origin;
			sharednormals[0] = sharedframes[0].zaxis;
			sharedcorners[1] = sharedframes[1].origin;
			sharednormals[1] = sharedframes[1].zaxis;

			ON_Plane *newframes;
			ON_3dPoint *newcorners;
			ON_3dVector *newnormals;
			newframes = (ON_Plane *) bu_malloc(9 * sizeof(ON_Plane),
					"new frames");
			newcorners = (ON_3dPoint *) bu_malloc(9 * sizeof(ON_3dPoint),
					"new corners");
			newnormals = (ON_3dVector *) bu_malloc(9 * sizeof(ON_3dVector),
					"new normals");
			newframes[0] = frames[0];
			newcorners[0] = corners[0];
			newnormals[0] = normals[0];
			newframes[1] = frames[1];
			newcorners[1] = corners[1];
			newnormals[1] = normals[1];
			newframes[2] = sharedframes[1];
			newcorners[2] = sharedcorners[1];
			newnormals[2] = sharednormals[1];
			newframes[3] = sharedframes[0];
			newcorners[3] = sharedcorners[0];
			newnormals[3] = sharednormals[0];
			localsurf->FrameAt(u.Mid(), v.Mid() - vq, newframes[4]);

			newcorners[4] = newframes[4].origin;
			newnormals[4] = newframes[4].zaxis;
			//ON_BoundingBox bbox = q0surf->BoundingBox();
			//bu_log("%d - in bbq0 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
			quads[0] = subdivideSurface(south, u, v.ParameterAt(first), newframes, newcorners, newnormals, divDepth + 1, depthLimit);
			delete south;

			newframes[0] = sharedframes[0];
			newcorners[0] = sharedcorners[0];
			newnormals[0] = sharednormals[0];
			newframes[1] = sharedframes[1];
			newcorners[1] = sharedcorners[1];
			newnormals[1] = sharednormals[1];
			newframes[2] = frames[2];
			newcorners[2] = corners[2];
			newnormals[2] = normals[2];
			newframes[3] = frames[3];
			newcorners[3] = corners[3];
			newnormals[3] = normals[3];
			localsurf->FrameAt(u.Mid(), v.Mid() + vq, newframes[4]);

			newcorners[4] = newframes[4].origin;
			newnormals[4] = newframes[4].zaxis;
			//bbox = q1surf->BoundingBox();
			//bu_log("%d - in bbq1 rpp %f %f %f %f %f %f\n", divDepth, bbox.m_min.x, bbox.m_max.x, bbox.m_min.y, bbox.m_max.y, bbox.m_min.z, bbox.m_max.z);
			quads[1] = subdivideSurface(north, u, v.ParameterAt(second), newframes, newcorners, newnormals, divDepth + 1, depthLimit);
			delete north;

			bu_free(newframes, "free subsurface frames array");
			bu_free(newcorners, "free subsurface corners array");
			bu_free(newnormals, "free subsurface normals array");

			parent->m_trimmed = true;
			parent->m_checkTrim = false;

			for (int i = 0; i < 2; i++) {
				if (!(quads[i]->m_trimmed)) {
					parent->m_trimmed = false;
				}
				if (quads[i]->m_checkTrim) {
					parent->m_checkTrim = true;
				}
			}
			if (m_removeTrimmed) {
				for (int i = 0; i < 2; i++) {
					if (!(quads[i]->m_trimmed))
					parent->addChild(quads[i]);
				}
			} else {
				for (int i = 0; i < 2; i++) {
					parent->addChild(quads[i]);
				}
			}
		}
		parent->BuildBBox();
		return parent;
	}
}


/**
 * Determine whether a given surface is flat enough, i.e. it falls
 * beneath our simple flatness constraints. The flatness constraint in
 * this case is a sampling of normals across the surface such that the
 * product of their combined dot products is close to 1.
 *
 * @f[ \prod_{i=1}^{7} n_i \dot n_{i+1} = 1 @f]
 *
 * Would be a perfectly flat surface. Generally something in the range
 * 0.8-0.9 should suffice (according to Abert, 2005).
 *
 * We're using a slightly different placement of the interior normal
 * tests to save on calculations
 *
 *   +-------------------+
 *   |                   |
 *   |    +         +    |
 *   |                   |
 *  V|                   |
 *   |                   |
 *   |    +         +    |
 *   |                   |
 *   +-------------------+
 *             U
 *
 *
 * The "+" indicates the normal sample.
 *
 * The positions are stored in the corner and normal arrays according
 * to the following index values:
 *
 *   3-------------------2
 *   |                   |
 *   |    6         8    |
 *   |                   |
 *  V|         4         |
 *   |                   |
 *   |    5         7    |
 *   |                   |
 *   0-------------------1
 *             U
 *
 * The actual values used in the flatness test are 0, 1, 2, 3 and
 * 5, 6, 7, 8 - the center point is not used.
 *
 */

#define NE 1
#define NW 2
#define SW 3
#define SE 4

bool
SurfaceTree::isFlat(const ON_Surface* UNUSED(surf), ON_Plane *frames, ON_3dVector *UNUSED(m_normals), ON_3dPoint *UNUSED(corners), const ON_Interval& UNUSED(u), const ON_Interval& UNUSED(v))
{
	/*
    ON_3dVector normals[8];
    for (int i = 0; i < 4; i++) {
	normals[i] = m_normals[i];
    }

    for (int i = 4; i < 8; i++) {
	normals[i] = m_normals[i+1];
    }

    double product = 1.0;

    double ax[4] VEC_ALIGN;
    double ay[4] VEC_ALIGN;
    double az[4] VEC_ALIGN;

    double bx[4] VEC_ALIGN;
    double by[4] VEC_ALIGN;
    double bz[4] VEC_ALIGN;

    distribute(4, normals, ax, ay, az);
    distribute(4, &normals[1], bx, by, bz);

    // how to get the normals in here?
    {
	dvec<4> xa(ax);
	dvec<4> ya(ay);
	dvec<4> za(az);
	dvec<4> xb(bx);
	dvec<4> yb(by);
	dvec<4> zb(bz);
	dvec<4> dots = xa * xb + ya * yb + za * zb;
	product *= dots.foldr(1, dvec<4>::mul());
	if (product < 0.0) return false;
    }
    // try the next set of normals
    {
	distribute(3, &normals[4], ax, ay, az);
	distribute(3, &normals[5], bx, by, bz);
	dvec<4> xa(ax);
	dvec<4> xb(bx);
	dvec<4> ya(ay);
	dvec<4> yb(by);
	dvec<4> za(az);
	dvec<4> zb(bz);
	dvec<4> dots = xa * xb + ya * yb + za * zb;
	product *= dots.foldr(1, dvec<4>::mul(), 3);
    }
    return product >= BREP_SURFACE_FLATNESS;
    */

    double Ndot = frames[0].zaxis * frames[1].zaxis;
    Ndot = Ndot * (frames[0].zaxis * frames[2].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[3].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[4].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[5].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[0].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[2].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[3].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[4].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[5].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[1].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[3].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[4].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[5].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[2].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[3].zaxis * frames[4].zaxis);
    Ndot = Ndot * (frames[3].zaxis * frames[5].zaxis);
    Ndot = Ndot * (frames[3].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[3].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[3].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[4].zaxis * frames[5].zaxis);
    Ndot = Ndot * (frames[4].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[4].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[4].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[5].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[5].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[5].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[6].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[6].zaxis * frames[8].zaxis);
    Ndot = Ndot * (frames[7].zaxis * frames[8].zaxis);

    return (fabs(Ndot) >= BREP_SURFACE_FLATNESS); // && fabs(uvdot) >= BREP_SURFACE_FLATNESS;

}

bool
SurfaceTree::isStraight(const ON_Surface* UNUSED(surf), ON_Plane *frames, ON_3dVector *UNUSED(m_normals), ON_3dPoint *UNUSED(corners), const ON_Interval& UNUSED(u), const ON_Interval& UNUSED(v))
{
    double Xdot = frames[0].xaxis * frames[1].xaxis;
    Xdot = Xdot * (frames[0].xaxis * frames[2].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[3].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[4].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[5].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[0].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[2].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[3].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[4].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[5].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[1].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[3].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[4].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[5].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[2].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[3].xaxis * frames[4].xaxis);
    Xdot = Xdot * (frames[3].xaxis * frames[5].xaxis);
    Xdot = Xdot * (frames[3].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[3].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[3].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[4].xaxis * frames[5].xaxis);
    Xdot = Xdot * (frames[4].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[4].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[4].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[5].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[5].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[5].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[6].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[6].xaxis * frames[8].xaxis);
    Xdot = Xdot * (frames[7].xaxis * frames[8].xaxis);

    return (fabs(Xdot) >= BREP_SURFACE_STRAIGHTNESS);
}

fastf_t
SurfaceTree::isFlatU(const ON_Surface* UNUSED(surf), ON_Plane *frames, ON_3dVector *m_normals, ON_3dPoint *UNUSED(corners), const ON_Interval& UNUSED(u), const ON_Interval& UNUSED(v))
{
    fastf_t product = 1.0;
    product *= m_normals[0]*m_normals[1];
    product *= m_normals[2]*m_normals[3];
    product *= m_normals[5]*m_normals[7];
    product *= m_normals[6]*m_normals[8];

    double Xdot = frames[0].xaxis * frames[1].xaxis;
    Xdot = Xdot * (frames[2].xaxis * frames[3].xaxis);
    Xdot = Xdot * (frames[5].xaxis * frames[7].xaxis);
    Xdot = Xdot * (frames[6].xaxis * frames[8].xaxis);

    double Ndot = frames[0].zaxis * frames[1].zaxis;
    Ndot = Ndot * (frames[2].zaxis * frames[3].zaxis);
    Ndot = Ndot * (frames[5].zaxis * frames[7].zaxis);
    Ndot = Ndot * (frames[6].zaxis * frames[8].zaxis);

    return product*fabs(Ndot*Xdot);
}

fastf_t
SurfaceTree::isFlatV(const ON_Surface* UNUSED(surf), ON_Plane *frames, ON_3dVector *m_normals, ON_3dPoint *UNUSED(corners), const ON_Interval& UNUSED(u), const ON_Interval& UNUSED(v))
{
	fastf_t product = 1.0;
    product *= m_normals[0]*m_normals[3];
    product *= m_normals[1]*m_normals[2];
    product *= m_normals[5]*m_normals[6];
    product *= m_normals[7]*m_normals[8];

    double Xdot = frames[0].xaxis * frames[3].xaxis;
    Xdot = Xdot * (frames[1].xaxis * frames[2].xaxis);
    Xdot = Xdot * (frames[5].xaxis * frames[6].xaxis);
    Xdot = Xdot * (frames[7].xaxis * frames[8].xaxis);

    double Ndot = frames[0].zaxis * frames[3].zaxis;
    Ndot = Ndot * (frames[1].zaxis * frames[2].zaxis);
    Ndot = Ndot * (frames[5].zaxis * frames[6].zaxis);
    Ndot = Ndot * (frames[7].zaxis * frames[8].zaxis);

    return product*fabs(Ndot*Xdot);
}

//--------------------------------------------------------------------------------
// get_closest_point implementation

typedef struct _gcp_data {
    const ON_Surface* surf;
    ON_3dPoint pt;

    // for newton iteration
    ON_3dPoint S;
    ON_3dVector du;
    ON_3dVector dv;
    ON_3dVector duu;
    ON_3dVector dvv;
    ON_3dVector duv;
} GCPData;


bool
gcp_gradient(pt2d_t out_grad, GCPData& data, pt2d_t uv)
{
    bool evaluated = data.surf->Ev2Der(uv[0],
				       uv[1],
				       data.S,
				       data.du,
				       data.dv,
				       data.duu,
				       data.duv,
				       data.dvv); // calc S(u, v) dS/du dS/dv d2S/du2 d2S/dv2 d2S/dudv
    if (!evaluated) return false;
    out_grad[0] = 2 * (data.du * (data.S - data.pt));
    out_grad[1] = 2 * (data.dv * (data.S - data.pt));
    return true;
}


bool
gcp_newton_iteration(pt2d_t out_uv, GCPData& data, pt2d_t grad, pt2d_t in_uv)
{
    ON_3dVector delta = data.S - data.pt;
    double g1du = 2 * (data.duu * delta) + 2 * (data.du * data.du);
    double g2du = 2 * (data.duv * delta) + 2 * (data.dv * data.du);
    double g1dv = g2du;
    double g2dv = 2 * (data.dvv * delta) + 2 * (data.dv * data.dv);
    mat2d_t jacob = { g1du, g1dv,
		      g2du, g2dv };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, grad);
	pt2dsub(out_uv, in_uv, tmp);
	return true;
    } else {
	std::cerr << "inverse failed!" << std::endl; // XXX fix the error handling
	return false;
    }
}


bool
get_closest_point(ON_2dPoint& outpt,
		  ON_BrepFace* face,
		  const ON_3dPoint& point,
		  SurfaceTree* tree,
		  double tolerance)
{
    int try_count = 0;
    bool delete_tree = false;
    bool found = false;
    double d_last = DBL_MAX;
    pt2d_t curr_grad;
    pt2d_t new_uv;
    GCPData data;
    data.surf = face->SurfaceOf();
    data.pt = point;

    TRACE("get_closest_point: " << PT(point));

    // get initial estimate
    SurfaceTree* a_tree = tree;
    if (a_tree == NULL) {
	a_tree = new SurfaceTree(face);
	delete_tree = true;
    }
    ON_Interval u, v;
    ON_2dPoint est = a_tree->getClosestPointEstimate(point, u, v);
    pt2d_t uv = { est[0], est[1] };

    // do the newton iterations
    // terminates on 1 of 3 conditions:
    // 1. if the gradient falls below an epsilon (preferred :-)
    // 2. if the gradient diverges
    // 3. iterated MAX_FCP_ITERATIONS
try_again:
    int diverge_count = 0;
    for (int i = 0; i < BREP_MAX_FCP_ITERATIONS; i++) {
	assert(gcp_gradient(curr_grad, data, uv));

	ON_3dPoint p = data.surf->PointAt(uv[0], uv[1]);
	double d = p.DistanceTo(point);
	TRACE("dist: " << d);

	if (NEAR_EQUAL(d, d_last, tolerance)) {
	    found = true; break;
	} else if (d > d_last) {
	    TRACE("diverged!");
	    diverge_count++;
	}
	gcp_newton_iteration(new_uv, data, curr_grad, uv);
	move(uv, new_uv);
	d_last = d;
    }
    if (found) {
	// check to see if we've left the surface domain
	double l, h;
	data.surf->GetDomain(0, &l, &h);
	if (uv[0] < l) uv[0] = l; // clamp if out of range!
	if (uv[0] > h) uv[0] = h;
	data.surf->GetDomain(1, &l, &h);
	if (uv[1] < l) uv[1] = l;
	if (uv[1] > h) uv[1] = h;

	outpt[0] = uv[0];
	outpt[1] = uv[1];
    } else {
	TRACE("FAILED TO FIND CLOSEST POINT!");
	// XXX: try the mid point of the domain -- HACK! but it seems to work!?
	if (try_count == 0) {
	    uv[0] = u.Mid();
	    uv[1] = v.Mid();
	    ++try_count;
	    goto try_again;
	}
    }

    if (delete_tree) delete a_tree;
    return found;
}


//--------------------------------------------------------------------------------
// pullback_curve implementation
typedef struct pbc_data {
    double tolerance;
    double flatness;
    const ON_Curve* curve;
    const ON_Surface* surf;
    ON_BrepFace* face;
    SurfaceTree* tree;
    ON_2dPointArray samples;
} PBCData;


typedef struct _bspline {
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    std::vector<double> params;
    std::vector<double> knots;
    ON_2dPointArray controls;
} BSpline;


bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness)
{
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}


bool
toUV(PBCData& data, ON_2dPoint& out_pt, double t)
{
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    return get_closest_point(out_pt, data.face, pointOnCurve, data.tree);
}


double
randomPointFromRange(PBCData& data, ON_2dPoint& out, double lo, double hi)
{
    assert(lo < hi);

#ifdef HAVE_DRAND48
    double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
#else
    double random_pos = rand() * (RANGE_HI - RANGE_LO) / (RAND_MAX + 1.) + RANGE_LO;
#endif

    double newt = random_pos * (hi - lo) + lo;
    assert(newt >= lo && newt <= hi);
    assert(toUV(data, out, newt));
    return newt;
}


bool
sample(PBCData& data, double t1, double t2, const ON_2dPoint& p1, const ON_2dPoint& p2)
{
    ON_2dPoint m;
    double t = randomPointFromRange(data, m, t1, t2);
    if (isFlat(p1, m, p2, data.flatness)) {
	data.samples.Append(p2);
    } else {
	sample(data, t1, t, p1, m);
	sample(data, t, t2, m, p2);
    }
    return true;
}


/**
 * this is uniform knot generation, not recommended for use with
 * chord-length parameter method... but we're not using
 * that. considering using the average method described at
 * http://www.cs.mtu.edu/~shene/COURSES/cs3621/NOTES/
 */
void
generateKnots(BSpline& bspline)
{
    int num_knots = bspline.m + 1;
    bspline.knots.resize(num_knots);
    for (int i = 0; i <= bspline.p; i++) {
	bspline.knots[i] = 0.0;
	TRACE("knot: " << bspline.knots[i]);
    }
    for (int i = 1; i <= bspline.n-bspline.p; i++) {
	bspline.knots[bspline.p+i] = ((double)i) / (bspline.n-bspline.p+1.0);
	TRACE("knot: " << bspline.knots[bspline.p+i]);
    }
    for (int i = bspline.m-bspline.p; i <= bspline.m; i++) {
	bspline.knots[i] = 1.0;
	TRACE("knot: " << bspline.knots[i]);
    }
    TRACE("knot size: " << bspline.knots.size());
}


int
getKnotInterval(BSpline& bspline, double u)
{
    int k = 0;
    while (u >= bspline.knots[k]) k++;
    k = (k == 0) ? k : k-1;
    return k;
}


int
getCoefficients(BSpline& bspline, TNT::Array1D<double>& N, double u)
{
    // evaluate the b-spline basis function for the given parameter u
    // place the results in N[]
    N = 0.0;
    if (NEAR_EQUAL(u, bspline.knots[0], BREP_FCP_ROOT_EPSILON)) {
	N[0] = 1.0;
	return 0;
    } else if (NEAR_EQUAL(u, bspline.knots[bspline.m], BREP_FCP_ROOT_EPSILON)) {
	N[bspline.n] = 1.0;
	return bspline.n;
    }
    int k = getKnotInterval(bspline, u);
    N[k] = 1.0;
    for (int d = 1; d <= bspline.p; d++) {
	double uk_1 = bspline.knots[k+1];
	double uk_d_1 = bspline.knots[k-d+1];
	N[k-d] = ((uk_1 - u)/(uk_1 - uk_d_1)) * N[k-d+1];
	for (int i = k-d+1; i <= k-1; i++) {
	    double ui = bspline.knots[i];
	    double ui_1 = bspline.knots[i+1];
	    double ui_d = bspline.knots[i+d];
	    double ui_d_1 = bspline.knots[i+d+1];
	    N[i] = ((u - ui)/(ui_d - ui)) * N[i] + ((ui_d_1 - u)/(ui_d_1 - ui_1))*N[i+1];
	}
	double uk = bspline.knots[k];
	double uk_d = bspline.knots[k+d];
	N[k] = ((u - uk)/(uk_d - uk)) * N[k];
    }
    return k;
}


// XXX: this function sucks...
void
generateParameters(BSpline& bspline)
{
    TRACE("generateParameters");
    double lastT = 0.0;
    bspline.params.resize(bspline.n+1);
    TNT::Array2D<double> N(UNIVERSAL_SAMPLE_COUNT, bspline.n+1);
    for (int i = 0; i < UNIVERSAL_SAMPLE_COUNT; i++) {
	double t = (double)i / (UNIVERSAL_SAMPLE_COUNT-1);
	TNT::Array1D<double> n = TNT::Array1D<double>(N.dim2(), N[i]);
	getCoefficients(bspline, n, t);
    }
    for (int i = 0; i < bspline.n+1; i++) {
	double max = -DBL_MAX;
	for (int j = 0; j < UNIVERSAL_SAMPLE_COUNT; j++) {
	    double f = N[j][i];
	    double t = ((double)j)/(UNIVERSAL_SAMPLE_COUNT-1);
	    if (f > max) {
		max = f;
		if (j == UNIVERSAL_SAMPLE_COUNT-1) bspline.params[i] = t;
	    } else if (f < max) {
		bspline.params[i] = lastT;
		break;
	    }
	    lastT = t;
	}
    }
    for (int i = 0; i < bspline.n+1; i++) {
    }
}


void
printMatrix(TNT::Array2D<double>& m)
{
    printf("---\n");
    for (int i = 0; i < m.dim1(); i++) {
	for (int j = 0; j < m.dim2(); j++) {
	    printf("% 5.5f ", m[i][j]);
	}
	printf("\n");
    }
}


void
generateControlPoints(BSpline& bspline, PBCData& data)
{
    TNT::Array2D<double> bigN(bspline.n+1, bspline.n+1);
    for (int i = 0; i < bspline.n+1; i++) {
	TNT::Array1D<double> n = TNT::Array1D<double>(bigN.dim2(), bigN[i]);
	getCoefficients(bspline, n, bspline.params[i]);
    }
    TNT::Array2D<double> bigD(bspline.n+1, 2);
    for (int i = 0; i < bspline.n+1; i++) {
	bigD[i][0] = data.samples[i].x;
	bigD[i][1] = data.samples[i].y;
    }

    printMatrix(bigD);
    printMatrix(bigN);

    JAMA::LU<double> lu(bigN);
    assert(lu.isNonsingular() > 0);
    TNT::Array2D<double> bigP = lu.solve(bigD); // big linear algebra black box here...

    // extract the control points
    for (int i = 0; i < bspline.n+1; i++) {
	ON_2dPoint& p = bspline.controls.AppendNew();
	p.x = bigP[i][0];
	p.y = bigP[i][1];
    }
}


ON_NurbsCurve*
newNURBSCurve(BSpline& spline)
{
    TRACE("newNURBSCurve!");

    TRACE("n: " << spline.n);
    TRACE("p: " << spline.p);

    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(2,
					  false,
					  spline.p+1,
					  spline.n+1);

    // truly - i don't know WTF openNURBS is doing here
    // when it prints out the knots, they only have multiplicity 3,
    // but yet the order of the curve is 4!!!
    int num_knots = (int)spline.knots.size() - 2;
    for (int i = 0; i < num_knots; i++) {
	double knot = spline.knots[i+1];
	TRACE("knot: " << knot);
	c->SetKnot(i, knot);
    }
    //c->ClampEnd(2);

    for (int i = 0; i < spline.controls.Count(); i++) {
	c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
}


ON_Curve*
interpolateCurve(PBCData& data)
{
    ON_Curve* curve;
    if (data.samples.Count() == 2) {
	// build a line
	curve = new ON_LineCurve(data.samples[0], data.samples[1]);
	curve->SetDomain(0.0, 1.0);
    } else {
	// build a NURBS curve, then see if it can be simplified!
	BSpline spline;
	spline.p = 3;
	spline.n = data.samples.Count()-1;
	spline.m = spline.n + spline.p + 1;
	generateKnots(spline);
	generateParameters(spline);
	generateControlPoints(spline, data);
	assert(spline.controls.Count() >= 4);
	curve = newNURBSCurve(spline);
	// XXX - attempt to simplify here!
    }
    ON_TextLog tl;
    TRACE("************** interpolateCurve");
    curve->Dump(tl);
    assert(curve->IsValid(&tl));
    return curve;
}


ON_Curve*
pullback_curve(ON_BrepFace* face,
	       const ON_Curve* curve,
	       SurfaceTree* tree,
	       double tolerance,
	       double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    double len;
    curve->GetLength(&len);
    data.flatness = (len < 1.0) ? flatness : flatness * len;

    data.curve = curve;
    data.face = face;
    data.surf = face->SurfaceOf();
    data.tree = tree;

    double u[2], v[2];
    data.surf->GetDomain(0, &u[0], &u[1]);
    data.surf->GetDomain(1, &v[0], &v[1]);
    TRACE("pullback_curve: " << PT2(u) << " | " << PT2(v));
    TRACE("pullback_curve: ");
    ON_TextLog tl;
    curve->Dump(tl);
    data.surf->Dump(tl);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = data.samples.AppendNew(); // new point is added to samples and returned
    assert(toUV(data, start, tmin));

    ON_2dPoint p1, p2;
    toUV(data, p1, tmin);
    toUV(data, p2, tmax);
    assert(sample(data, tmin, tmax, p1, p2));

    // step 2 - interpolate the samples
    return interpolateCurve(data);
}


bool
sortX(BRNode* first, BRNode* second)
{
    point_t first_min, second_min;
    point_t first_max, second_max;

    first->GetBBox(first_min, first_max);
    second->GetBBox(second_min, second_max);

    if (first_min[X] < second_min[X])
	return true;
    else
	return false;
}


bool sortY(BRNode* first, BRNode* second)
{
    point_t first_min, second_min;
    point_t first_max, second_max;

    first->GetBBox(first_min, first_max);
    second->GetBBox(second_min, second_max);

    if (first_min[Y] < second_min[Y])
	return true;
    else
	return false;
}


} /* end namespace */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
