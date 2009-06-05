/*                 O P E N N U R B S _ E X T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2009 United States Government as represented by
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
 * brep/surface utilities
 *
 * XXX: these should probably be migrated to openNURBS package proper
 * but there are a lot of dependency issues (e.g. where do the math
 * routines go?)
 */

#ifndef __OPENNURBS_EXT
#define __OPENNURBS_EXT

#include "opennurbs.h"
#include <vector>
#include <list>
#include <limits>
#include <iostream>
#include "vmath.h"

/* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20
#define SIGN(x) ((x) >= 0 ? 1 : -1)
/* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.9
/* Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 50
/* Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 1e-5
/* trim curve point sampling count for isLinear() check and possibly growing bounding box*/
#define BREP_BB_CRV_PNT_CNT 10
#define BREP_CURVE_FLATNESS 0.95

/* subdivision size factors */
#define BREP_SURF_SUB_FACTOR 0.1
#define BREP_TRIM_SUB_FACTOR 0.01

static std::numeric_limits<double> real;

// XXX debugging crapola (clean up later)
#define ON_PRINT4(p) "[" << (p)[0] << "," << (p)[1] << "," << (p)[2] << "," << (p)[3] << "]"
#define ON_PRINT3(p) "(" << (p)[0] << "," << (p)[1] << "," << (p)[2] << ")"
#define ON_PRINT2(p) "(" << (p)[0] << "," << (p)[1] << ")"
#define PT(p) ON_PRINT3(p)
#define PT2(p) ON_PRINT2(p)
#define IVAL(_ival) "[" << (_ival).m_t[0] << "," << (_ival).m_t[1] << "]"
#define TRACE(s)
#define TRACE1(s)
#define TRACE2(s)
//#define TRACE(s) std::cerr << s << std::endl;
//#define TRACE1(s) std::cerr << s << std::endl;
//#define TRACE2(s) std::cerr << s << std::endl;

namespace brlcad {

	//--------------------------------------------------------------------------------
	// Bounding area hierarchy classes

	template<class BA>
	class BANode;

	template<class BA>
	class BASegment {
	public:
	BANode<BA>* m_node;
	double m_near;
	double m_far;

	BASegment() {}
	BASegment(BANode<BA>* node, double near, double far) :
		m_node(node), m_near(near), m_far(far) {}
	BASegment(const BASegment& seg) : m_node(seg.m_node),
		m_near(seg.m_near),
		m_far(seg.m_far) {}
	BASegment& operator=(const BASegment& seg) {
		m_node = seg.m_node;
		m_near = seg.m_near;
		m_far = seg.m_far;
		return *this;
	}

	bool operator<(const BASegment<BA>& seg) {
		return m_near < seg.m_near;
	}
	};

	using namespace std;

	template<class BA>
	class BANode {
	public:
	typedef vector<BANode<BA>*> ChildList;
	typedef list<BASegment<BA> > IsectList;

	ChildList m_children;
	BA m_node;
	ON_3dPoint m_estimate;

	BANode();
	BANode(const BA& node);
	virtual ~BANode();

	void addChild(const BA& child);
	void addChild(BANode<BA>* child);
	void removeChild(const BA& child);
	void removeChild(BANode<BA>* child);
	virtual bool isLeaf() const;

	virtual int depth();
	virtual void getLeaves(list<BANode<BA>*>& out_leaves);
	virtual ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
	virtual ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
	void GetBBox(double* min, double* max);

	virtual bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
	virtual bool intersectsHierarchy(ON_Ray& ray, std::list<BASegment<BA> >* results = 0);

	private:
	BANode<BA>* closer(const ON_3dPoint& pt, BANode<BA>* left, BANode<BA>* right);
	};

	template<class BA>
	class SubcurveBANode : public BANode<BA> {
	public:
		SubcurveBANode(const ON_Curve* curve, const BA& node,
					   const ON_BrepFace* face,
					   const ON_Interval& t,
					   fastf_t vdot,
					   bool innerTrim = false,
					   bool checkTrim = true,
					   bool trimmed = false);
		
		bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
		bool isLeaf() const;
		bool isTrimmed(const ON_2dPoint& uv) const;
		bool doTrimming() const;
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
		ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
		fastf_t getLinearEstimateOfV( fastf_t u );
		fastf_t getCurveEstimateOfV( const ON_2dPoint& uv, fastf_t tol ) const;
		const ON_BrepFace* m_face;
		const ON_Curve* m_trim;
		/* curve domain */
		ON_Interval m_t;
		/* surface domain */
		ON_Interval m_u;
		ON_Interval m_v;
		bool m_checkTrim;
		bool m_trimmed;
		bool m_XIncreasing;
        bool m_innerTrim;
	private:
		fastf_t m_slope;
		fastf_t m_vdot;
		fastf_t m_bb_diag;
		ON_3dPoint m_start;
		ON_3dPoint m_end;
	};

	typedef BANode<ON_BoundingBox> BRNode;
	typedef SubcurveBANode<ON_BoundingBox> SubcurveBRNode;

	//--------------------------------------------------------------------------------
	// Template Implementation
	template<class BA>
	inline
	BANode<BA>::BANode() { }

	template<class BA>
	inline
	BANode<BA>::BANode(const BA& node) : m_node(node) {
	#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < 3; i++) {
	#else
	for (size_t i = 0; i < 3; i++) {
	#endif
		double d = m_node.m_max[i] - m_node.m_min[i];
		if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		m_node.m_min[i] -= 0.001;
		m_node.m_max[i] += 0.001;
		}
	}
	}

	template<class BA>
	inline
	BANode<BA>::~BANode() {
	// delete the children
	#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < m_children.size(); i++) {
	#else
	for (size_t i = 0; i < m_children.size(); i++) {
	#endif
		delete m_children[i];
	}
	}

	template<class BA>
	inline void
	BANode<BA>::addChild(const BA& child) {
	m_children.push_back(new BANode<BA>(child));
	}

	template<class BA>
	inline void
	BANode<BA>::addChild(BANode<BA>* child) {
	m_children.push_back(child);
	}

	template<class BA>
	inline void
	BANode<BA>::removeChild(const BA& child) {
	// XXX implement
	}

	template<class BA>
	inline void
	BANode<BA>::removeChild(BANode<BA>* child) {
	// XXX implement
	}

	template<class BA>
	inline bool
	BANode<BA>::isLeaf() const { return false; }

	template<class BA>
	inline void
	BANode<BA>::GetBBox(double* min, double* max) {
	min[0] = m_node.m_min[0];
	min[1] = m_node.m_min[1];
	min[2] = m_node.m_min[2];
	max[0] = m_node.m_max[0];
	max[1] = m_node.m_max[1];
	max[2] = m_node.m_max[2];
	}


	template<class BA>
	inline bool
	BANode<BA>::intersectedBy(ON_Ray& ray, double* tnear_opt, double* tfar_opt) {
	double tnear = -real.infinity();
	double tfar = real.infinity();
	#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < 3; i++) {
	#else
	for (size_t i = 0; i < 3; i++) {
	#endif

		//cout << "m_node.m_min[" << i << "]: " << m_node.m_min[i] << "\n";
		//cout << "m_node.m_max[" << i << "]: " << m_node.m_max[i] << "\n";
		if (ON_NearZero(ray.m_dir[i])) {
		if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
	//		    cout << "ray.m_origin[" << i << "]: " << ray.m_origin[i] << "\n";
	//		    cout << "m_node.m_min[" << i << "]: " << m_node.m_min[i] << "\n";
	//		    cout << "m_node.m_max[" << i << "]: " << m_node.m_max[i] << "\n\n";
			return false;
		}
		}
		else {
		double t1 = (m_node.m_min[i]-ray.m_origin[i]) / ray.m_dir[i];
		double t2 = (m_node.m_max[i]-ray.m_origin[i]) / ray.m_dir[i];
		if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; } // swap
		if (t1 > tnear) tnear = t1;
		if (t2 < tfar) tfar = t2;
		if (tnear > tfar) /* box is missed */ return false;
		if (tfar < 0) /* box is behind ray */ return false;
		}
	}
	if (tnear_opt != 0 && tfar_opt != 0) { *tnear_opt = tnear; *tfar_opt = tfar; }
	return true;
	}

	template<class BA>
	bool
	BANode<BA>::intersectsHierarchy(ON_Ray& ray, std::list<BASegment<BA> >* results_opt) {
	double tnear, tfar;
	bool intersects = intersectedBy(ray, &tnear, &tfar);
	if (intersects && isLeaf()) {
		if (results_opt != 0) results_opt->push_back(BASegment<BA>(this, tnear, tfar));
	} else if (intersects) {
	// XXX: bug in g++? had to typedef the below to get it to work!
	//       for (std::list<BANode<BA>*>::iterator j = m_children.begin(); j != m_children.end(); j++) {
	// 	(*j)->intersectsHierarchy(ray, results_opt);
	//       }
	#if defined(_WIN32) && !defined(__CYGWIN__)
		for (int i = 0; i < m_children.size(); i++) {
	#else
		for (size_t i = 0; i < m_children.size(); i++) {
	#endif
		m_children[i]->intersectsHierarchy(ray, results_opt);
		}
	}

	return intersects;
	}

	template<class BA>
	int
	BANode<BA>::depth() {
	int d = 0;
	#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < m_children.size(); i++) {
	#else
	for (size_t i = 0; i < m_children.size(); i++) {
	#endif
		d = 1 + max(d, m_children[i]->depth());
	}
	return d;
	}

	template<class BA>
	void
	BANode<BA>::getLeaves(list<BANode<BA>*>& out_leaves) {
	if (m_children.size() > 0) {
	#if defined(_WIN32) && !defined(__CYGWIN__)
		for (int i = 0; i < m_children.size(); i++) {
	#else
		for (size_t i = 0; i < m_children.size(); i++) {
	#endif
		m_children[i]->getLeaves(out_leaves);
		}
	} else {
		out_leaves.push_back(this);
	}
	}


	template<class BA>
	BANode<BA>*
	BANode<BA>::closer(const ON_3dPoint& pt, BANode<BA>* left, BANode<BA>* right) {
	double ldist = pt.DistanceTo(left->m_estimate);
	double rdist = pt.DistanceTo(right->m_estimate);
	TRACE("\t" << ldist << " < " << rdist);
	if (ldist < rdist) return left;
	else return right;
	}

	template<class BA>
	ON_2dPoint
	BANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt) {
	ON_Interval u, v;
	return getClosestPointEstimate(pt, u, v);
	}

	template<class BA>
	ON_2dPoint
	BANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	TRACE("getClosestPointEstimate(" << PT(pt) << ")");
	if (m_children.size() > 0) {
		BRNode* closestNode = m_children[0];
	#if defined(_WIN32) && !defined(__CYGWIN__)
		for (int i = 1; i < m_children.size(); i++) {
	#else
		for (size_t i = 1; i < m_children.size(); i++) {
	#endif
		closestNode = closer(pt, closestNode, m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
		}
		return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw new exception();
	}

	template<class BA>
	inline SubcurveBANode<BA>::SubcurveBANode(const ON_Curve* curve, const BA& node,
						  const ON_BrepFace* face,
						  const ON_Interval& t,
						  fastf_t vdot,
						  bool innerTrim,
						  bool checkTrim,
						  bool trimmed)
	    : m_trim(curve),BANode<BA>(node), m_face(face), m_t(t), m_innerTrim(innerTrim), m_checkTrim(checkTrim), m_trimmed(trimmed) { 
			m_start = curve->PointAt(m_t[0]);
			m_end = curve->PointAt(m_t[1]);
			if ( (m_end[X] - m_start[X]) > 0.0 ) {
				m_XIncreasing = true;
			} else {
				m_XIncreasing = false;
			}
			m_slope = (m_end[Y] - m_start[Y])/(m_end[X] - m_start[X]);
			m_bb_diag = DIST_PT_PT(m_start,m_end);
			
		}

	template<class BA>
	inline bool
	SubcurveBANode<BA>::intersectedBy(ON_Ray& ray, double *tnear, double *tfar) {
	return !m_trimmed && BANode<BA>::intersectedBy(ray, tnear, tfar);
	}

	template<class BA>
	inline bool
	SubcurveBANode<BA>::isLeaf() const {
	return true;
	}

	template<class BA>
	inline bool
	SubcurveBANode<BA>::isTrimmed(const ON_2dPoint& uv) const {
	    if (m_checkTrim) {
			fastf_t v = m_start[Y] + m_slope*(uv[X] - m_start[X]);
			if (fabs(uv[Y] - v) < fabs(m_start[Y]-m_end[Y])*0.001) v = getCurveEstimateOfV(uv,0.0001);
			if (uv[Y] < v) {
				if (m_XIncreasing) {
					return true;
				} else {
					return false;
				}
			} else if (uv[Y] > v) {
				if (!m_XIncreasing) {
					return true;
				} else {
					return false;
				}
			} else {
				return true;
				}
		} else {
			return m_trimmed;
		}
	}

	template<class BA>
	inline bool
	SubcurveBANode<BA>::doTrimming() const {
	return m_checkTrim;
	}

	template<class BA>
	ON_2dPoint
	SubcurveBANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt) {
	ON_Interval u, v;
	return getClosestPointEstimate(pt, u, v);
	}

	template<class BA>
	ON_2dPoint
	SubcurveBANode<BA>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()},  // include the corners for an easy refinement
				{m_u.Max(), m_v.Min()},
				{m_u.Max(), m_v.Max()},
				{m_u.Min(), m_v.Max()},
				{m_u.Mid(), m_v.Mid()}}; // include the estimate
	ON_3dPoint corners[5];
	const ON_Surface* surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	// XXX - pass these in from SurfaceTree::curveBBox() to avoid this recalculation?
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
		!surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
		!surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
		!surf->EvPoint(uvs[3][0], uvs[3][1], corners[3])) {
		throw new exception(); // XXX - fix this
	}
	corners[4] = BANode<BA>::m_estimate;

	// find the point on the curve closest to pt
	int mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
	#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 1; i < 5; i++) {
	#else
	for (size_t i = 1; i < 5; i++) {
	#endif
		tmpdist = pt.DistanceTo(corners[i]);
		TRACE("\t" << mindist << " < " << tmpdist);
		if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
		}
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
	}
	
	template<class BA>
	fastf_t 
	SubcurveBANode<BA>::getLinearEstimateOfV( fastf_t u ) {
		fastf_t v = m_start[Y] + m_slope*(u - m_start[X]);
		return v;
	}

	template<class BA>
	fastf_t
	SubcurveBANode<BA>::getCurveEstimateOfV(const ON_2dPoint& uv, fastf_t tol ) const {
	    double guess = m_t[0] + (m_t[1] - m_t[0])/2;
	    double guess_old = m_t[0];
	    double x,y,xp,yp,xpp,ypp,dp,dpp;
	    ON_3dPoint p;
	    ON_3dVector d1;
	    ON_3dVector d2;
	    m_trim->Ev2Der(guess, p, d1, d2);
	    int cnt = 0;
	    double d_old = 0;
	    double d = fabs(uv[1]-p[1]);
	    bu_log("Called Curve Estimation Routine.\n");
	    int MAX_CNT = 1000;
	    while ((cnt < MAX_CNT) && !NEAR_ZERO(d_old-d,tol)) {
		d_old = d;
		m_trim->Ev2Der(guess, p, d1, d2);
    		y = p[Y] - uv[Y];
    		x = p[X] - uv[X];
    		yp = d1[1];
    		xp = d1[0];
    		ypp = d2[1];
    		xpp = d2[0];
		d = fabs(uv[1]-p[1]);
    		dp = (2*yp*y+2*xp*x)/(2*sqrt(y*y+x*x));
    		dpp = (2*ypp*y+2*xpp*x+2*yp*yp+2*xp*xp)/(2*sqrt(y*y+x*x))-((2*yp*y+2*xp*x)*(2*yp*y+2*xp*x))/((4*y*y+4*x*x)*sqrt(4*y*y+4*x*x));
		guess = guess - dp/dpp;
		cnt++;
	    }
	    if (cnt == MAX_CNT) {
	    	bu_log("Iteration didn't converge.  Using linear test.\n");
		ON_3dPoint pstart = m_trim->PointAt(m_t[0]);
		ON_3dPoint pend = m_trim->PointAt(m_t[1]);
		bu_log("pstart[X]: %f, pstart[Y]: %f, pend[X]: %f, pend[Y]: %f, uv[X]: %f, p[X]: %f, uv[Y]: %f, p[Y]: %f, linear_test_y: %f\n", pstart[0], pstart[1], pend[0], pend[1], uv[X], p[X], uv[Y], p[Y],  m_start[Y] + m_slope*(uv[X] - m_start[X]) );
		return m_start[Y] + m_slope*(uv[X] - m_start[X]);	
	    } else {
	        return p[Y];
	    }
	}
/*	
	template<class BA>
	fastf_t 
	SubcurveBANode<BA>::getCurveEstimateOfV(const ON_2dPoint& uv, fastf_t tol ) const {
	        ON_3dVector tangent;
		point_t A,B;
		double  Ta,Tb;

		VMOVE(A,m_start);
		VMOVE(B,m_end);
		Ta = m_t[0];
		Tb = m_t[1];

		fastf_t dU = B[X] - A[X];
		fastf_t dT = Tb - Ta;
		fastf_t guess = Ta + (uv[X] - A[X]) * dT/dU;
		ON_3dPoint p = m_trim->PointAt(guess);

		int cnt=0;
		while ((cnt < 50) && (!NEAR_ZERO(p[X]-uv[X],tol))) {
		    if (p[X] < uv[X]) {
			Ta = guess;
			VMOVE(A,p);
		    } else {
			Tb = guess;
			VMOVE(B,p);
		    }
		    dU = B[X] - A[X];
		    dT = Tb - Ta;
		    guess = Ta + (uv[X] - A[X]) * dT/dU;
		    p = m_trim->PointAt(guess);
		    cnt++;
		}
		return p[Y];
	}
*/		
    //--------------------------------------------------------------------------------
    // Bounding volume hierarchy classes

    template<class BV>
    class BVNode;

    template<class BV>
    class BVSegment {
    public:
	BVNode<BV>* m_node;
	double m_near;
	double m_far;

	BVSegment() {}
	BVSegment(BVNode<BV>* node, double near, double far) :
	    m_node(node), m_near(near), m_far(far) {}
	BVSegment(const BVSegment& seg) : m_node(seg.m_node),
	    m_near(seg.m_near),
	    m_far(seg.m_far) {}
	BVSegment& operator=(const BVSegment& seg) {
	    m_node = seg.m_node;
	    m_near = seg.m_near;
	    m_far = seg.m_far;
	    return *this;
	}

	bool operator<(const BVSegment<BV>& seg) {
	    return m_near < seg.m_near;
	}
    };

    using namespace std;

    template<class BV>
    class BVNode {
    public:
	typedef vector<BVNode<BV>*> ChildList;
	typedef list<BVSegment<BV> > IsectList;

	ChildList m_children;
	BV m_node;
	ON_3dPoint m_estimate;

	BVNode();
	BVNode(const BV& node);
	virtual ~BVNode();

	void addChild(const BV& child);
	void addChild(BVNode<BV>* child);
	void removeChild(const BV& child);
	void removeChild(BVNode<BV>* child);
	virtual bool isLeaf() const;

	virtual int depth();
	virtual void getLeaves(list<BVNode<BV>*>& out_leaves);
	virtual ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
	virtual ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
	void GetBBox(double* min, double* max);

	virtual bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
	virtual bool intersectsHierarchy(ON_Ray& ray, std::list<BVSegment<BV> >* results = 0);

    private:
	BVNode<BV>* closer(const ON_3dPoint& pt, BVNode<BV>* left, BVNode<BV>* right);
    };

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
	void getLeaves(list<BRNode*>& out_leaves);
	void getLeavesAbove(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v);
	void getLeavesRight(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v);
	int depth();

    private:
	bool isLinear(const ON_Curve* curve, double min, double max);
	BRNode* subdivideCurve(const ON_Curve* curve, double min, double max, bool innerTrim, int depth);
	BRNode* curveBBox(const ON_Curve* curve, ON_Interval& t,bool leaf, bool innerTrim, const ON_BoundingBox& bb);
	BRNode* initialLoopBBox();
	
	ON_BrepFace* m_face;
	BRNode* m_root;
	list<BRNode*> m_sortedX;
	list<BRNode*> m_sortedY;
    };

    template<class BV>
    class SubsurfaceBVNode : public BVNode<BV> {
    public:
	SubsurfaceBVNode(const BV& node,
			 const ON_BrepFace* face,
			 const ON_Interval& u,
			 const ON_Interval& v,
			 bool checkTrim = false,
			 bool trimmed = false);

	bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
	bool isLeaf() const;
	bool isTrimmed(const ON_2dPoint& uv);
	bool doTrimming() const;
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);
	void getTrimsAbove(const ON_2dPoint& uv, list<BRNode*>& out_leaves);
	bool prepTrims(CurveTree *ct);

	const ON_BrepFace* m_face;
	ON_Interval m_u;
	ON_Interval m_v;
	bool m_checkTrim;
	bool m_trimmed;
    private:
	list<BRNode*> m_trims_above;
	list<BRNode*> m_trims_right;
    };

    typedef BVNode<ON_BoundingBox> BBNode;
    typedef SubsurfaceBVNode<ON_BoundingBox> SubsurfaceBBNode;

    //--------------------------------------------------------------------------------
    // Template Implementation
    template<class BV>
    inline
    BVNode<BV>::BVNode() { }

    template<class BV>
    inline
    BVNode<BV>::BVNode(const BV& node) : m_node(node) {
#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < 3; i++) {
#else
	for (size_t i = 0; i < 3; i++) {
#endif
	    double d = m_node.m_max[i] - m_node.m_min[i];
	    if (ON_NearZero(d, ON_ZERO_TOLERANCE)) {
		m_node.m_min[i] -= 0.001;
		m_node.m_max[i] += 0.001;
	    }
	}
    }

    template<class BV>
    inline
    BVNode<BV>::~BVNode() {
	// delete the children
#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < m_children.size(); i++) {
#else
	for (size_t i = 0; i < m_children.size(); i++) {
#endif
	    delete m_children[i];
	}
    }

    template<class BV>
    inline void
    BVNode<BV>::addChild(const BV& child) {
	m_children.push_back(new BVNode<BV>(child));
    }

    template<class BV>
    inline void
    BVNode<BV>::addChild(BVNode<BV>* child) {
	m_children.push_back(child);
    }

    template<class BV>
    inline void
    BVNode<BV>::removeChild(const BV& child) {
	// XXX implement
    }

    template<class BV>
    inline void
    BVNode<BV>::removeChild(BVNode<BV>* child) {
	// XXX implement
    }

    template<class BV>
    inline bool
    BVNode<BV>::isLeaf() const { return false; }

    template<class BV>
    inline void
    BVNode<BV>::GetBBox(double* min, double* max) {
	min[0] = m_node.m_min[0];
	min[1] = m_node.m_min[1];
	min[2] = m_node.m_min[2];
	max[0] = m_node.m_max[0];
	max[1] = m_node.m_max[1];
	max[2] = m_node.m_max[2];
    }


    template<class BV>
    inline bool
    BVNode<BV>::intersectedBy(ON_Ray& ray, double* tnear_opt, double* tfar_opt) {
	double tnear = -real.infinity();
	double tfar = real.infinity();
#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < 3; i++) {
#else
	for (size_t i = 0; i < 3; i++) {
#endif

	    //cout << "m_node.m_min[" << i << "]: " << m_node.m_min[i] << "\n";
	    //cout << "m_node.m_max[" << i << "]: " << m_node.m_max[i] << "\n";
	    if (ON_NearZero(ray.m_dir[i])) {
		if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i]) {
//		    cout << "ray.m_origin[" << i << "]: " << ray.m_origin[i] << "\n";
//		    cout << "m_node.m_min[" << i << "]: " << m_node.m_min[i] << "\n";
//		    cout << "m_node.m_max[" << i << "]: " << m_node.m_max[i] << "\n\n";
		    return false;
		}
	    }
	    else {
		double t1 = (m_node.m_min[i]-ray.m_origin[i]) / ray.m_dir[i];
		double t2 = (m_node.m_max[i]-ray.m_origin[i]) / ray.m_dir[i];
		if (t1 > t2) { double tmp = t1; t1 = t2; t2 = tmp; } // swap
		if (t1 > tnear) tnear = t1;
		if (t2 < tfar) tfar = t2;
		if (tnear > tfar) /* box is missed */ return false;
		if (tfar < 0) /* box is behind ray */ return false;
	    }
	}
	if (tnear_opt != 0 && tfar_opt != 0) { *tnear_opt = tnear; *tfar_opt = tfar; }
	return true;
    }

    template<class BV>
    bool
    BVNode<BV>::intersectsHierarchy(ON_Ray& ray, std::list<BVSegment<BV> >* results_opt) {
	double tnear, tfar;
	bool intersects = intersectedBy(ray, &tnear, &tfar);
	if (intersects && isLeaf()) {
	    if (results_opt != 0) results_opt->push_back(BVSegment<BV>(this, tnear, tfar));
	} else if (intersects) {
// XXX: bug in g++? had to typedef the below to get it to work!
//       for (std::list<BVNode<BV>*>::iterator j = m_children.begin(); j != m_children.end(); j++) {
// 	(*j)->intersectsHierarchy(ray, results_opt);
//       }
#if defined(_WIN32) && !defined(__CYGWIN__)
	    for (int i = 0; i < m_children.size(); i++) {
#else
	    for (size_t i = 0; i < m_children.size(); i++) {
#endif
		m_children[i]->intersectsHierarchy(ray, results_opt);
	    }
	}

	return intersects;
    }

    template<class BV>
    int
    BVNode<BV>::depth() {
	int d = 0;
#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 0; i < m_children.size(); i++) {
#else
	for (size_t i = 0; i < m_children.size(); i++) {
#endif
	    d = 1 + max(d, m_children[i]->depth());
	}
	return d;
    }

    template<class BV>
    void
    BVNode<BV>::getLeaves(list<BVNode<BV>*>& out_leaves) {
	if (m_children.size() > 0) {
#if defined(_WIN32) && !defined(__CYGWIN__)
	    for (int i = 0; i < m_children.size(); i++) {
#else
	    for (size_t i = 0; i < m_children.size(); i++) {
#endif
		m_children[i]->getLeaves(out_leaves);
	    }
	} else {
	    out_leaves.push_back(this);
	}
    }


    template<class BV>
    BVNode<BV>*
    BVNode<BV>::closer(const ON_3dPoint& pt, BVNode<BV>* left, BVNode<BV>* right) {
	double ldist = pt.DistanceTo(left->m_estimate);
	double rdist = pt.DistanceTo(right->m_estimate);
	TRACE("\t" << ldist << " < " << rdist);
	if (ldist < rdist) return left;
	else return right;
    }

    template<class BV>
    ON_2dPoint
    BVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt) {
	ON_Interval u, v;
	return getClosestPointEstimate(pt, u, v);
    }

    template<class BV>
    ON_2dPoint
    BVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	TRACE("getClosestPointEstimate(" << PT(pt) << ")");
	if (m_children.size() > 0) {
	    BBNode* closestNode = m_children[0];
#if defined(_WIN32) && !defined(__CYGWIN__)
	    for (int i = 1; i < m_children.size(); i++) {
#else
	    for (size_t i = 1; i < m_children.size(); i++) {
#endif
		closestNode = closer(pt, closestNode, m_children[i]);
		TRACE("\t" << PT(closestNode->m_estimate));
	    }
	    return closestNode->getClosestPointEstimate(pt, u, v);
	}
	throw new exception();
    }

    template<class BV>
    inline SubsurfaceBVNode<BV>::SubsurfaceBVNode(const BV& node,
						  const ON_BrepFace* face,
						  const ON_Interval& u,
						  const ON_Interval& v,
						  bool checkTrim,
						  bool trimmed)
	: BVNode<BV>(node), m_face(face), m_u(u), m_v(v), m_checkTrim(checkTrim), m_trimmed(trimmed) { }

    template<class BV>
    inline bool
    SubsurfaceBVNode<BV>::intersectedBy(ON_Ray& ray, double *tnear, double *tfar) {
	return !m_trimmed && BVNode<BV>::intersectedBy(ray, tnear, tfar);
    }

    template<class BV>
    inline bool
    SubsurfaceBVNode<BV>::isLeaf() const {
	return true;
    }

	template<class BV>
    inline bool
    SubsurfaceBVNode<BV>::isTrimmed(const ON_2dPoint& uv) {
		SubcurveBRNode* br;
		list<BRNode*> trims;
		point_t bmin,bmax;
		
		if (m_checkTrim) {
		
		getTrimsAbove(uv,trims);
		
		if (trims.empty()) {
			return true;
#if 0
		} else if (trims.size() == 1) {
			br = dynamic_cast<SubcurveBRNode*>(*trims.begin());
			return br->isTrimmed(uv);
#endif
		} else {//find closest BB
			list<BRNode*>::iterator i;
			SubcurveBRNode* closest = NULL;
			fastf_t currHeight; //=999999.9;
			point_t min,max;
			for( i=trims.begin();i!=trims.end();i++) {
				br = dynamic_cast<SubcurveBRNode*>(*i);
				//if (i == trims.begin()) {
				//	//double le,ce;
				//	//le = br->getLinearEstimateOfV(uv[X]);
				////	//ce = br->getCurveEstimateOfV(uv,0.0001);
				//	dist = uv[Y] - br->getLinearEstimateOfV(uv[X]);
				//    closest = br;
				//} else {
				fastf_t v = br->getLinearEstimateOfV(uv[X]); // - uv[Y];
				//v = br->getCurveEstimateOfV(uv,0.0001);
					br->GetBBox(bmin,bmax);
					if ((v > uv[Y]) && ((v <= bmax[Y]) && (v >= bmin[Y]))) {
					    if (closest == NULL){
						currHeight = v;
						closest = br;
					    } else if (v < currHeight ) {
						currHeight = v;
						closest = br;
					    }
					}
				//}
			}
			if (closest == NULL) {
				return true;
			} else {
				return closest->isTrimmed(uv);
			}
		}
		} else {
		return m_trimmed;
		}
	}

    template<class BV>
    inline bool
    SubsurfaceBVNode<BV>::doTrimming() const {
	return m_checkTrim;
    }

    template<class BV>
    ON_2dPoint
    SubsurfaceBVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt) {
	ON_Interval u, v;
	return getClosestPointEstimate(pt, u, v);
    }

    template<class BV>
    void
    SubsurfaceBVNode<BV>::getTrimsAbove(const ON_2dPoint& uv, list<BRNode*>& out_leaves) {
	    point_t bmin,bmax;
	    double dist;
	    for (list<BRNode*>::iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++) {
			SubcurveBRNode* br = dynamic_cast<SubcurveBRNode*>(*i);
			br->GetBBox(bmin,bmax);
			dist = 0.03*DIST_PT_PT(bmin,bmax);
			if ((uv[X] > bmin[X]-dist) && (uv[X] < bmax[X]+dist))
				out_leaves.push_back(br);
	    }
		
    }

    template<class BV>
    ON_2dPoint
    SubsurfaceBVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	double uvs[5][2] = {{m_u.Min(), m_v.Min()},  // include the corners for an easy refinement
			    {m_u.Max(), m_v.Min()},
			    {m_u.Max(), m_v.Max()},
			    {m_u.Min(), m_v.Max()},
			    {m_u.Mid(), m_v.Mid()}}; // include the estimate
	ON_3dPoint corners[5];
	const ON_Surface* surf = m_face->SurfaceOf();

	u = m_u;
	v = m_v;

	// XXX - pass these in from SurfaceTree::surfaceBBox() to avoid this recalculation?
	if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) ||
	    !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
	    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) ||
	    !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3])) {
	    throw new exception(); // XXX - fix this
	}
	corners[4] = BVNode<BV>::m_estimate;

	// find the point on the surface closest to pt
	int mini = 0;
	double mindist = pt.DistanceTo(corners[mini]);
	double tmpdist;
#if defined(_WIN32) && !defined(__CYGWIN__)
	for (int i = 1; i < 5; i++) {
#else
	for (size_t i = 1; i < 5; i++) {
#endif
	    tmpdist = pt.DistanceTo(corners[i]);
	    TRACE("\t" << mindist << " < " << tmpdist);
	    if (tmpdist < mindist) {
		mini = i;
		mindist = tmpdist;
	    }
	}
	TRACE("Closest: " << mindist << "; " << PT2(uvs[mini]));
	return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
    }

    template<class BV>
    inline bool
    SubsurfaceBVNode<BV>::prepTrims(CurveTree *ct) {
		list<BRNode*>::iterator i;
		SubcurveBRNode* br;
		//	point_t surfmin,surfmax;
		point_t curvemin,curvemax;
		double dist; 
		
		//	BVNode<BV>::GetBBox(surfmin,surfmax);
		
		m_trims_above.clear();
		m_trims_right.clear();
		ct->getLeavesAbove( m_trims_above,m_u,m_v);
		ct->getLeavesRight( m_trims_right,m_u,m_v);
		m_trims_above.sort(sortY);
		m_trims_right.sort(sortX);
		
		if ( m_trims_above.empty() || m_trims_right.empty()) {
			m_trimmed = true;
			m_checkTrim = false;
		} else if (!m_trims_above.empty()) {//trimmed above check contains
			i = m_trims_above.begin();
			br = dynamic_cast<SubcurveBRNode*>(*i);
			br->GetBBox(curvemin,curvemax);
			dist = 0.03*DIST_PT_PT(curvemin,curvemax);
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
					SubcurveBRNode* bs;
					bs = dynamic_cast<SubcurveBRNode*>(*i);
					point_t smin,smax;
					bs->GetBBox(smin,smax);
					if ((smin[Y] >= curvemax[Y]) || (smin[X] >= curvemax[X]) || (smax[X] <= curvemin[X])) { //can determine inside/outside without closer inspection
						if (br->m_XIncreasing) {
							m_trimmed=true;
							m_checkTrim=false;
						} else {
							m_trimmed=false;
							m_checkTrim=false;
						} 
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
		return true;
    }

    //--------------------------------------------------------------------------------
    // SurfaceTree declaration
    class SurfaceTree {
    public:
	SurfaceTree(ON_BrepFace* face);
	~SurfaceTree();

	BBNode* getRootNode() const;

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
	void getLeaves(list<BBNode*>& out_leaves);
	int depth();

    private:
	bool isFlat(const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v);
	BBNode* subdivideSurface(const ON_Interval& u, const ON_Interval& v, int depth);
	BBNode* surfaceBBox(bool leaf, const ON_Interval& u, const ON_Interval& v);

	ON_BrepFace* m_face;
	BBNode* m_root;
    };


    /**-------------------------------------------------------------------------------
     *                    g e t _ c l o s e s t _ p o i n t
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
			   double tolerance = -1.0);


    /**                  p u l l b a c k _ c u r v e
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
     *    given two parameters on the curve t1 and t2 (which map to points p1 and p2 on the curve)
     *    let m be a parameter randomly chosen near the middle of the interval [t1, t2]
     *                                                              ____
     *    then the curve between t1 and t2 is flat if distance(C(m), p1p2) < flatness
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
				    double tolerance = 1.0e-6,
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
