/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2014 United States Government as represented by
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

#include "vmath.h"
#include "bu.h"

#include "brep.h"
#include "libbrep_brep_tools.h"
#include "dvec.h"

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

void
brep_get_plane_ray(ON_Ray& r, plane_ray& pr)
{
    vect_t v1;
    VMOVE(v1, r.m_dir);
    fastf_t min = MAX_FASTF;
    int index = -1;
    for (int i = 0; i < 3; i++) {
	// find the smallest component
	if (fabs(v1[i]) < min) {
	    min = fabs(v1[i]);
	    index = i;
	}
    }
    v1[index] += 1; // alter the smallest component
    VCROSS(pr.n1, v1, r.m_dir); // n1 is perpendicular to v1
    VUNITIZE(pr.n1);
    VCROSS(pr.n2, pr.n1, r.m_dir);       // n2 is perpendicular to v1 and n1
    VUNITIZE(pr.n2);
    pr.d1 = VDOT(pr.n1, r.m_origin);
    pr.d2 = VDOT(pr.n2, r.m_origin);
    TRACE1("n1:" << ON_PRINT3(pr.n1) << " n2:" << ON_PRINT3(pr.n2) << " d1:" << pr.d1 << " d2:" << pr.d2);
}


void
brep_r(const ON_Surface* surf, plane_ray& pr, pt2d_t uv, ON_3dPoint& pt, ON_3dVector& su, ON_3dVector& sv, pt2d_t R)
{
    vect_t vp;

    assert(surf->Ev1Der(uv[0], uv[1], pt, su, sv));
    VMOVE(vp, pt);

    R[0] = VDOT(pr.n1, vp) - pr.d1;
    R[1] = VDOT(pr.n2, vp) - pr.d2;
}


void
brep_newton_iterate(plane_ray& pr, pt2d_t R, ON_3dVector& su, ON_3dVector& sv, pt2d_t uv, pt2d_t out_uv)
{
    vect_t vsu, vsv;
    VMOVE(vsu, su);
    VMOVE(vsv, sv);

    mat2d_t jacob = { VDOT(pr.n1, vsu), VDOT(pr.n1, vsv),
		      VDOT(pr.n2, vsu), VDOT(pr.n2, vsv) };
    mat2d_t inv_jacob;
    if (mat2d_inverse(inv_jacob, jacob)) {
	// check inverse validity
	pt2d_t tmp;
	mat2d_pt2d_mul(tmp, inv_jacob, R);
	pt2dsub(out_uv, uv, tmp);
    } else {
	TRACE2("inverse failed"); // FIXME: how to handle this?
	move(out_uv, uv);
    }
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
CurveTree::CurveTree(const ON_BrepFace* face) :
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
		std::list<fastf_t> splitlist;
		for (int knot_index = 1; knot_index <= knotcnt; knot_index++) {
		    ON_Interval range(knots[knot_index - 1], knots[knot_index]);

		    if (range.Length() > TOL)
			getHVTangents(trimCurve, range, splitlist);
		}
		for (std::list<fastf_t>::iterator l = splitlist.begin(); l != splitlist.end(); l++) {
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


bool
CurveTree::getHVTangents(const ON_Curve* curve, ON_Interval& t, std::list<fastf_t>& list)
{
    double x;
    double midpoint = (t[1]+t[0])/2.0;
    ON_Interval left(t[0], midpoint);
    ON_Interval right(midpoint, t[1]);
    int status = ON_Curve_Has_Tangent(curve, t[0], t[1], TOL);

    switch (status) {

	case 1: /* 1 Vertical tangent */
	    x = ON_Curve_Get_Vertical_Tangent(curve, t[0], t[1], TOL2);
	    list.push_back(x);
	    return true;

	case 2: /* 1 Horizontal tangent */
	    x = ON_Curve_Get_Horizontal_Tangent(curve, t[0], t[1], TOL2);
	    list.push_back(x);
	    return true;

	case 3: /* Horizontal and vertical tangents present - Simple midpoint split */
	    if (left.Length() > TOL)
		getHVTangents(curve, left, list);
	    if (right.Length() > TOL)
		getHVTangents(curve, right, list);
	    return true;

	default:
	    return false;

    }

    return false;  //Should never get here
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
    VSETALL(minpt, INFINITY);
    VSETALL(maxpt, -INFINITY);
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
SurfaceTree::SurfaceTree()
    : m_removeTrimmed(false),
      ctree(NULL),
      m_face(NULL),
      m_root(NULL)
{
}


SurfaceTree::SurfaceTree(const ON_BrepFace* face, bool removeTrimmed, int depthLimit)
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
    if (!surf) {
	TRACE("ERROR: NULL surface encountered in SurfaceTree()");
	return;
    }

    TRACE("Creating surface tree for: " << face->m_face_index);
    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    double uq = u.Length()*0.25;
    double vq = v.Length()*0.25;

    ///////////////////////////////////////////////////////////////////////
    // Populate initial frames array for use in tree build
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

    m_root = subdivideSurface(surf, u, v, frames, 0, depthLimit, 1);
    if (m_root) {
	m_root->BuildBBox();
    }
    TRACE("u: [" << u[0] << ", " << u[1] << "]");
    TRACE("v: [" << v[0] << ", " << v[1] << "]");
    TRACE("m_root: " << m_root);
    while (!f_queue.empty()) {
	bu_free(f_queue.front(), "free subsurface frames array");
	f_queue.pop();
    }
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
brep_getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv , BBNode* node) {
    plane_ray pr;
    const ON_Surface *surf = node->m_face->SurfaceOf();
    double umin, umax;
    double vmin, vmax;
    surf->GetDomain(0, &umin, &umax);
    surf->GetDomain(1, &vmin, &vmax);

    ON_3dVector dir = node->m_normal;
    dir.Reverse();
    ON_Ray ray((ON_3dPoint&)pt, dir);
    brep_get_plane_ray(ray, pr);

    //know use this as guess to iterate to closer solution
    pt2d_t Rcurr;
    pt2d_t new_uv;
    ON_3dVector su, sv;
    bool found=false;
    fastf_t Dlast = MAX_FASTF;
    pt2d_t nuv;
    nuv[0] = (node->m_u[1] + node->m_u[0])/2.0;
    nuv[1] = (node->m_v[1] + node->m_v[0])/2.0;
    ON_3dPoint newpt;
    for (int i = 0; i < BREP_MAX_ITERATIONS; i++) {
	brep_r(surf, pr, nuv, newpt, su, sv, Rcurr);
	fastf_t d = v2mag(Rcurr);

	if (d < BREP_INTERSECTION_ROOT_EPSILON) {
	    TRACE1("R:"<<ON_PRINT2(Rcurr));
	    found = true;
	    break;
	} else if (d < BREP_INTERSECTION_ROOT_SETTLE) {
	    found = true;
	}
	brep_newton_iterate(pr, Rcurr, su, sv, nuv, new_uv);

	//Check for closed surface wrap around
	if (surf->IsClosed(0)) {
	    if (new_uv[0] < umin) {
		new_uv[0] = umin;
	    } else if (new_uv[0] > umax) {
		new_uv[0] = umax;
	    }
	}
	if (surf->IsClosed(1)) {
	    if (new_uv[1] < vmin) {
		new_uv[1] = vmin;
	    } else if (new_uv[1] > vmax) {
		new_uv[1] = vmax;
	    }
	}
#ifdef HOOD
	//push answer back to within node bounds
	double ufluff = (node->m_u[1] - node->m_u[0])*0.01;
	double vfluff = (node->m_v[1] - node->m_v[0])*0.01;
#else
	//push answer back to within node bounds
	double ufluff = 0.0;
	double vfluff = 0.0;
#endif
	if (new_uv[0] < node->m_u[0] - ufluff)
	    new_uv[0] = node->m_u[0];
	else if (new_uv[0] > node->m_u[1] + ufluff)
	    new_uv[0] = node->m_u[1];

	if (new_uv[1] < node->m_v[0] - vfluff)
	    new_uv[1] = node->m_v[0];
	else if (new_uv[1] > node->m_v[1] + vfluff)
	    new_uv[1] = node->m_v[1];


	surface_EvNormal(surf,new_uv[0], new_uv[1], newpt, ray.m_dir);
	ray.m_dir.Reverse();
	brep_get_plane_ray(ray, pr);

	if (d < Dlast) {
	    move(nuv, new_uv);
	    Dlast = d;
	}
    }
    if (found) {
	uv.x = nuv[0];
	uv.y = nuv[1];
	return 1;
    }
    return -1;
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
	if (brep_getSurfacePoint(pt, curr_uv, node)) {
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

    nodes.clear();
    (void)m_root->getLeavesBoundingPoint(pt, nodes);
    for (i = nodes.begin(); i != nodes.end(); i++) {
	BBNode* node = (*i);
	if (brep_getSurfacePoint(pt, curr_uv, node)) {
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

    return -1;
}


//static int bb_cnt=0;
BBNode*
SurfaceTree::surfaceBBox(const ON_Surface *localsurf, bool isLeaf, ON_Plane *m_frames, const ON_Interval& u, const ON_Interval& v)
{
    point_t min, max, buffer;
    ON_BoundingBox bbox = localsurf->BoundingBox();

    VSETALL(buffer, BREP_EDGE_MISS_TOLERANCE);

    //bu_log("in bb%d rpp %f %f %f %f %f %f\n", bb_cnt, min[0], max[0], min[1], max[1], min[2], max[2]);
    VMOVE(min, bbox.Min());
    VMOVE(max, bbox.Max());

    // calculate the estimate point on the surface: i.e. use the point
    // on the surface defined by (u.Mid(), v.Mid()) as a heuristic for
    // finding the uv domain bounding a portion of the surface close
    // to an arbitrary model-space point (see
    // getClosestPointEstimate())
    ON_3dPoint estimate;
    ON_3dVector normal;
    estimate = m_frames[4].origin;
    normal = m_frames[4].zaxis;

    BBNode* node;
    if (isLeaf) {
	/* vect_t delta; */

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
    if (!surface_EvNormal(surf,surf->Domain(0).Mid(), surf->Domain(1).Mid(), estimate, normal)) {
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
SurfaceTree::subdivideSurface(const ON_Surface *localsurf,
			      const ON_Interval& u,
			      const ON_Interval& v,
			      ON_Plane frames[],
			      int divDepth,
			      int depthLimit,
			      int prev_knot
    )
{
    BBNode* quads[4];
    BBNode* parent = NULL;
    double usplit = 0.0;
    double vsplit = 0.0;
    double width = 0.0;
    double height = 0.0;
    double ratio = 5.0;
    double uq = u.Length()*0.25;
    double vq = v.Length()*0.25;
    localsurf->FrameAt(u.Mid() - uq, v.Mid() - vq, frames[5]);
    localsurf->FrameAt(u.Mid() - uq, v.Mid() + vq, frames[6]);
    localsurf->FrameAt(u.Mid() + uq, v.Mid() - vq, frames[7]);
    localsurf->FrameAt(u.Mid() + uq, v.Mid() + vq, frames[8]);
    unsigned int do_u_split = 0;
    unsigned int do_v_split = 0;
    unsigned int do_both_splits = 0;

    usplit = u.Mid();
    vsplit = v.Mid();

    // The non-knot case where all criteria are satisfied is the
    // terminating case for the recursion - handle that first
    if (!prev_knot) {
	localsurf->GetSurfaceSize(&width, &height);
	if (((width/height < ratio) && (width/height > 1.0/ratio) && isFlat(frames) && isStraight(frames))
	    || (divDepth >= depthLimit)) { //BREP_MAX_FT_DEPTH
	    return surfaceBBox(localsurf, true, frames, u, v);
	}
    }

    // Knots
    if (prev_knot) {
	int spanu_cnt = localsurf->SpanCount(0);
	int spanv_cnt = localsurf->SpanCount(1);
	parent = initialBBox(ctree, localsurf, m_face, u, v);
	if (spanu_cnt > 1) {
	    double *spanu = new double[spanu_cnt+1];
	    localsurf->GetSpanVector(0, spanu);
	    do_u_split = 1;
	    usplit = spanu[(spanu_cnt+1)/2];
	    delete [] spanu;
	}
	if (spanv_cnt > 1) {
	    double *spanv = new double[spanv_cnt+1];
	    localsurf->GetSpanVector(1, spanv);
	    do_v_split = 1;
	    vsplit = spanv[(spanv_cnt+1)/2];
	    delete [] spanv;
	}
	if (do_u_split && do_v_split) {
	    do_both_splits = 1;
	    do_u_split = 0;
	    do_v_split = 0;
	}
    }
    // Flatness
    if (!prev_knot) {
	bool isUFlat = isFlatU(frames);
	bool isVFlat = isFlatV(frames);

	parent = (divDepth == 0) ? initialBBox(ctree, localsurf, m_face, u, v) : surfaceBBox(localsurf, false, frames, u, v);

	if ((!isVFlat || (width/height > ratio)) && (!isUFlat || (height/width > ratio))) {
	    do_both_splits = 1;
	}
	if ((!isUFlat || (width/height > ratio)) && !do_both_splits) {
	    do_u_split = 1;
	}
	if (!do_both_splits && ! do_u_split) {
	    do_v_split = 1;
	}
    }

    ///////////////////////////////////

    if (do_both_splits) {
	ON_Interval firstu(u.Min(), usplit);
	ON_Interval secondu(usplit, u.Max());
	ON_Interval firstv(v.Min(), vsplit);
	ON_Interval secondv(vsplit, v.Max());
	ON_BoundingBox box = localsurf->BoundingBox();

	ON_Surface *q0surf = NULL;
	ON_Surface *q1surf = NULL;
	ON_Surface *q2surf = NULL;
	ON_Surface *q3surf = NULL;

	bool split = ON_Surface_Quad_Split(localsurf, u, v, usplit, vsplit, &q0surf, &q1surf, &q2surf, &q3surf);
	/* FIXME: this needs to be handled more gracefully */
	if (!split) {
	    delete parent;
	    return NULL;
	}
	q0surf->ClearBoundingBox();
	q1surf->ClearBoundingBox();
	q2surf->ClearBoundingBox();
	q3surf->ClearBoundingBox();

	/*********************************************************************
	 * In order to avoid fairly expensive re-calculation of 3d points at
	 * uv coordinates, all values that are shared between children at
	 * the same depth of a surface subdivision are pre-computed and
	 * passed as parameters.
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
	localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
	localsurf->FrameAt(u.Min(), vsplit, sharedframes[1]);
	localsurf->FrameAt(usplit, v.Max(), sharedframes[2]);
	localsurf->FrameAt(u.Max(), vsplit, sharedframes[3]);
	// When splitting via knots, we don't know what point frames[4] is until
	// the knot is selected
	if (prev_knot) localsurf->FrameAt(usplit, vsplit, frames[4]);

	ON_Plane *newframes;
	if (!f_queue.empty()) {newframes = f_queue.front(); f_queue.pop();} else {newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");}
	newframes[0] = frames[0];
	newframes[1] = sharedframes[0];
	newframes[2] = frames[4];
	newframes[3] = sharedframes[1];
	newframes[4] = frames[5];
	quads[0] = subdivideSurface(q0surf, firstu, firstv, newframes, divDepth+1, depthLimit, prev_knot);
	delete q0surf;
	newframes[0] = sharedframes[0];
	newframes[1] = frames[1];
	newframes[2] = sharedframes[3];
	newframes[3] = frames[4];
	newframes[4] = frames[7];
	quads[1] = subdivideSurface(q1surf, secondu, firstv, newframes, divDepth+1, depthLimit, prev_knot);
	delete q1surf;
	newframes[0] = frames[4];
	newframes[1] = sharedframes[3];
	newframes[2] = frames[2];
	newframes[3] = sharedframes[2];
	newframes[4] = frames[8];
	quads[2] = subdivideSurface(q2surf, secondu, secondv, newframes, divDepth+1, depthLimit, prev_knot);
	delete q2surf;
	newframes[0] = sharedframes[1];
	newframes[1] = frames[4];
	newframes[2] = sharedframes[2];
	newframes[3] = frames[3];
	newframes[4] = frames[6];
	quads[3] = subdivideSurface(q3surf, firstu, secondv, newframes, divDepth+1, depthLimit, prev_knot);
	delete q3surf;
	memset(newframes, 0, 9 * sizeof(ON_Plane *));
	f_queue.push(newframes);

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
	parent->BuildBBox();
	return parent;
    }
    //////////////////////////////////////
    if (do_u_split) {
	bool split;
	ON_Interval firstu(u.Min(), usplit);
	ON_Interval secondu(usplit, u.Max());
	ON_Surface *east = NULL;
	ON_Surface *west = NULL;

	ON_BoundingBox box = localsurf->BoundingBox();

	int dir = 0;
	if (prev_knot) {
	    split = localsurf->Split(dir, usplit, east, west);
	} else {
	    split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), east, west);
	}

	/* FIXME: this needs to be handled more gracefully */
	if (!split || !east || !west) {
	    bu_log("DEBUG: Split failure (split:%d, east:%p, west:%p)\n", split, (void *)east, (void *)west);
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
	 * passed as parameters.
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
	localsurf->FrameAt(usplit, v.Min(), sharedframes[0]);
	localsurf->FrameAt(usplit, v.Max(), sharedframes[1]);

	ON_Plane *newframes;
	if (!f_queue.empty()) {newframes = f_queue.front(); f_queue.pop();} else {newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");}
	newframes[0] = frames[0];
	newframes[1] = sharedframes[0];
	newframes[2] = sharedframes[1];
	newframes[3] = frames[3];

	if (prev_knot) {
	    localsurf->FrameAt(firstu.Mid(), v.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(east, firstu, v, newframes, divDepth+1, depthLimit, prev_knot);
	} else {
	    ON_Interval first(0, 0.5);
	    localsurf->FrameAt(u.Mid() - uq, v.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(east, u.ParameterAt(first), v, newframes, divDepth + 1, depthLimit, prev_knot);
	}
	delete east;

	newframes[0] = sharedframes[0];
	newframes[1] = frames[1];
	newframes[2] = frames[2];
	newframes[3] = sharedframes[1];

	if (prev_knot) {
	    localsurf->FrameAt(secondu.Mid(), v.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(west, secondu, v, newframes, divDepth+1, depthLimit, prev_knot);
	} else {
	    ON_Interval second(0.5, 1.0);
	    localsurf->FrameAt(u.Mid() + uq, v.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(west, u.ParameterAt(second), v, newframes, divDepth + 1, depthLimit, prev_knot);
	}
	delete west;

	memset(newframes, 0, 9 * sizeof(ON_Plane *));
	f_queue.push(newframes);

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
	parent->BuildBBox();
	return parent;
    }
    if (do_v_split || !prev_knot) {
	bool split;
	ON_Interval firstv(v.Min(), vsplit);
	ON_Interval secondv(vsplit, v.Max());

	//////////////////////////////////////
	ON_Surface *north = NULL;
	ON_Surface *south = NULL;

	ON_BoundingBox box = localsurf->BoundingBox();

	int dir = 1;
	if (prev_knot) {
	    split = localsurf->Split(dir, vsplit, south, north);
	} else {
	    split = localsurf->Split(dir, localsurf->Domain(dir).Mid(), south, north);
	}

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
	 * passed as parameters.
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
	localsurf->FrameAt(u.Min(), vsplit, sharedframes[0]);
	localsurf->FrameAt(u.Max(), vsplit, sharedframes[1]);

	ON_Plane *newframes;
	if (!f_queue.empty()) {newframes = f_queue.front(); f_queue.pop();} else {newframes = (ON_Plane *)bu_malloc(9*sizeof(ON_Plane), "new frames");}
	newframes[0] = frames[0];
	newframes[1] = frames[1];
	newframes[2] = sharedframes[1];
	newframes[3] = sharedframes[0];
	if (prev_knot) {
	    localsurf->FrameAt(u.Mid(), firstv.Mid(), newframes[4]);
	    quads[0] = subdivideSurface(south, u, firstv, newframes, divDepth+1, depthLimit, prev_knot);
	} else {
	    ON_Interval first(0, 0.5);
	    localsurf->FrameAt(u.Mid(), v.Mid() - vq, newframes[4]);
	    quads[0] = subdivideSurface(south, u, v.ParameterAt(first), newframes, divDepth + 1, depthLimit, prev_knot);
	}
	delete south;

	newframes[0] = sharedframes[0];
	newframes[1] = sharedframes[1];
	newframes[2] = frames[2];
	newframes[3] = frames[3];

	if (prev_knot) {
	    localsurf->FrameAt(u.Mid(), secondv.Mid(), newframes[4]);
	    quads[1] = subdivideSurface(north, u, secondv, newframes, divDepth+1, depthLimit, prev_knot);
	} else {
	    ON_Interval second(0.5, 1.0);
	    localsurf->FrameAt(u.Mid(), v.Mid() + vq, newframes[4]);
	    quads[1] = subdivideSurface(north, u, v.ParameterAt(second), newframes, divDepth + 1, depthLimit, prev_knot);
	}
	delete north;

	memset(newframes, 0, 9 * sizeof(ON_Plane *));
	f_queue.push(newframes);

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

	parent->BuildBBox();
	return parent;
    }

    if (!do_both_splits && !do_u_split && !do_v_split) {
	((ON_Surface *)localsurf)->ClearBoundingBox();
	delete parent;
	return subdivideSurface(localsurf, u, v, frames, 0, depthLimit, 0);
    }

    // Should never get here
    return NULL;
}


bool
SurfaceTree::isFlat(ON_Plane *frames)
{
    return ON_Surface_IsFlat(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isStraight(ON_Plane *frames)
{
    return ON_Surface_IsStraight(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isFlatU(ON_Plane *frames)
{
    return ON_Surface_IsFlat_U(frames, BREP_SURFACE_FLATNESS);
}


bool
SurfaceTree::isFlatV(ON_Plane *frames)
{
    return ON_Surface_IsFlat_V(frames, BREP_SURFACE_FLATNESS);
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
    pt2d_t curr_grad = {0.0, 0.0};
    pt2d_t new_uv = {0.0, 0.0};
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
    pt2d_t uv = {est[0], est[1]};

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
	if (gcp_newton_iteration(new_uv, data, curr_grad, uv)) {
	    move(uv, new_uv);
	} else {
	    // iteration failed, nudge diagonally
	    uv[0] += VUNITIZE_TOL;
	    uv[1] += VUNITIZE_TOL;
	}
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

    if (delete_tree)
	delete a_tree;
    return found;
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


} /* end namespace */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
