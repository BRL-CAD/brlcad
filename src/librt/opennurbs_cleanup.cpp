/*               O P E N N U R B S _ E X T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2009 United States Government as represented by
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
 * Author -
 *   Jason Owens
 */

#include "common.h"

#include "bio.h"
#include <assert.h>
#include <vector>
#include <limits>

#include "tnt.h"
#include "jama_lu.h"

#include "vmath.h"
#include "bu.h"
#include "vector.h"
#include "opennurbs_ext.h"


// grows 3D BBox along each axis by this factor
#define BBOX_GROW_3D 0.1


namespace brlcad {
    //--------------------------------------------------------------------------------
    // CurveTree
    CurveTree::CurveTree(const ON_BrepFace* face)
	: m_face(face) {
	    // The initial root node of the tree contains only the face
	    // pointer and will hold as children nodes referencing the
	    // the loops.
	    m_root = new BRNode(face);
	    // Now, for each loop create a Loop BANode and add it as
	    // a child to the root face node.
	    for (int i = 0; i < face->LoopCount(); i++) {
		bool innerLoop = (i > 0) ? true : false;
		const ON_BrepLoop* loop = face->Loop(i);
		BRNode* loopnode = new BRNode(face,loop,innerLoop);
		
		m_root->addChild(loopnode);
		// For each loop, break it down into trims and add
		// the trims as children to the Loop Node.
		for (int j = 0; j < loop->m_ti.Count(); j++) {
		    // Extract info on the particular trim
		    ON_BrepTrim& trim = face->Brep()->m_T[loop->m_ti[j]];
		    const ON_Curve* trimCurve = trim.TrimCurveOf();
		    double min,max;
		    (void)trimCurve->GetDomain(&min, &max);
		    ON_Interval t(min,max);
		    BRNode* trimnode = new BRNode(face,loop,trimCurve,t,innerLoop);
		    // Break up the trimming curve into managable sub-segments,
		    // identify the knots and splice the curve into knot-to-knot
		    // sub-intervals.
		    int knotcnt = trimCurve->SpanCount();
		    double *knots = new double[knotcnt+1];
		    trimCurve->GetSpanVector(knots);
		    // If Linear, simple - just walk the knots and add
		    if (trimCurve->IsLinear()) {
			for (int i = 1; i <= knotcnt; i++) {
		           double xmax = knots[i];
			   ON_Interval tc(min,xmax);
	       		   if (!NEAR_ZERO(xmax-min,0.000001)) {
	    		      BRNode* childnode = new BRNode(face,loop,trimCurve,tc,innerLoop);
			      trimnode->addChild(childnode);
       			   }
			   min = xmax;
			}			   
		    } else {
			// Not Linear - walk the knot sub-intervals, split
			// further based on horizontal and vertical tangents. 
			list<double> splitlist;
			for(int i=1;i<=knotcnt;i++) {
			    ON_Interval t(knots[i-1],knots[i]);
			    getHVTangents(trimCurve,t,splitlist);
			}
			// Once split, pass each sub-interval to a routine that
			// will build a tree containing nodes and leaves with 
			// sub-intervals at the leaves satisfying the linearity 
			// criteria using trimnode as the tree root.  Once this 
			// is done, add trimnode to the loopnode child list.
			for( list<double>::iterator l=splitlist.begin();l != splitlist.end();l++) {
			    double xmax = *l;
			    if (!NEAR_ZERO(xmax-min,0.000001)) {
				GetBAChildren(trimnode,1,min,xmax);
				loopnode->addChild(trimnode);
			    }
			    min = xmax;
			}
		    }
		    delete knots;
		    // Build loopnode bbox from child nodes
		    ParentBBBuild(loopnode);
		}
		// Build m_root bbox from child nodes
		ParentBBBuild(m_root);
	    }
	    getLeaves(m_sortedY);
	    m_sortedY.sort(sortY);
	    return;
	}

	CurveTree::~CurveTree() {
	    delete m_root;
	}

	BRNode*
	CurveTree::getRootNode() const {
	    return m_root;
	}

	int
	CurveTree::depth() {
	    return m_root->depth();
	}

	void
	CurveTree::getLeaves(list<BRNode*>& out_leaves) {
	    m_root->getLeaves(out_leaves);
	}

	
	void
	CurveTree::getLeavesAbove(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v) {
	    point_t bmin,bmax;
	    double dist;
	    for (list<BRNode*>::iterator i = m_sortedX.begin(); i != m_sortedX.end(); i++) {
		BRNode* br = dynamic_cast<BRNode*>(*i);
		br->GetBBox(bmin,bmax);
		dist = 0.000001;//0.03*DIST_PT_PT(bmin,bmax);
		if (bmax[X]+dist < u[0])
		    continue;
		if (bmin[X]-dist < u[1]) {
		    if (bmax[Y]+dist > v[0]) {
			out_leaves.push_back(br);
		    }
		}
	    }
	}
	
	
	fastf_t CurveTree::getVerticalTangent(const ON_Curve *curve,fastf_t min,fastf_t max) {
	    fastf_t mid;
	    ON_3dVector tangent;
	    bool tanmin;
	    
    	    // first lets check end points
    	    tangent = curve->TangentAt(max);
    	    if (NEAR_ZERO(tangent.x,0.00001) )
		return max;
	    tangent = curve->TangentAt(min);
    	    if (NEAR_ZERO(tangent.x,0.00001) )
		return min;
	    
	    tanmin = (tangent[X] < 0.0);
	    while ( (max-min) > 0.00001 ) {
		mid = (max + min)/2.0;
		tangent = curve->TangentAt(mid);
		if (NEAR_ZERO(tangent[X], 0.00001)) {
		    return mid;
		}
		if ( (tangent[X] < 0.0) == tanmin ) {
		    min = mid;
		} else {
		    max = mid;
		}
	    }
	    return min;
	}
	
	
	fastf_t CurveTree::getHorizontalTangent(const ON_Curve *curve,fastf_t min,fastf_t max) {
	    fastf_t mid;
	    ON_3dVector tangent;
	    bool tanmin;
	    
    	    // first lets check end points
	    tangent = curve->TangentAt(max);
    	    if (NEAR_ZERO(tangent.y,0.00001) )
		return max;
    	    tangent = curve->TangentAt(min);
    	    if (NEAR_ZERO(tangent.y,0.00001) )
		return min;
	    
	    tanmin = (tangent[Y] < 0.0);
	    while ( (max-min) > 0.00001 ) {
		mid = (max + min)/2.0;
		tangent = curve->TangentAt(mid);
		if (NEAR_ZERO(tangent[Y], 0.00001)) {
		    return mid;
		}
		if ( (tangent[Y] < 0.0) == tanmin ) {
		    min = mid;
		} else {
		    max = mid;
		}
	    }
	    return min;
	}
	
	bool CurveTree::getHVTangents(const ON_Curve* curve, ON_Interval& t, list<fastf_t>& list) {
	    bool tanx1,tanx2,tanx_changed;
	    bool tany1,tany2,tany_changed;
	    bool tan_changed;
	    ON_3dVector tangent1,tangent2;
	    ON_3dPoint p1,p2;
	    
	    tangent1 = curve->TangentAt(t[0]);
	    tangent2 = curve->TangentAt(t[1]);
	    
	    tanx1 = (tangent1[X] < 0.0);
	    tanx2 = (tangent2[X] < 0.0);
	    tany1 = (tangent1[Y] < 0.0);
	    tany2 = (tangent2[Y] < 0.0);
	    
	    tanx_changed =(tanx1 != tanx2);
	    tany_changed =(tany1 != tany2);
	    
	    tan_changed = tanx_changed || tany_changed;
	    
	    if ( tan_changed ) {
		if (tanx_changed && tany_changed) {//horz & vert simply split
		    double midpoint = (t[1]+t[0])/2.0;
		    ON_Interval left(t[0],midpoint);
		    ON_Interval right(midpoint,t[1]);
		    getHVTangents(curve, left, list);
		    getHVTangents(curve, right, list);
		    return true;
		} else if (tanx_changed) {//find horz
		    double x = getVerticalTangent(curve,t[0],t[1]);
		    list.push_back(x);
		} else { //find vert
		    double x = getHorizontalTangent(curve,t[0],t[1]);
		    list.push_back(x);
		}
	    } else { // check point slope for change
		bool slopex,slopex_changed;
		bool slopey,slopey_changed;
		bool slope_changed;
		fastf_t xdelta,ydelta;
		
		p1 = curve->PointAt(t[0]);
		p2 = curve->PointAt(t[1]);
		
		xdelta = (p2[X] - p1[X]);
		slopex = (xdelta < 0.0);
		ydelta = (p2[Y] - p1[Y]);
		slopey = (ydelta < 0.0);
		
		if ( NEAR_ZERO(xdelta, 0.000001) || NEAR_ZERO(ydelta, 0.000001)) {
		    return true;
		}
		
		slopex_changed = (slopex != tanx1);
		slopey_changed = (slopey != tany1);
		
		slope_changed = slopex_changed || slopey_changed;
		
		if (slope_changed) {  //2 horz or 2 vert changes simply split
		    double midpoint = (t[1]+t[0])/2.0;
		    ON_Interval left(t[0],midpoint);
		    ON_Interval right(midpoint,t[1]);
		    getHVTangents(curve, left, list);
		    getHVTangents(curve, right, list);
		    return true;
		}
	    }
	    return true;
	}
	
	
	void CurveTree::GetBAChildren(BRNode* parent, int depth, double min, double max) {
	    double mid = (max + min) / 2.0;
	    ON_Interval tl(min, mid);
	    ON_Interval tr(mid, max);
	    BRNode* left = new BRNode(parent->m_face, parent->m_loop, parent->m_curve, tl, parent->m_innerTrim);
	    BRNode* right = new BRNode(parent->m_face, parent->m_loop, parent->m_curve, tr, parent->m_innerTrim);
	    TrimBBBuild(left);
	    TrimBBBuild(right);
	    if (!(isLinear(parent->m_curve, min, mid))) {
		GetBAChildren(left, depth + 1, min, mid);
	    }
	    if (!(isLinear(parent->m_curve, mid, max))) {
		GetBAChildren(right, depth + 1, mid, max);
	    }
	    parent->addChild(left);
	    parent->addChild(right);
	    // Once children are done, make sure parent bounding box contains
	    // all child bounding boxes.
	    ParentBBBuild(parent);
	}

	void CurveTree::ParentBBBuild(BRNode* parent) {
	    vector<BRNode*>& children = parent->m_children;
	    for (vector<BRNode*>::iterator i = children.begin(); i != children.end(); i++) {
		for (int j = 0; j < 3; j++) {
		    if (parent->m_BBox.m_min[j] > (*i)->m_BBox.m_min[j])
			parent->m_BBox.m_min[j] = (*i)->m_BBox.m_min[j];
		    if (parent->m_BBox.m_max[j] < (*i)->m_BBox.m_max[j])
			parent->m_BBox.m_max[j] = (*i)->m_BBox.m_max[j];
		}
	    }
	}

	void CurveTree::TrimBBBuild(BRNode* node) {
	    double min = node->m_t[0];
	    double max = node->m_t[1];
	    double delta = (max + min)/(BREP_BB_CRV_PNT_CNT-1);
	    point_t points[BREP_BB_CRV_PNT_CNT];
	    ON_3dPoint pnt;
	    for(int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++){
		pnt = node->m_curve->PointAt(min + delta*i);
		VSET(points[i],pnt[0],pnt[1],pnt[2]);
	    }
	    pnt = node->m_curve->PointAt(max);
	    VSET(points[BREP_BB_CRV_PNT_CNT-1],pnt[0],pnt[1],pnt[2]);
	    point_t minpt, maxpt;
	    VSETALL(minpt, MAX_FASTF);
	    VSETALL(maxpt, -MAX_FASTF);
	    for (int i = 0; i < BREP_BB_CRV_PNT_CNT; i++)
		VMINMAX(minpt,maxpt,((double*)points[i]));
	    
	    VMOVE(pnt,minpt);
	    node->m_BBox.Set(pnt,false);
	    VMOVE(pnt,maxpt);
	    node->m_BBox.Set(pnt,true);
	}
	

	//Determine whether a given curve segment is linear

	bool
	CurveTree::isLinear(const ON_Curve* curve, double min, double max)
	{
		double mid = (max + min)/2.0;
		double qtr = (max - min)/4.0;
		ON_3dPoint  pmin = curve->PointAt(min);
		ON_3dPoint  pmax = curve->PointAt(max);

		const ON_Surface* surf = m_face->SurfaceOf();
		ON_Interval u = surf->Domain(0);
		ON_Interval v = surf->Domain(1);
		point_t a,b;
		VSET(a,u[0],v[0],0.0);
		VSET(b,u[1],v[1],0.0);
		double dd = DIST_PT_PT(a,b);
		double cd = DIST_PT_PT(pmin,pmax);

		if (cd > BREP_TRIM_SUB_FACTOR*dd )
			return false;

		double delta = (max - min)/(BREP_BB_CRV_PNT_CNT-1);
		ON_3dPoint points[BREP_BB_CRV_PNT_CNT];
		for(int i=0;i<BREP_BB_CRV_PNT_CNT-1;i++){
			points[i] = curve->PointAt(min + delta*i);
		}
		points[BREP_BB_CRV_PNT_CNT-1] = curve->PointAt(max);

		ON_3dVector A;
		ON_3dVector B;
		double vdot = 1.0;
		A = points[BREP_BB_CRV_PNT_CNT-1] - points[0];
		A.Unitize();
		for(int i=1;i<BREP_BB_CRV_PNT_CNT-1;i++){
			B = points[i] - points[0];
			B.Unitize();
			vdot = vdot * ( A * B );
			if ( vdot < BREP_CURVE_FLATNESS )
				return false; //already failed
		}

		return  (vdot >= BREP_CURVE_FLATNESS);
	}
    //--------------------------------------------------------------------------------
    // SurfaceTree

    SurfaceTree::SurfaceTree(const ON_BrepFace* face, CurveTree* ctree) : m_face(face), m_ctree(ctree) {
	const ON_Surface* surf = face->SurfaceOf();
	ON_Interval d1 = surf->Domain(0);
	ON_Interval d2 = surf->Domain(1);
	m_root = new BBNode(m_face,surf,d1,d2,m_ctree);
	if (!(m_root->isFlat())) {
	    GetBVChildren(m_root,1);
	}
    }

    SurfaceTree::~SurfaceTree() {
        delete m_root;
    }

    BBNode* SurfaceTree::getRootNode() const {
        return m_root;
    }

    int SurfaceTree::depth() {
        return m_root->depth();
    }

    ON_2dPoint SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt) {
        return m_root->getClosestPointEstimate(pt);
    }

    ON_2dPoint SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
        return m_root->getClosestPointEstimate(pt, u, v);
    }

    void SurfaceTree::getLeaves(list<BBNode*>& out_leaves) {
        m_root->getLeaves(out_leaves);
    }
    
    /* When breaking into quadrants, numbering convention is:
     *
     *  ---------------------
     *  |         |         |
     *  |    3    |    4    |
     *  |         |         |
     *  ---------------------
     *  |         |         |
     *  |    1    |    2    |
     *  |         |         |
     *  ---------------------
     *
     */
    void SurfaceTree::GetBVChildren(BBNode* parent, int depth) {
	BBNode* quads[4];
	ON_Interval first(0,0.5);
	ON_Interval second(0.5,1.0);
	ON_Interval ufirst = parent->m_u.ParameterAt(first);
	ON_Interval vfirst = parent->m_v.ParameterAt(first);
	ON_Interval usecond = parent->m_u.ParameterAt(second);
	ON_Interval vsecond = parent->m_v.ParameterAt(second);
	CurveTree* ctree = parent->m_ctree;
	parent->m_surface->EvNormal(parent->m_u.ParameterAt(0.5),parent->m_v.Min(),parent->m_normals[1],0);
	parent->m_surface->EvNormal(parent->m_u.Min(),parent->m_v.ParameterAt(0.5),parent->m_normals[5],0);
	parent->m_surface->EvNormal(parent->m_u.Max(),parent->m_v.ParameterAt(0.5),parent->m_normals[7],0);
	parent->m_surface->EvNormal(parent->m_u.ParameterAt(0.5),parent->m_v.Max(),parent->m_normals[11],0);
	quads[0] = new BBNode(parent->m_face,parent->m_surface,ufirst,vfirst, ctree, parent->m_normals[0], parent->m_normals[1], parent->m_normals[3], parent->m_normals[5], parent->m_normals[6]);
	quads[1] = new BBNode(parent->m_face,parent->m_surface,usecond,vfirst, ctree, parent->m_normals[1], parent->m_normals[2], parent->m_normals[4], parent->m_normals[6], parent->m_normals[7]);
	quads[2] = new BBNode(parent->m_face,parent->m_surface,ufirst,vsecond, ctree, parent->m_normals[5], parent->m_normals[6], parent->m_normals[8], parent->m_normals[10], parent->m_normals[11]);
	quads[3] = new BBNode(parent->m_face,parent->m_surface,usecond,vsecond, ctree, parent->m_normals[6], parent->m_normals[7], parent->m_normals[9], parent->m_normals[11], parent->m_normals[12]);
	
	for (int i = 0; i < 4; i++) {
	    if ((quads[i]->NodeTrimmed() != 1) && !(quads[i]->isFlat())) {
		GetBVChildren(quads[i],depth++);
	    }
	    if (!(quads[i]->m_trimmed)) {
		parent->addChild(quads[i]);
	    }
	}
	BuildBBox(parent);
    }

    void SurfaceTree::BuildBBox(BBNode* currentnode) {
	for (vector<BBNode*>::iterator childnode = currentnode->m_children.begin(); childnode != currentnode->m_children.end(); childnode++) {
	    if (childnode == currentnode->m_children.begin()) {
		currentnode->m_BBox.m_min[0] = (*childnode)->m_BBox.m_min[0];
		currentnode->m_BBox.m_min[1] = (*childnode)->m_BBox.m_min[1];
		currentnode->m_BBox.m_min[2] = (*childnode)->m_BBox.m_min[2];
		currentnode->m_BBox.m_max[0] = (*childnode)->m_BBox.m_max[0];
		currentnode->m_BBox.m_max[1] = (*childnode)->m_BBox.m_max[1];
		currentnode->m_BBox.m_max[2] = (*childnode)->m_BBox.m_max[2];
	    } else {
    		for (int j = 0; j < 3; j++) {
    		    if (currentnode->m_BBox.m_min[j] > (*childnode)->m_BBox.m_min[j]) {
    			currentnode->m_BBox.m_min[j] = (*childnode)->m_BBox.m_min[j];
    		    }
    		    if (currentnode->m_BBox.m_max[j] < (*childnode)->m_BBox.m_max[j]) {
    			currentnode->m_BBox.m_max[j] = (*childnode)->m_BBox.m_max[j];
    		    }
    		}
    	    }
    	}
	    
    }
    
    
    bool sortY (BRNode* first, BRNode* second) { 
    	point_t first_min,second_min;
	point_t first_max,second_max;
	
	first->GetBBox(first_min,first_max);
	second->GetBBox(second_min,second_max);
	
	if ( first_min[Y] < second_min[Y] )
	    return true;
	else 
	    return false;
    }

}




// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
