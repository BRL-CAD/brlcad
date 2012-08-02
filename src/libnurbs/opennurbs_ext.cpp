/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2012 United States Government as represented by
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
    m_face(face), m_adj_face_index(-99)
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
    if (vdot < BREP_CURVE_FLATNESS)
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
    surf->FrameAt(u.Min(), v.Min(), frames[0]);
    surf->FrameAt(u.Max(), v.Min(), frames[1]);
    surf->FrameAt(u.Max(), v.Max(), frames[2]);
    surf->FrameAt(u.Min(), v.Max(), frames[3]);
    surf->FrameAt(u.Mid(), v.Mid(), frames[4]);
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
    if (m_root) {
	m_root->BuildBBox();
    }
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
    ON_2dPoint curr_uv(0.0, 0.0);
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
    VMOVE(min, bbox.Min());
    VMOVE(max, bbox.Max());

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
    node->m_u = u;
    node->m_v = v;
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
    node->m_u = u;
    node->m_v = v;
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
    localsurf->GetSpanVector(0, spanu);
    localsurf->GetSpanVector(1, spanv);

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
	bool split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !south || !north) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)south, (void *)north);
	    delete parent;
	    return NULL;
	}

	split = localsurf->Split(dir, vsplit, south, north);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !south || !north) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)south, (void *)north);
	    delete parent;
	    return NULL;
	}

	south->ClearBoundingBox();
	north->ClearBoundingBox();

	dir = 0;
	split = south->Split(dir, usplit, q0surf, q1surf);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !q0surf || !q1surf) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)q0surf, (void *)q1surf);
	    delete parent;
	    return NULL;
	}

	delete south;
	q0surf->ClearBoundingBox();
	q1surf->ClearBoundingBox();
	split = north->Split(dir, usplit, q3surf, q2surf);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !q3surf || !q2surf) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)q3surf, (void *)q2surf);
	    delete parent;
	    return NULL;
	}

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
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 4; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
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
	bool split = localsurf->Split(dir, usplit, east, west);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !east || !west) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)east, (void *)west);
	    delete parent;
	    return NULL;
	}

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
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 2; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
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
	bool split = localsurf->Split(dir, vsplit, south, north);

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !south || !north) {
	    bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)south, (void *)north);
	    delete parent;
	    return NULL;
	}

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
	    if (!quads[i]) {
		continue;
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->m_trimmed = false;
	    }
	    if (quads[i]->m_checkTrim) {
		parent->m_checkTrim = true;
	    }
	}
	if (m_removeTrimmed) {
	    for (int i = 0; i < 2; i++) {
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->addChild(quads[i]);
		}
	    }
	} else {
	    for (int i = 0; i < 2; i++) {
		parent->addChild(quads[i]);
	    }
	}
    } else {
	//return surfaceBBox(localsurf, true, corners, normals, u, v);
	//parent->addChild(subdivideSurface(localsurf, u, v, frames, corners, normals, 0));
	((ON_Surface *)localsurf)->ClearBoundingBox();
	delete parent;
	return subdivideSurface(localsurf, u, v, frames, corners, normals, 0, depthLimit);
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

    double width, height;
    double ratio = 5.0;
    localsurf->GetSurfaceSize(&width, &height);
    if (((width/height < ratio) && (width/height > 1.0/ratio) && isFlat(frames) && isStraight(frames))
	 || (divDepth >= depthLimit)) { //BREP_MAX_FT_DEPTH))) {
	return surfaceBBox(localsurf, true, corners, normals, u, v);
    } else {
	bool isUFlat = isFlatU(frames);
	bool isVFlat = isFlatV(frames);

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
	    bool split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);

	    /* FIXME: this needs to be handled more gracefully */
	    if (!split || !south || !north) {
		bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)south, (void *)north);
		delete parent;
		return NULL;
	    }

	    south->ClearBoundingBox();
	    north->ClearBoundingBox();

	    dir = 0;
	    split = south->Split(dir, south->Domain(dir).Mid(), q0surf, q1surf);

	    /* FIXME: this needs to be handled more gracefully */
	    if (!split || !q0surf || !q1surf) {
		bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)q0surf, (void *)q1surf);
		delete parent;
		return NULL;
	    }

	    delete south;
	    q0surf->ClearBoundingBox();
	    q1surf->ClearBoundingBox();
	    split = north->Split(dir, north->Domain(dir).Mid(), q3surf, q2surf);

	    /* FIXME: this needs to be handled more gracefully */
	    if (!split || !q3surf || !q2surf) {
		bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)q3surf, (void *)q2surf);
		delete parent;
		return NULL;
	    }

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
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->m_trimmed = false;
		}
		if (quads[i]->m_checkTrim) {
		    parent->m_checkTrim = true;
		}
	    }
	    if (m_removeTrimmed) {
		for (int i = 0; i < 4; i++) {
		    if (!quads[i]) {
			continue;
		    }
		    if (!(quads[i]->m_trimmed)) {
			parent->addChild(quads[i]);
		    }
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
	    bool split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), east, west);

	    /* FIXME: this needs to be handled more gracefully */
	    if (!split || !east || !west) {
		bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)east, (void *)west);
		delete parent;
		return NULL;
	    }

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
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->m_trimmed = false;
		}
		if (quads[i]->m_checkTrim) {
		    parent->m_checkTrim = true;
		}
	    }
	    if (m_removeTrimmed) {
		for (int i = 0; i < 2; i++) {
		    if (!quads[i]) {
			continue;
		    }
		    if (!(quads[i]->m_trimmed)) {
			parent->addChild(quads[i]);
		    }
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
	    bool split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);

	    /* FIXME: this needs to be handled more gracefully */
	    if (!split || !south || !north) {
		bu_log("DEBUG: Split failure (split:%d, surf1:%p, surf2:%p)\n", split, (void *)south, (void *)north);
		delete parent;
		return NULL;
	    }

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
		if (!quads[i]) {
		    continue;
		}
		if (!(quads[i]->m_trimmed)) {
		    parent->m_trimmed = false;
		}
		if (quads[i]->m_checkTrim) {
		    parent->m_checkTrim = true;
		}
	    }
	    if (m_removeTrimmed) {
		for (int i = 0; i < 2; i++) {
		    if (!quads[i]) {
			continue;
		    }
		    if (!(quads[i]->m_trimmed)) {
			parent->addChild(quads[i]);
		    }
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
 *  V|         +         |
 *   |                   |
 *   |    +         +    |
 *   |                   |
 *   +-------------------+
 *             U
 *
 *
 * The "+" indicates the normal sample.
 *
 * The frenet frames are stored in the frames arrays according
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
SurfaceTree::isFlat(ON_Plane *frames)
{
    double Ndot=1.0;

    /* The flatness test compares flatness criteria to running product of the normal vector of
     * the frenet frame projected onto each other normal in the frame set.
     */
    for(int i=0; i<8; i++) {
	for( int j=i+1; j<9; j++) {
	    if ((Ndot = Ndot * frames[i].zaxis * frames[j].zaxis) < BREP_SURFACE_FLATNESS) {
		    return false;
	    }
	}
    }

    return true;
}


bool
SurfaceTree::isStraight(ON_Plane *frames)
{
    double Xdot=1.0;

    /* The straightness test compares flatness criteria to running product of the tangent vector of
     * the frenet frame projected onto each other tangent in the frame set.
     */
    for(int i=0; i<8; i++) {
	for( int j=i+1; j<9; j++) {
	    if ((Xdot = Xdot * frames[0].xaxis * frames[1].xaxis) < BREP_SURFACE_FLATNESS) {
		    return false;
	    }
	}
    }

    return true;
}


bool
SurfaceTree::isFlatU(ON_Plane *frames)
{
    // check surface normals in U direction
	double Ndot = 1.0;
    if ((Ndot=frames[0].zaxis * frames[1].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[2].zaxis * frames[3].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[5].zaxis * frames[7].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[6].zaxis * frames[8].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    }

    // check for U twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames[0].xaxis * frames[1].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[2].xaxis * frames[3].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[5].xaxis * frames[7].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[6].xaxis * frames[8].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    }

    return true;
}


bool
SurfaceTree::isFlatV(ON_Plane *frames)
{
    // check surface normals in V direction
	double Ndot = 1.0;
    if ((Ndot=frames[0].zaxis * frames[3].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[1].zaxis * frames[2].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[5].zaxis * frames[6].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Ndot=Ndot * frames[7].zaxis * frames[8].zaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    }

    // check for V twist within plane
    double Xdot = 1.0;
    if ((Xdot=frames[0].xaxis * frames[3].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[1].xaxis * frames[2].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[5].xaxis * frames[6].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    } else if ((Xdot=Xdot * frames[7].xaxis * frames[8].xaxis) < BREP_SURFACE_FLATNESS) {
	return false;
    }

    return true;
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


bool
sortY(BRNode* first, BRNode* second)
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


struct Triangle {
    ON_3dPoint A, B, C;
    inline void CreateFromPoints(ON_3dPoint &_A, ON_3dPoint &_B, ON_3dPoint &_C) {
	A = _A, B = _B, C = _C;
    }
    ON_3dPoint BarycentricCoordinate(ON_3dPoint &pt)
    {
	ON_Matrix M(3, 3);
	M[0][0] = A[0], M[0][1] = B[0], M[0][2] = C[0];
	M[1][0] = A[1], M[1][1] = B[1], M[1][2] = C[1];
	M[2][0] = A[2], M[2][1] = B[2], M[2][2] = C[2];
	M.Invert(0.0);
	ON_Matrix MX(3, 1);
	MX[0][0] = pt[0], MX[1][0] = pt[1], MX[2][0] = pt[2];
	M.Multiply(M, MX);
	return ON_3dPoint(M[0][0], M[1][0], M[2][0]);
    }
};


bool
triangle_intersection(const struct Triangle &TriA, const struct Triangle &TriB, ON_3dPoint &center)
{
    ON_Plane plane_a(TriA.A, TriA.B, TriA.C);
    ON_Plane plane_b(TriB.A, TriB.B, TriB.C);
    ON_Line intersect;
    if (!ON_Intersect(plane_a, plane_b, intersect))
	return false;
    ON_3dVector line_normal = ON_CrossProduct(plane_a.Normal(), intersect.Direction());

    // dpi - >0: one side of the line, <0: another side, ==0: on the line
    double dp1 = ON_DotProduct(TriA.A - intersect.from, line_normal);
    double dp2 = ON_DotProduct(TriA.B - intersect.from, line_normal);
    double dp3 = ON_DotProduct(TriA.C - intersect.from, line_normal);

    int points_on_one_side = (dp1 >= 0) + (dp2 >= 0) + (dp3 >= 0);
    if (points_on_one_side == 0 || points_on_one_side == 3)
	return false;

    double dp4 = ON_DotProduct(TriB.A - intersect.from, line_normal);
    double dp5 = ON_DotProduct(TriB.B - intersect.from, line_normal);
    double dp6 = ON_DotProduct(TriB.C - intersect.from, line_normal);
    points_on_one_side = (dp4 >= 0) + (dp5 >= 0) + (dp6 >= 0);
    if (points_on_one_side == 0 || points_on_one_side == 3)
	return false;

    double t[4];
    int count = 0;
    double t1, t2;

    // dpi*dpj < 0 - the corresponding points are on different sides
    // - the line segment between them are intersected by the plane-plane
    // intersection line
    if (dp1*dp2 < 0) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.B, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp1*dp3 < 0) {
	intersect.ClosestPointTo(TriA.A, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp2*dp3 < 0 && count != 2) {
	intersect.ClosestPointTo(TriA.B, &t1);
	intersect.ClosestPointTo(TriA.C, &t2);
	double d1 = TriA.B.DistanceTo(intersect.PointAt(t1));
	double d2 = TriA.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (count != 2)
	return false;
    if (dp4*dp5 < 0) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.B, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.B.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp4*dp6 < 0) {
	intersect.ClosestPointTo(TriB.A, &t1);
	intersect.ClosestPointTo(TriB.C, &t2);
	double d1 = TriB.A.DistanceTo(intersect.PointAt(t1));
	double d2 = TriB.C.DistanceTo(intersect.PointAt(t2));
	if (!ZERO(d1 + d2))
	    t[count++] = t1 + d1/(d1+d2)*(t2-t1);
    }
    if (dp5*dp6 < 0 && count != 4) {
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
    int left = std::max(t[0], t[2]);
    int right = std::min(t[1], t[3]);
    if (left > right)
	return false;
    center = intersect.PointAt((left+right)/2);
    return true;
}


struct PointPair {
    int indexA, indexB;
    double distance;
    bool operator < (const PointPair &_pp) const {
	return distance < _pp.distance;
    }
};


struct Subsurface {
private:
    ON_BoundingBox m_node;
public:
    ON_Surface *m_surf;
    ON_Interval m_u, m_v;
    Subsurface *m_children[4];

    Subsurface() : m_surf(NULL)
    {
	m_children[0] = m_children[1] = m_children[2] = m_children[3] = NULL;
    }
    Subsurface(const Subsurface &_ssurf)
    {
	m_surf = _ssurf.m_surf->Duplicate();
	m_u = _ssurf.m_u;
	m_v = _ssurf.m_v;
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
	for (int i = 0; i < 4; i++)
	    m_children[i] = new Subsurface();
	ON_Surface *temp_surf1 = NULL, *temp_surf2 = NULL;
	ON_BOOL32 ret = true;
	ret = m_surf->Split(0, m_u.Mid(), temp_surf1, temp_surf2);
	if (!ret) {
	    delete temp_surf1;
	    delete temp_surf2;
	    return -1;
	}

	ret = temp_surf1->Split(1, m_v.Mid(), m_children[0]->m_surf, m_children[1]->m_surf);
	delete temp_surf1;
	if (!ret) {
	    delete temp_surf2;
	    return -1;
	}
	m_children[0]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[0]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[0]->SetBBox(m_children[0]->m_surf->BoundingBox());
	m_children[1]->m_u = ON_Interval(m_u.Min(), m_u.Mid());
	m_children[1]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[1]->SetBBox(m_children[1]->m_surf->BoundingBox());

	ret = temp_surf2->Split(1, m_v.Mid(), m_children[2]->m_surf, m_children[3]->m_surf);
	delete temp_surf2;
	if (!ret)
	    return -1;
	m_children[2]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[2]->m_v = ON_Interval(m_v.Min(), m_v.Mid());
	m_children[2]->SetBBox(m_children[2]->m_surf->BoundingBox());
	m_children[3]->m_u = ON_Interval(m_u.Mid(), m_u.Max());
	m_children[3]->m_v = ON_Interval(m_v.Mid(), m_v.Max());
	m_children[3]->SetBBox(m_children[3]->m_surf->BoundingBox());

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
};


#define INTERSECT_MAX_DEPTH 8
int
surface_surface_intersection(const ON_Surface* surfA,
			     const ON_Surface* surfB,
			     ON_SimpleArray<ON_NurbsCurve*> &intersect3d,
			     ON_SimpleArray<ON_NurbsCurve*> &intersect_uv2d,
			     ON_SimpleArray<ON_NurbsCurve*> &intersect_st2d,
			     double max_dis,
			     double)
{
    if (surfA == NULL || surfB == NULL) {
	return -1;
    }

    /* First step: Initialize the first two Subsurface.
     * It's just like getting the root of the surface tree.
     */
    typedef std::vector<std::pair<Subsurface*, Subsurface*> > NodePairs;
    NodePairs nodepairs;
    Subsurface *rootA = new Subsurface(), *rootB = new Subsurface();
    rootA->SetBBox(surfA->BoundingBox());
    rootA->m_u = surfA->Domain(0);
    rootA->m_v = surfA->Domain(1);
    rootA->m_surf = surfA->Duplicate();
    rootB->SetBBox(surfB->BoundingBox());
    rootB->m_u = surfB->Domain(0);
    rootB->m_v = surfB->Domain(1);
    rootB->m_surf = surfB->Duplicate();
    nodepairs.push_back(std::make_pair(rootA, rootB));
    ON_3dPointArray curvept;
    ON_2dPointArray curveuv, curvest;
    int bbox_count = 0;

    /* Second step: calculate the intersection of the bounding boxes, using
     * trigonal approximation.
     * Only the children of intersecting b-box pairs need to be considered.
     * The children will be generated only when they are needed, using the
     * method of splitting a NURBS surface.
     * So finally only a small subset of the surface tree is created.
     * TODO: the tolerance value should be adapted instead of using
     * INTERSECT_MAX_DEPTH. We stop going deeper when we get small enough
     * bounding boxes and intersection points that are accurate enough.
     */
    for (int h = 0; h <= INTERSECT_MAX_DEPTH; h++) {
	if (nodepairs.empty())
	    break;
	NodePairs tmp_pairs;
	if (h) {
	    for (NodePairs::iterator i = nodepairs.begin(); i != nodepairs.end(); i++) {
		int ret1 = 0;
		if ((*i).first->m_children[0] == NULL)
		    ret1 = (*i).first->Split();
		int ret2 = 0;
		if ((*i).second->m_children[0] == NULL)
		    ret2 = (*i).second->Split();
		if (ret1) {
		    if (ret2) { /* both splits failed */
			tmp_pairs.push_back(*i);
			h = INTERSECT_MAX_DEPTH;
		    } else { /* the first failed */
			for (int j = 0; j < 4; j++)
			    tmp_pairs.push_back(std::make_pair((*i).first, (*i).second->m_children[j]));
		    }
		} else {
		    if (ret2) { /* the second failed */
			for (int j = 0; j < 4; j++)
			    tmp_pairs.push_back(std::make_pair((*i).first->m_children[j], (*i).second));
		    } else { /* both success */
			for (int j = 0; j < 4; j++)
			    for (int k = 0; k < 4; k++)
				tmp_pairs.push_back(std::make_pair((*i).first->m_children[j], (*i).second->m_children[k]));
		    }
		}
	    }
	} else {
	    tmp_pairs = nodepairs;
	}
	nodepairs.clear();
	for (NodePairs::iterator i = tmp_pairs.begin(); i != tmp_pairs.end(); i++) {
	    ON_BoundingBox box_a, box_b, box_intersect;
	    (*i).first->GetBBox(box_a.m_min, box_a.m_max);
	    (*i).second->GetBBox(box_b.m_min, box_b.m_max);
	    if (box_intersect.Intersection(box_a, box_b)) {
		if (h == INTERSECT_MAX_DEPTH) {
		    // We have arrived at the bottom of the trees.
		    // Get an estimate of the intersection point lying on the intersection curve

		    // Get the corners of each surface sub-patch inside the bounding-box.
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
			    curvept.Append(average);
			    curveuv.Append(avguv);
			    curvest.Append(avgst);
			}
		    }
		    bbox_count++;
		} else {
		    nodepairs.push_back(*i);
		}
	    }
	}
	/* for (int i = 0; i < curvept.Count(); i++) {
	    bu_log("(%lf %lf %lf)\n", curvept[i].x, curvept[i].y, curvept[i].z);
	}
	bu_log("%d %d\n", h, tmp_pairs.size());*/
    }
    bu_log("We get %d intersection bounding boxes.\n", bbox_count);
    bu_log("%d points on the intersection curves.\n", curvept.Count());

    if (!curvept.Count()) {
	delete rootA;
	delete rootB;
	return 0;
    }

    // Third step: Fit the points in curvept into NURBS curves.
    // Here we use polyline approximation.
    // TODO: Find a better fitting algorithm unless this is good enough.

    if (!(max_dis > 0.0)) {
	// max_dis <= 0.0 means that we need to automatically generate a threshold.
	if (ZERO(surfA->BoundingBox().Volume())) {
	    max_dis = pow(surfB->BoundingBox().Volume(), 1.0/3.0) * 0.2;
	} else if (ZERO(surfB->BoundingBox().Volume())) {
	    max_dis = pow(surfA->BoundingBox().Volume(), 1.0/3.0) * 0.2;
	} else {
	    max_dis = pow(surfA->BoundingBox().Volume()*surfB->BoundingBox().Volume(), 1.0/6.0) * 0.2;
	}
    }
    bu_log("max_dis: %lf\n", max_dis);
    // NOTE: More tests are needed to find a better threshold.

    std::vector<PointPair> ptpairs;
    for (int i = 0; i < curvept.Count(); i++) {
	for (int j = i + 1; j < curvept.Count(); j++) {
	    PointPair ppair;
	    ppair.distance = curvept[i].DistanceTo(curvept[j]);
	    if (ppair.distance < max_dis) {
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
    for (unsigned int i = 0; i < polylines.size(); i++) {
	if (polylines[i] != NULL) {
	    int startpoint = (*polylines[i])[0];
	    int endpoint = (*polylines[i])[polylines[i]->Count() - 1];
	    if (curvept[startpoint].DistanceTo(curvept[endpoint]) < max_dis) {
		polylines[i]->Append(startpoint);
	    }

	    // The intersection curves in the 3d space
	    ON_3dPointArray ptarray;
	    for (int j = 0; j < polylines[i]->Count(); j++)
		ptarray.Append(curvept[(*polylines[i])[j]]);
	    ON_PolylineCurve curve(ptarray);
	    ON_NurbsCurve *nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect3d.Append(nurbscurve);
	    }

	    // The intersection curves in the 2d UV parameter space (surfA)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curveuv[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = ON_PolylineCurve(ptarray);
	    curve.ChangeDimension(2);
	    nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect_uv2d.Append(nurbscurve);
	    }

	    // The intersection curves in the 2d UV parameter space (surfB)
	    ptarray.Empty();
	    for (int j = 0; j < polylines[i]->Count(); j++) {
		ON_2dPoint &pt2d = curvest[(*polylines[i])[j]];
		ptarray.Append(ON_3dPoint(pt2d.x, pt2d.y, 0.0));
	    }
	    curve = ON_PolylineCurve(ptarray);
	    curve.ChangeDimension(2);
	    nurbscurve = ON_NurbsCurve::New();
	    if (curve.GetNurbForm(*nurbscurve)) {
		intersect_st2d.Append(nurbscurve);
	    }

	    delete polylines[i];
	}
    }
    
    bu_log("Segments: %d\n", intersect3d.Count());
    bu_free(index, "int");
    bu_free(startpt, "int");
    bu_free(endpt, "int");
    delete rootA;
    delete rootB;

    // WIP
    return 0;
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
