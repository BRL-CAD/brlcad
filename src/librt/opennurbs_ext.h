/*                 O P E N N U R B S _ E X T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file opennurbs_ext.h
 *
 * Extensions to the openNURBS library
 *
 */

#ifndef __OPENNURBS_EXT
#define __OPENNURBS_EXT

#include "opennurbs.h"
#include <vector>
#include <list>
#include <iostream>
#include "vmath.h"

/* These definitions were added to opennurbs_curve.h - they are
 * extensions of openNURBS, so add them here instead.  At some
 * point a more coherent structure should probably be set up for
 * organization of openNURBS extensions, since there may be more
 * to come, but at least try to keep as many as possible out of
 * the external openNURBS tree - simplifies syncing.
 */
class ON_Ray {
public:
    ON_3dPoint m_origin;
    ON_3dVector m_dir;

    ON_Ray(ON_3dPoint& origin, ON_3dVector& dir) : m_origin(origin), m_dir(dir) {}
    ON_Ray(const ON_Ray& r) : m_origin(r.m_origin), m_dir(r.m_dir) {}
    ON_Ray& operator=(const ON_Ray& r) {
	m_origin = r.m_origin;
	m_dir = r.m_dir;
	return *this;
    }
  
    ON_3dPoint PointAt(double t) const {
	return m_origin + m_dir * t;
    }
  
    double DistanceTo(const ON_3dPoint& pt, double* out_t = NULL) const {
	ON_3dVector w = pt - m_origin;
	double c1 = w * m_dir;
	if (c1 <= 0) return pt.DistanceTo(m_origin);
	double c2 = m_dir * m_dir;
	double b = c1 / c2;
	ON_3dPoint p = m_dir * b + m_origin;
	if (out_t != NULL) *out_t = b;
	return p.DistanceTo(pt);
    }
};


ON_DECL
bool ON_NearZero(double x, double tolerance = ON_ZERO_TOLERANCE);

/* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.85
#define BREP_SURFACE_STRAIGHTNESS 0.75
/* Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 50
/* Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 1e-5
/* trim curve point sampling count for isLinear() check and possibly growing bounding box*/
#define BREP_BB_CRV_PNT_CNT 10
#define BREP_CURVE_FLATNESS 0.95

/* subdivision size factors */
#define BREP_SURF_SUB_FACTOR 1
#define BREP_TRIM_SUB_FACTOR 1
/**
 * The EDGE_MISS_TOLERANCE setting is critical in a couple of ways -
 * too small and the allowed uncertainty region near edges will be
 * smaller than the actual uncertainty needed for accurate solid
 * raytracing, too large and trimming will not be adequate.  May need
 * to adapt this to the scale of the model, perhaps using bounding box
 * size to key off of.
 */
/* #define BREP_EDGE_MISS_TOLERANCE 5e-2 */
#define BREP_EDGE_MISS_TOLERANCE 5e-3
#define BREP_SAME_POINT_TOLERANCE 1e-6

// FIXME: debugging crapola (clean up later)
#define ON_PRINT4(p) "[" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ", " << (p)[3] << "]"
#define ON_PRINT3(p) "(" << (p)[0] << ", " << (p)[1] << ", " << (p)[2] << ")"
#define ON_PRINT2(p) "(" << (p)[0] << ", " << (p)[1] << ")"
#define PT(p) ON_PRINT3(p)
#define PT2(p) ON_PRINT2(p)
#define IVAL(_ival) "[" << (_ival).m_t[0] << ", " << (_ival).m_t[1] << "]"
#define TRACE(s)
#define TRACE1(s)
#define TRACE2(s)
//#define TRACE(s) std::cerr << s << std::endl;
//#define TRACE1(s) std::cerr << s << std::endl;
//#define TRACE2(s) std::cerr << s << std::endl;

namespace brlcad {

/**
 * Bounding area hierarchy classes
 */
template<class BA>
class BANode;


template<class BA>
class BANode {
public:
    BANode();
    BANode(const BA& node);
    BANode(const ON_Curve* curve, int m_adj_face_index, const BA& node,
	   const ON_BrepFace* face, const ON_Interval& t,
	   bool innerTrim = false, bool checkTrim = true, bool trimmed = false);
    ~BANode();

    /** List of all children of a given node */
    typedef std::vector<BANode<BA>*> ChildList;
    ChildList m_children;

    /** Bounding Box */
    BA m_node;

    /** Node management functions */
    void addChild(const BA& child);
    void addChild(BANode<BA>* child);
    void removeChild(const BA& child);
    void removeChild(BANode<BA>* child);

    /** Test if this node is a leaf node (i.e. m_children is empty) */
    bool isLeaf();

    /** Return a list of all nodes below this node that are leaf nodes */
    void getLeaves(std::list<BANode<BA>*>& out_leaves);

    /** Report the depth of this node in the hierarchy */
    int depth();

    /**
     * Get 2 points defining bounding box:
     *
     *       *----------------max
     *       |                 |
     *  v    |                 |
     *       |                 |
     *      min----------------*
     *                 u
     */ 
    void GetBBox(double* min, double* max) const;

    /** Surface Information */
    const ON_BrepFace* m_face;
    ON_Interval m_u;
    ON_Interval m_v;

    /** Trim Curve Information */
    const ON_Curve* m_trim;
    ON_Interval m_t;
    int m_adj_face_index;

    /** Trimming Flags */
    bool m_checkTrim;
    bool m_trimmed;
    bool m_XIncreasing;
    bool m_Horizontal;
    bool m_Vertical;
    bool m_innerTrim;


    ON_3dPoint m_estimate;
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
    fastf_t getLinearEstimateOfV(fastf_t u);
    fastf_t getCurveEstimateOfV(fastf_t u, fastf_t tol) const;
    fastf_t getCurveEstimateOfU(fastf_t v, fastf_t tol) const;

    int isTrimmed(const ON_2dPoint& uv, fastf_t &trimdist) const;
    bool doTrimming() const;
		    		    
private:
    BANode<BA>* closer(const ON_3dPoint& pt, BANode<BA>* left, BANode<BA>* right);
    fastf_t m_slope;
    fastf_t m_vdot;
    fastf_t m_bb_diag;
    ON_3dPoint m_start;
    ON_3dPoint m_end;
};


typedef BANode<ON_BoundingBox> BRNode;


template<class BA>
inline
BANode<BA>::BANode() { }

template<class BA>
inline
__BU_ATTR_ALWAYS_INLINE
BANode<BA>::BANode(const ON_Curve* curve, int adj_face_index, const BA& node,
		   const ON_BrepFace* face, const ON_Interval& t,
		   bool innerTrim, bool checkTrim, bool trimmed)
    : m_node(node), m_face(face), m_trim(curve), m_t(t), m_adj_face_index(adj_face_index),
      m_checkTrim(checkTrim), m_trimmed(trimmed), m_innerTrim(innerTrim)
{
    m_start = curve->PointAt(m_t[0]);
    m_end = curve->PointAt(m_t[1]);
    // check for vertical segments they can be removed
    // from trims above (can't tell direction and don't
    // need
    m_Horizontal = false;
    m_Vertical = false;

    if (NEAR_EQUAL(m_end[X], m_start[X], 0.000001)) {
	m_Vertical = true;
	if (m_innerTrim) {
	    m_XIncreasing = false;
	} else {
	    m_XIncreasing = true;
	}
    } else if (NEAR_EQUAL(m_end[Y], m_start[Y], 0.000001)) {
	m_Horizontal = true;
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
	m_slope = 0.0;
    } else {
	if ((m_end[X] - m_start[X]) > 0.0) {
	    m_XIncreasing = true;
	} else {
	    m_XIncreasing = false;
	}
	m_slope = (m_end[Y] - m_start[Y])/(m_end[X] - m_start[X]);
    }
    m_bb_diag = DIST_PT_PT(m_start, m_end);
}


template<class BA>
inline
__BU_ATTR_ALWAYS_INLINE
BANode<BA>::BANode(const BA& node) : m_node(node)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (NEAR_ZERO(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


template<class BA>
BANode<BA>::~BANode()
{
    // delete the children
    for (size_t i = 0; i < m_children.size(); i++) {
	delete m_children[i];
    }
}


template<class BA>
inline void
BANode<BA>::addChild(const BA& child)
{
    m_children.push_back(new BANode<BA>(child));
}


template<class BA>
inline void
BANode<BA>::addChild(BANode<BA>* child)
{
    if (child) {
	m_children.push_back(child);
    }
}


template<class BA>
inline void
BANode<BA>::removeChild(const BA& child)
{
    // FIXME: implement
}


template<class BA>
inline void
BANode<BA>::removeChild(BANode<BA>* child)
{
    // FIXME: implement
}


template<class BA>
inline bool
BANode<BA>::isLeaf()
{
    if (m_children.size() == 0) return true;
    return false;
}


/*
  template<class BA>
  inline void
  BANode<BA>::GetBBox(double* min, double* max) const
  {
  min[0] = m_node.m_min[0];
  min[1] = m_node.m_min[1];
  min[2] = m_node.m_min[2];
  max[0] = m_node.m_max[0];
  max[1] = m_node.m_max[1];
  max[2] = m_node.m_max[2];
  }
*/

template<class BA>
inline void
__BU_ATTR_ALWAYS_INLINE
BANode<BA>::GetBBox(double* min, double* max) const
{
    VSETALL(min, MAX_FASTF);
    VSETALL(max, -MAX_FASTF);
    VMINMAX(min, max, m_start);
    VMINMAX(min, max, m_end);
}


template<class BA>
int
BANode<BA>::depth()
{
    int d = 0;
    for (size_t i = 0; i < m_children.size(); i++) {
	d = 1 + std::max(d, m_children[i]->depth());
    }
    return d;
}


template<class BA>
void
BANode<BA>::getLeaves(std::list<BANode<BA>*>& out_leaves)
{
    if (m_children.size() > 0) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}


template<class BA>
BANode<BA>*
BANode<BA>::closer(const ON_3dPoint& pt, BANode<BA>* left, BANode<BA>* right)
{
    double ldist = pt.DistanceTo(left->m_estimate);
    double rdist = pt.DistanceTo(right->m_estimate);
    TRACE("\t" << ldist << " < " << rdist);
    if (ldist < rdist) return left;
    else return right;
}


template<class BA>
int
BANode<BA>::isTrimmed(const ON_2dPoint& uv, fastf_t &trimdist) const
{
    point_t bmin, bmax;
    BANode<BA>::GetBBox(bmin, bmax);
    if ((bmin[X] <= uv[X]) && (uv[X] <= bmax[X])) { //if check trim and in BBox
	fastf_t v = getCurveEstimateOfV(uv[X], 0.0000001);
	trimdist = v - uv[Y];
	if (uv[Y] <= v) {
	    if (m_XIncreasing) {
		return 1;
	    } else {
		return 0;
	    }
	} else if (uv[Y] > v) {
	    if (!m_XIncreasing) {
		return 1;
	    } else {
		return 0;
	    }
	} else {
	    return 1;
	}
    } else {
	trimdist = -1.0;
	if (m_trimmed) {
	    return 1;
	} else {
	    return 0;
	}
    }
}


template<class BA>
inline bool
BANode<BA>::doTrimming() const
{
    return m_checkTrim;
}


template<class BA>
ON_2dPoint
BANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt)
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}


template<class BA>
ON_2dPoint
BANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v)
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()},  // include the corners for an easy refinement
			    {m_u.Max(), m_v.Min()},
			    {m_u.Max(), m_v.Max()},
			    {m_u.Min(), m_v.Max()},
			    {m_u.Mid(), m_v.Mid()}}; // include the estimate
	ON_3dPoint corners[5];
	const ON_Surface* surf = m_face->SurfaceOf();
    			
	u = m_u;
	v = m_v;
    			
	// ??? should we pass these in from SurfaceTree::curveBBox() to avoid this recalculation?
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3])) {
	    throw new std::exception(); // FIXME
	}
	corners[4] = BANode<BA>::m_estimate;
    			
	// find the point on the curve closest to pt
	size_t mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	for (size_t i = 1; i < 5; i++) {
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;	    
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
    } else {
	if (m_children.size() > 0) {
	    BRNode* closestNode = m_children[0];
	    for (size_t i = 1; i < m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_children[i]);
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	} else {
	    throw new std::exception();
	}
    }
}


template<class BA>
fastf_t
BANode<BA>::getLinearEstimateOfV(fastf_t u)
{
    fastf_t v = m_start[Y] + m_slope*(u - m_start[X]);
    return v;
}


template<class BA>
fastf_t
BANode<BA>::getCurveEstimateOfV(fastf_t u, fastf_t tol) const
{
    ON_3dVector tangent;
    point_t A, B;
    double Ta, Tb;

    if (m_start[X] < u) {
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol)) {  //vertical
    	return A[Y];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);


    fastf_t du = fabs(Tan_end.x - Tan_start.x);
    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (du > 0.05) {
    	guess = Ta + dT/2;
    	p = m_trim->PointAt(guess);

	if (UNLIKELY(NEAR_EQUAL(p[X], u, SMALL_FASTF))) {
	    /* hit 'u' exactly, done deal */
	    return p[Y];
	}

    	if (p[X] > u) {
	    /* v is behind us, back up the end */
    	    Tb = guess;
    	    VMOVE(B, p);
    	    Tan_end = m_trim->TangentAt(Tb);
    	} else {
	    /* v is in front, move start forward */
    	    Ta = guess;
    	    VMOVE(A, p);
    	    Tan_start = m_trim->TangentAt(Ta);
    	}
    	dT = Tb - Ta;
    	du = fabs(Tan_end.x - Tan_start.x);
    }

    dU = B[X] - A[X];
    if (NEAR_ZERO(dU, tol)) {  //vertical
    	return A[Y];
    }

    guess = Ta + (u - A[X]) * dT/dU;
    p = m_trim->PointAt(guess);

    int cnt=0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[X], u, tol))) {
	if (p[X] < u) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dU = B[X] - A[X];
	if (NEAR_ZERO(dU, tol)) {  //vertical
	    return A[Y];
	}

	dT = Tb - Ta;
	guess = Ta + (u - A[X]) * dT/dU;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'v' given a trim curve and 'u' did not converge within iteration bound(%d).\n",
	       cnt);
    }
    return p[Y];
}


template<class BA>
fastf_t
BANode<BA>::getCurveEstimateOfU(fastf_t v, fastf_t tol) const
{
    ON_3dVector tangent;
    point_t A, B;
    double Ta, Tb;

    if (m_start[Y] < v) {
	VMOVE(A, m_start);
	VMOVE(B, m_end);
	Ta = m_t[0];
	Tb = m_t[1];
    } else {
	VMOVE(A, m_end);
	VMOVE(B, m_start);
	Ta = m_t[1];
	Tb = m_t[0];
    }

    fastf_t dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol)) {  //horizontal
	return A[X];
    }

    ON_3dVector Tan_start = m_trim->TangentAt(Ta);
    ON_3dVector Tan_end = m_trim->TangentAt(Tb);


    fastf_t dv = fabs(Tan_end.y - Tan_start.y);
    fastf_t dT = Tb - Ta;
    fastf_t guess;
    ON_3dPoint p;

    /* Use quick binary subdivision until derivatives at end points in 'u' are within 5 percent */
    while (dv > 0.05) {
    	guess = Ta + dT/2;
    	p = m_trim->PointAt(guess);
    	if (p[Y] < v) {
    	    Ta = guess;
    	    VMOVE(A, p);
    	    Tan_start = m_trim->TangentAt(Ta);
    	} else {
    	    Tb = guess;
    	    VMOVE(B, p);
    	    Tan_end = m_trim->TangentAt(Tb);
    	}
    	dT = Tb - Ta;
    	dv = fabs(Tan_end.y - Tan_start.y);
    }

    dV = B[Y] - A[Y];
    if (NEAR_ZERO(dV, tol)) {  //horizontal
    	return A[X];
    }

    guess = Ta + (v - A[Y]) * dT/dV;
    p = m_trim->PointAt(guess);

    int cnt=0;
    while ((cnt < 1000) && (!NEAR_EQUAL(p[Y], v, tol))) {
	if (p[Y] < v) {
	    Ta = guess;
	    VMOVE(A, p);
	} else {
	    Tb = guess;
	    VMOVE(B, p);
	}
	dV = B[Y] - A[Y];
	if (NEAR_ZERO(dV, tol)) {  //horizontal
	    return A[X];
	}
	dT = Tb - Ta;
	guess = Ta + (v - A[Y]) * dT/dV;
	p = m_trim->PointAt(guess);
	cnt++;
    }
    if (cnt > 999) {
	bu_log("getCurveEstimateOfV(): estimate of 'u' given a trim curve and 'v' did not converge within iteration bound(%d).\n",
	       cnt);
    }
    return p[X];
}


extern bool sortX(BRNode* first, BRNode* second);
extern bool sortY(BRNode* first, BRNode* second);

//--------------------------------------------------------------------------------
// CurveTree declaration
class CurveTree {
public:
    CurveTree(ON_BrepFace* face);
    ~CurveTree();

    BRNode* getRootNode() const;

    /**
     * Calculate, using the surface bounding volume hierarchy, a uv
     * estimate for the closest point on the surface to the point in
     * 3-space.
     */
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);

    /**
     * Return just the leaves of the surface tree
     */
    void getLeaves(std::list<BRNode*>& out_leaves);
    void getLeavesAbove(std::list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v);
    void getLeavesAbove(std::list<BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol);
    void getLeavesRight(std::list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v);
    void getLeavesRight(std::list<BRNode*>& out_leaves, const ON_2dPoint& pt, fastf_t tol);
    int depth();

private:
    fastf_t getVerticalTangent(const ON_Curve *curve, fastf_t min, fastf_t max);
    fastf_t getHorizontalTangent(const ON_Curve *curve, fastf_t min, fastf_t max);
    bool getHVTangents(const ON_Curve* curve, ON_Interval& t, std::list<fastf_t>& list);
    bool isLinear(const ON_Curve* curve, double min, double max);
    BRNode* subdivideCurve(const ON_Curve* curve, int adj_face_index, double min, double max, bool innerTrim, int depth);
    BRNode* curveBBox(const ON_Curve* curve, int adj_face_index, ON_Interval& t, bool leaf, bool innerTrim, const ON_BoundingBox& bb);
    BRNode* initialLoopBBox();

    ON_BrepFace* m_face;
    int m_adj_face_index;
    BRNode* m_root;
    std::list<BRNode*> m_sortedX;
    std::list<BRNode*> m_sortedY;
};


//--------------------------------------------------------------------------------
// Bounding volume hierarchy classes

template<class BV>
class BVNode;


template<class BV>
class BVNode {
public:
    BVNode();
    BVNode(const BV& node);
    BVNode(CurveTree* ct);
    BVNode(CurveTree* ct, const BV& node);
    BVNode(CurveTree* ct, const BV& node, const ON_BrepFace* face, const ON_Interval& u,
	   const ON_Interval& v, bool checkTrim = false, bool trimmed = false);

    ~BVNode();

    // List of all children of a given node
    typedef std::vector<BVNode<BV>*> ChildList;
    ChildList m_children;

    // Curve Tree associated with the parent Surface Tree
    CurveTree* m_ctree;

    // Bounding Box
    BV m_node;

    // Test if this node is a leaf node in the hierarchy
    bool isLeaf();

    // Return all leaves below this node that are leaf nodes
    void getLeaves(std::list<BVNode<BV>*>& out_leaves);
		
    // Functions to add and remove child nodes from this node.
    void addChild(const BV& child);
    void addChild(BVNode<BV>* child);
    void removeChild(const BV& child);
    void removeChild(BVNode<BV>* child);
		
    // Report the depth of this node in the hierarchy
    int depth();
		
    // Get 2 points defining a bounding box
    //
    //                _  max  _
    //        _   -       +      -  _
    //     *  _           +         _  *
    //     |      -   _   + _   -      |
    //     |             *+            |
    //     |             |+            |
    //     |          _  |+   _        |
    //     |  _   -      |       -  _  |
    //     *  _          |          _  *
    //            -   _  |  _   -      
    //                  min
    // 
    void GetBBox(double* min, double* max);

    // Surface Information
    const ON_BrepFace* m_face;
    ON_Interval m_u;
    ON_Interval m_v;

    // Trimming Flags
    bool m_checkTrim;
    bool m_trimmed;
		
    // Point used for closeness testing - usually
    // based on evaluation of the curve/surface at
    // the center of the parametric domain
    ON_3dPoint m_estimate;

    // Normal at the m_estiamte point
    ON_3dVector m_normal;

    // Test whether a ray intersects the 3D bounding volume
    // of the node - if so, and node is not a leaf node, query
    // children.  If leaf node, and intersects, add to list.
    bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    bool intersectsHierarchy(ON_Ray& ray, std::list<BVNode<ON_BoundingBox>*>& results);

    // Report if a given uv point is within the uv boundardies
    // defined by a node.
    bool containsUV(const ON_2dPoint& uv);


    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
    int getLeavesBoundingPoint(const ON_3dPoint& pt, std::list<BVNode<BV> *>& out);
    int isTrimmed(const ON_2dPoint& uv, BRNode* closest, fastf_t &closesttrim);
    bool doTrimming() const;
		
    void getTrimsAbove(const ON_2dPoint& uv, std::list<BRNode*>& out_leaves);
    void BuildBBox();
    bool prepTrims();

private:
    BVNode<BV>* closer(const ON_3dPoint& pt, BVNode<BV>* left, BVNode<BV>* right);
    std::list<BRNode*> m_trims_above;
    std::list<BRNode*> m_trims_vertical;
//		std::list<BRNode*> m_trims_right;
};


typedef BVNode<ON_BoundingBox> BBNode;

//--------------------------------------------------------------------------------
// Template Implementation
template<class BV>
inline
BVNode<BV>::BVNode()
{
    m_face = NULL;
}


template<class BV>
inline
BVNode<BV>::BVNode(const BV& node) : m_node(node)
{
    m_face = NULL;
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


template<class BV>
inline
BVNode<BV>::BVNode(CurveTree* ct): m_ctree(ct)
{
    m_face = NULL;
}


template<class BV>
inline
BVNode<BV>::BVNode(CurveTree* ct, const BV& node) : m_ctree(ct), m_node(node)
{
    m_face = NULL;
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


template<class BV>
inline
__BU_ATTR_ALWAYS_INLINE
BVNode<BV>::BVNode(CurveTree* ct, const BV& node, const ON_BrepFace* face,
		   const ON_Interval& u, const ON_Interval& v, bool checkTrim, bool trimmed)
    : m_ctree(ct), m_node(node), m_face(face), m_u(u), m_v(v), m_checkTrim(checkTrim), 
      m_trimmed(trimmed)
{
    for (int i = 0; i < 3; i++) {
	double d = m_node.m_max[i] - m_node.m_min[i];
	if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
	    m_node.m_min[i] -= 0.001;
	    m_node.m_max[i] += 0.001;
	}
    }
}


template<class BV>
BVNode<BV>::~BVNode() {
    // delete the children
    for (size_t i = 0; i < m_children.size(); i++) {
	delete m_children[i];
    }
}


template<class BV>
inline void
BVNode<BV>::addChild(const BV& child)
{
    m_children.push_back(new BVNode<BV>(child));
}


template<class BV>
inline void
BVNode<BV>::addChild(BVNode<BV>* child)
{
    if (child) {
	m_children.push_back(child);
    }
}


template<class BV>
inline void
BVNode<BV>::removeChild(const BV& child)
{
    // FIXME: implement
}


template<class BV>
inline void
BVNode<BV>::removeChild(BVNode<BV>* child)
{
    // FIXME: implement
}


template<class BV>
inline bool
BVNode<BV>::isLeaf()
{
    if (m_children.size() == 0) return true;
    return false;
}


template<class BV>
inline void
BVNode<BV>::GetBBox(double* min, double* max)
{
    min[0] = m_node.m_min[0];
    min[1] = m_node.m_min[1];
    min[2] = m_node.m_min[2];
    max[0] = m_node.m_max[0];
    max[1] = m_node.m_max[1];
    max[2] = m_node.m_max[2];
}


template<class BV>
inline bool
BVNode<BV>::intersectedBy(ON_Ray& ray, double* tnear_opt, double* tfar_opt)
{
    double tnear = -DBL_MAX;
    double tfar = DBL_MAX;
    bool untrimmedresult = true;
    for (int i = 0; i < 3; i++) {
	if (ON_NearZero(ray.m_dir[i])) {
	    if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
		untrimmedresult = false;
	    }
	} else {
	    double t1 = (m_node.m_min[i]-ray.m_origin[i]) / ray.m_dir[i];
	    double t2 = (m_node.m_max[i]-ray.m_origin[i]) / ray.m_dir[i];
	    if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; } // swap
	    if (t1 > tnear) tnear = t1;
	    if (t2 < tfar) tfar = t2;
	    if (tnear > tfar) /* box is missed */ untrimmedresult = false;
	    if (tfar < 0) /* box is behind ray */ untrimmedresult = false;
	}
    }
    if (tnear_opt != NULL && tfar_opt != NULL) { 
	*tnear_opt = tnear; *tfar_opt = tfar; 
    }
    if (isLeaf()) {
	return !m_trimmed && untrimmedresult;
    } else {
	return untrimmedresult;
    }
}


template<class BV>
bool
BVNode<BV>::intersectsHierarchy(ON_Ray& ray, std::list<BBNode*>& results_opt)
{
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
	results_opt.push_back(this);
    } else if (intersects) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->intersectsHierarchy(ray, results_opt);
	}
    }
    return intersects; 
}


template<class BV>
bool
BVNode<BV>::containsUV(const ON_2dPoint& uv)
{
    if ((uv[0] > m_u[0]) && (uv[0] < m_u[1]) && (uv[1] > m_v[0]) && (uv[1] < m_v[1])) {
	return true;
    } else {
	return false;
    }
}


template<class BV>
int
BVNode<BV>::depth()
{
    int d = 0;
    for (size_t i = 0; i < m_children.size(); i++) {
	d = 1 + std::max(d, m_children[i]->depth());
    }
    return d;
}


template<class BV>
void
BVNode<BV>::getLeaves(std::list<BVNode<BV>*>& out_leaves)
{
    if (m_children.size() > 0) {
	for (size_t i = 0; i < m_children.size(); i++) {
	    m_children[i]->getLeaves(out_leaves);
	}
    } else {
	out_leaves.push_back(this);
    }
}


template<class BV>
BVNode<BV>*
BVNode<BV>::closer(const ON_3dPoint& pt, BVNode<BV>* left, BVNode<BV>* right)
{
    double ldist = pt.DistanceTo(left->m_estimate);
    double rdist = pt.DistanceTo(right->m_estimate);
    TRACE("\t" << ldist << " < " << rdist);
    if (ldist < rdist) return left;
    else return right;
}


template<class BV>
ON_2dPoint
BVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt)
{
    ON_Interval u, v;
    return getClosestPointEstimate(pt, u, v);
}


template<class BV>
ON_2dPoint
BVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v)
{
    if (isLeaf()) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()}, {m_u.Max(), m_v.Min()}, 
			    {m_u.Max(), m_v.Max()}, {m_u.Min(), m_v.Max()},
			    {m_u.Mid(), m_v.Mid()}}; // include the estimate
	ON_3dPoint corners[5];
	const ON_Surface* surf = m_face->SurfaceOf();
		
	u = m_u;
	v = m_v;
		
	// ??? pass these in from SurfaceTree::surfaceBBox() to avoid this recalculation?
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3])) {
	    throw new std::exception(); // FIXME
	}
	corners[4] = BVNode<BV>::m_estimate;
		
	// find the point on the surface closest to pt
	size_t mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	for (size_t i = 1; i < 5; i++) {
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
		
    } else {
	if (m_children.size() > 0) {
	    BBNode* closestNode = m_children[0];
	    for (size_t i = 1; i < m_children.size(); i++) {
		closestNode = closer(pt, closestNode, m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw new std::exception();
    }
}


template<class BV>
int
BVNode<BV>::getLeavesBoundingPoint(const ON_3dPoint& pt, std::list<BVNode<BV> *>& out)
{
    if (isLeaf()) {
	double min[3], max[3];
	GetBBox(min, max);
	if ((pt.x >= (min[0])) && (pt.x <= (max[0])) &&
	    (pt.y >= (min[1])) && (pt.y <= (max[1])) &&
	    (pt.z >= (min[2])) && (pt.z <= (max[2]))) {
	    // falls within BBox so put in list
	    out.push_back(this);
	    return 1;
	}
	return 0;

    } else {
	int sum = 0;
	for (size_t i = 0; i < m_children.size(); i++) {
	    sum+=m_children[i]->getLeavesBoundingPoint(pt, out);
	}
	return sum;
    }
}


template<class BV>
int
BVNode<BV>::isTrimmed(const ON_2dPoint& uv, BRNode* closest, fastf_t &closesttrim)
{
    BRNode* br;
    std::list<BRNode*> trims;
	    
    closesttrim = -1.0;
    if (m_checkTrim) {
	getTrimsAbove(uv, trims);
	if (trims.empty()) {
	    return 1;
	} else {//find closest BB
	    std::list<BRNode*>::iterator i;
	    BRNode* vclosest = NULL;
	    BRNode* uclosest = NULL;
	    fastf_t currHeight = (fastf_t)0.0;
	    bool currTrimStatus = false;
	    bool verticalTrim = false;
	    bool underTrim = false;
	    double vdist = 0.0;
	    double udist = 0.0;

	    for (i=trims.begin();i!=trims.end();i++) {
		br = dynamic_cast<BRNode*>(*i);

		/* skip if trim below */
		if (br->m_node.m_max[1] < uv[Y])
		    continue;

		if (br->m_Vertical) {
		    if ((br->m_v[0] <= uv[Y]) && (br->m_v[1] >= uv[Y])) {
			double dist = fabs(uv[X] - br->m_v[0]);
			if (!verticalTrim) { //haven't seen vertical trim yet
			    verticalTrim = true;
			    vdist = dist;
			    vclosest = br;
			} else {
			    if (dist < vdist) {
				vdist = dist;
				vclosest = br;
			    }
			}
				
		    }
		    continue;
		}
		fastf_t v;
		int trimstatus = br->isTrimmed(uv, v);
		if (v >= 0.0) {
		    if (closest == NULL) {
			currHeight = v;
			currTrimStatus = trimstatus;
			closest = br;
		    } else if (v < currHeight) {
			currHeight = v;
			currTrimStatus = trimstatus;
			closest = br;
		    }
		} else {
		    double dist = fabs(v);
		    if (!underTrim) {
			underTrim = true;
			udist = dist;
			uclosest = br;
		    } else {
			if (dist < udist)
			    udist = dist;
			uclosest = br;
		    }
		}
	    }
	    if (closest == NULL) {
		if (verticalTrim) {
		    closesttrim = vdist;
		    closest = vclosest;
		}
		if ((underTrim) && (!verticalTrim || (udist < closesttrim))) {
		    closesttrim = udist;
		    closest = uclosest;
		}
		return 1;
	    } else {
		closesttrim = currHeight;
		if ((verticalTrim) && (vdist < closesttrim)) {
		    closesttrim = vdist;
		    closest = vclosest;
		}
		if ((underTrim) && (udist < closesttrim)) {
		    closesttrim = udist;
		    closest = uclosest;
		}
		return currTrimStatus;
	    }
	}
    } else {
	if (m_trimmed) {
	    return 1;
	} else {
	    return 0;
	}
    }
}


template<class BV>
inline bool
BVNode<BV>::doTrimming() const
{
    return m_checkTrim;
}


template<class BV>
void
BVNode<BV>::getTrimsAbove(const ON_2dPoint& uv, std::list<BRNode*>& out_leaves)
{
    point_t bmin, bmax;
    double dist;
    for (std::list<BRNode*>::iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++) {
	BRNode* br = dynamic_cast<BRNode*>(*i);
	br->GetBBox(bmin, bmax);
	dist = 0.000001; //0.03*DIST_PT_PT(bmin, bmax);
	if ((uv[X] > bmin[X]-dist) && (uv[X] < bmax[X]+dist))
	    out_leaves.push_back(br);
    }	    
}


template<class BV>
void BVNode<BV>::BuildBBox()
{
    if (m_children.size() > 0) {
	for (std::vector<BBNode*>::iterator childnode = m_children.begin(); childnode != m_children.end(); childnode++) {

	    if (!(*childnode)->isLeaf())
		(*childnode)->BuildBBox();
	    if (childnode == m_children.begin()) {
		m_node = ON_BoundingBox((*childnode)->m_node.m_min, (*childnode)->m_node.m_max);
	    } else {
		for (int j = 0; j < 3; j++) {
		    if (m_node.m_min[j] > (*childnode)->m_node.m_min[j]) {
			m_node.m_min[j] = (*childnode)->m_node.m_min[j];
		    }
		    if (m_node.m_max[j] < (*childnode)->m_node.m_max[j]) {
			m_node.m_max[j] = (*childnode)->m_node.m_max[j];
		    }
		}
	    }
	}
    }
}


template<class BV>
bool
BVNode<BV>::prepTrims()
{
    CurveTree* ct = m_ctree;
    std::list<BRNode*>::iterator i;
    BRNode* br;
    //	point_t surfmin, surfmax;
    point_t curvemin, curvemax;
    double dist = 0.000001;
    bool trim_already_assigned = false;

    m_trims_above.clear();

    if (ct != NULL)
    	ct->getLeavesAbove(m_trims_above, m_u, m_v);

    m_trims_above.sort(sortY);


    if (!m_trims_above.empty()) {
	i = m_trims_above.begin();
	while (i != m_trims_above.end()) {
	    br = dynamic_cast<BRNode*>(*i);
	    if (br->m_Vertical) { // check V to see if trim possibly overlaps
		br->GetBBox(curvemin, curvemax);
		if (curvemin[Y]-dist <= m_v[1]) { //possibly contains trim can't rule out check closer
		    m_checkTrim = true;
		    trim_already_assigned = true;
		    i++;
		} else {
		    i = m_trims_above.erase(i);
		}
		//i = m_trims_above.erase(i);
		//i++;
	    } else {
		i++;
	    }
	}
    }

    if (!trim_already_assigned) { // already contains possible vertical trim
	if (m_trims_above.empty() /*|| m_trims_right.empty()*/) {
	    m_trimmed = true;
	    m_checkTrim = false;
	} else if (!m_trims_above.empty()) {//trimmed above check contains
	    i = m_trims_above.begin();
	    br = dynamic_cast<BRNode*>(*i);
	    br->GetBBox(curvemin, curvemax);
	    dist = 0.000001; //0.03*DIST_PT_PT(curvemin, curvemax);
	    if (curvemin[Y]-dist > m_v[1]) {
		i++;

		if (i == m_trims_above.end()) { //easy only trim in above list
		    if (br->m_XIncreasing) {
			m_trimmed=true;
			m_checkTrim=false;
		    } else {
			m_trimmed=false;
			m_checkTrim=false;
		    }
		} else { //check for trim bbox overlap TODO: look for multiple overlaps
		    BRNode* bs;
		    bs = dynamic_cast<BRNode*>(*i);
		    point_t smin, smax;
		    bs->GetBBox(smin, smax);
		    if ((smin[Y] >= curvemax[Y]) || (smin[X] >= curvemax[X]) || (smax[X] <= curvemin[X])) { //can determine inside/outside without closer inspection
			if (br->m_XIncreasing) {
			    m_trimmed=true;
			    m_checkTrim=false;
			} else {
			    m_trimmed=false;
			    m_checkTrim=false;
			}
		    } else {
			m_checkTrim=true;
		    }
		}
	    } else {
		//m_contains_trim = true; //will have to check for trim at shotline
		m_checkTrim = true;
	    }
	} else {// something wrong here
	    bu_log("Error prepping trims");
	    return false;
	}
    }
    return true;
}


//--------------------------------------------------------------------------------
// SurfaceTree declaration
class SurfaceTree {
private:
    bool m_removeTrimmed;

public:
    SurfaceTree(ON_BrepFace* face, bool removeTrimmed=true, int depthLimit = BREP_MAX_FT_DEPTH);
    ~SurfaceTree();

    CurveTree* ctree;

    BBNode* getRootNode() const;

    /**
     * Calculate, using the surface bounding volume hierarchy, a uv
     * estimate for the closest point on the surface to the point in
     * 3-space.
     */
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);

    /**
     * Return surface
     */
    const ON_Surface *getSurface();
    int getSurfacePoint(const ON_3dPoint& pt, ON_2dPoint& uv, const ON_3dPoint& from, double tolerance = BREP_SAME_POINT_TOLERANCE) const;

    /**
     * Return just the leaves of the surface tree
     */
    void getLeaves(std::list<BBNode*>& out_leaves);
    int depth();

private:
    bool isFlat(const ON_Surface* surf, ON_Plane frames[], ON_3dVector normals[], ON_3dPoint corners[], const ON_Interval& u, const ON_Interval& v);
    bool isStraight(const ON_Surface* surf, ON_Plane frames[], ON_3dVector normals[], ON_3dPoint corners[], const ON_Interval& u, const ON_Interval& v);
    fastf_t isFlatU(const ON_Surface* surf, ON_Plane frames[], ON_3dVector normals[], ON_3dPoint corners[], const ON_Interval& u, const ON_Interval& v);
    fastf_t isFlatV(const ON_Surface* surf, ON_Plane frames[], ON_3dVector normals[], ON_3dPoint corners[], const ON_Interval& u, const ON_Interval& v);
    BBNode* subdivideSurfaceByKnots(const ON_Surface *localsurf, const ON_Interval& u, const ON_Interval& v, ON_Plane frames[], ON_3dPoint corners[], ON_3dVector normals[], int depth, int depthLimit);
    BBNode* subdivideSurface(const ON_Surface *localsurf, const ON_Interval& u, const ON_Interval& v, ON_Plane frames[], ON_3dPoint corners[], ON_3dVector normals[], int depth, int depthLimit);
    BBNode* surfaceBBox(const ON_Surface *localsurf, bool leaf, ON_3dPoint corners[], ON_3dVector normals[], const ON_Interval& u, const ON_Interval& v);

    ON_BrepFace* m_face;
    BBNode* m_root;
};


/**
 * g e t _ c l o s e s t _ p o i n t
 *
 * approach:
 *
 * - get an estimate using the surface tree (if non-null, create
 * one otherwise)
 *
 * - find a point (u, v) for which S(u, v) is closest to _point_
 *                                                     _      __
 *   -- minimize the distance function: D(u, v) = sqrt(|S(u, v)-pt|^2)
 *                                       _      __
 *   -- simplify by minimizing f(u, v) = |S(u, v)-pt|^2
 *
 *   -- minimum occurs when the gradient is zero, i.e.
 *     \f[ \nabla f(u, v) = |\vec{S}(u, v)-\vec{p}|^2 = 0 \f]
 */
bool get_closest_point(ON_2dPoint& outpt,
		       ON_BrepFace* face,
		       const ON_3dPoint& point,
		       SurfaceTree* tree = NULL,
		       double tolerance = BREP_FCP_ROOT_EPSILON);


/**
 * p u l l b a c k _ c u r v e
 *
 * Pull an arbitrary model-space *curve* onto the given *surface* as a
 * curve within the surface's domain when, for each point c = C(t) on
 * the curve and the closest point s = S(u, v) on the surface, we have:
 * distance(c, s) <= *tolerance*.
 *
 * The resulting 2-dimensional curve will be approximated using the
 * following process:
 *
 * 1. Adaptively sample the 3d curve in the domain of the surface
 * (ensure tolerance constraint). Sampling terminates when the
 * following flatness criterion is met:
 * given two parameters on the curve t1 and t2 (which map to points p1 and p2 on the curve)
 * let m be a parameter randomly chosen near the middle of the interval [t1, t2]
 *                                                              ____
 * then the curve between t1 and t2 is flat if distance(C(m), p1p2) < flatness
 *
 * 2. Use the sampled points to perform a global interpolation using
 *    universal knot generation to build a B-Spline curve.
 *
 * 3. If the curve is a line or an arc (determined with openNURBS routines),
 *    return the appropriate ON_Curve subclass (otherwise, return an ON_NurbsCurve).
 */
extern ON_Curve* pullback_curve(ON_BrepFace* face,
				const ON_Curve* curve,
				SurfaceTree* tree = NULL,
				double tolerance = BREP_FCP_ROOT_EPSILON,
				double flatness = 1.0e-3);


}


#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
