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
 * These routines build on the openNURBS libraries, implementing the
 * Surface Tree NURBS surface breakdown described by Abert(2005).
 * The basic idea is to identify "nearly" flat subdomains of each
 * NURBS surface, the center of which can be used to get a good initial
 * guess for Newton iteration based ray/surface intersection algorithms.
 * Component nodes of these surfaces trees are referred to as Bounding
 * Volume Nodes, and the tree as a Bounding Volume Hierarchy.
 *
 * The same basic idea of using subdomains is put to work implementing
 * the trimming algorithms.  Nodes in this tree are referred to as
 * Bounding Area Nodes (since they describe an area in UV space rather
 * than a volume in 3 space) and the tree as a Bounding Area Hierarchy
 *
 */
 
#ifndef __OPENNURBS_EXT
#define __OPENNURBS_EXT

#include "opennurbs.h"
#include <vector>
#include <list>
#include <limits>
#include <iostream>
#include "vmath.h"
#include "vector.h"

/* Maximum limits for the depth of leaves in surface and curve trees */
#define BREP_MAX_FT_DEPTH 8
#define BREP_MAX_LN_DEPTH 20


/* Flatness criteria - how close to planer/linear surfaces and curves must
 * be to ensure well behaved solution convergence.
 */
#define BREP_SURFACE_FLATNESS 0.9
#define BREP_CURVE_FLATNESS 0.95

/* trim curve point sampling count for isLinear()*/
#define BREP_BB_CRV_PNT_CNT 10

/* subdivision size factors */
#define BREP_TRIM_SUB_FACTOR 0.01

// grows 3D BBox along each axis by this factor
#define BBOX_GROW_3D 0.1


static std::numeric_limits<double> real;

namespace brlcad {


  inline void
      distribute(const int count, const ON_3dVector* v, double x[], double y[], double z[]) {
	   for (int i = 0; i < count; i++) {
	       x[i] = v[i].x;
	       y[i] = v[i].y;
	       z[i] = v[i].z;
	   }
      }

    
  using namespace std;
  
  /**********************************************************************
   *                  Bounding Area Hierarchy Classes
   *
   *  Now that the basic hierarchy classes are laid out, use them to
   *  create a more specific node class intended to hold surface area
   *  bounding information.
   *
   **********************************************************************/

   template<class BH>
    class BANode;


   template<class BH>
    class BANode {
      public:
	BANode(const ON_BrepFace* face);
	BANode(const ON_BrepFace* face, const ON_BrepLoop* loop, bool innerTrim);
	BANode(const ON_BrepFace* face, const ON_BrepLoop* loop,
	       const ON_Curve* curve, ON_Interval& t, bool innerTrim);
	~BANode();
	
	// List of all children of a given node
	typedef vector<BANode<BH>*> ChildList;
	ChildList m_children;

	// Bounding Box
	BH m_BBox;

	// Test if this node is a leaf node in the hierarchy
	bool isLeaf();

	// Return all leaves below this node that are leaf nodes
	void getLeaves(list<BANode<BH>*>& out_leaves);
	
	// Functions to add and remove child nodes from a this node.
	void addChild(const BANode& child);
	void addChild(BANode<BH>* child);
	void removeChild(const BANode& child);
	void removeChild(BANode<BH>* child);

	// Report the depth of this node in the hierarchy
	int depth();

	// Get 2 points defining bounding box
	void GetBBox(double* min, double* max);
   
	// Surface Information
	const ON_BrepFace* m_face;
	ON_Interval m_u;
	ON_Interval m_v;
	bool FaceNode;

	// Loop Information
	const ON_BrepLoop* m_loop;
	bool LoopNode;
	
	// Curve Information
	const ON_Curve* m_curve;
	ON_Interval m_t;

	// Trimming Information
	bool m_XIncreasing;
	bool m_innerTrim;
	
	int isTrimmed(const ON_2dPoint& uv, double &trimdist);
	
	fastf_t getLinearEstimateOfV( fastf_t u );
	fastf_t getCurveEstimateOfV( fastf_t u, fastf_t tol ) const;
	
      private:
	fastf_t m_slope;
	fastf_t m_bb_diag;
	ON_3dPoint m_start;
	ON_3dPoint m_end;
	bool m_Vertical;
    };

   // For the ON_BoundingBox definition see opennurbs_bounding_box.h lines 25-484. 
  typedef BANode<ON_BoundingBox> BRNode;
 
  //********************************************
  //  Bounding Area Template Implementation
  //********************************************

  template<class BH>
      inline BANode<BH>::BANode(const ON_BrepFace* face) : m_face(face) {
	  FaceNode = true;
	  LoopNode = false;
      	  VSETALL(this->m_BBox.m_min,MAX_FASTF);
	  VSETALL(this->m_BBox.m_max,-MAX_FASTF);
      }
 
  template<class BH>
      inline BANode<BH>::BANode(const ON_BrepFace* face, const ON_BrepLoop* loop, bool innerTrim) 
      : m_face(face), m_loop(loop), m_innerTrim(innerTrim) {
	  FaceNode = false;
	  LoopNode = true;
	  VSETALL(this->m_BBox.m_min,MAX_FASTF);
	  VSETALL(this->m_BBox.m_max,-MAX_FASTF);
      } 

  template<class BH>
      inline BANode<BH>::BANode(const ON_BrepFace* face, const ON_BrepLoop* loop, 
	     const ON_Curve* curve, ON_Interval& t, bool innerTrim)
      : m_face(face), m_loop(loop), m_curve(curve),  m_t(t), m_innerTrim(innerTrim) {
	  FaceNode = false;
	  LoopNode = false;
	  m_start = curve->PointAt(m_t[0]);
	  m_end = curve->PointAt(m_t[1]);
	  VSETALL(this->m_BBox.m_min,MAX_FASTF);
	  VSETALL(this->m_BBox.m_max,-MAX_FASTF);
	  // check for vertical segments they can be removed from
	  // w
	  // trims above (can't tell direction and don't need
          if ( NEAR_ZERO(m_end[X]-m_start[X], DBL_EPSILON) ) {
	      m_Vertical = true;
	      if (m_innerTrim) {
		  m_XIncreasing = false;
	      } else {
		  m_XIncreasing = true;
	      }
	  } else {
	      m_Vertical = false;
	      if ( (m_end[X] - m_start[X]) > 0.0 ) {
		  m_XIncreasing = true;
	      } else {
		  m_XIncreasing = false;
	      }
	      m_slope = (m_end[Y] - m_start[Y])/(m_end[X] - m_start[X]);
	  }
	  m_bb_diag = DIST_PT_PT(m_start,m_end);
      }

  template<class BH>
    inline BANode<BH>::~BANode() {
      // When destroying a node, always destroy its children
      for (int i = 0; i < m_children.size(); i++) {
	delete m_children[i];
      }
    }


  template<class BH>
    inline bool
    BANode<BH>::isLeaf() {
      if (m_children.size() == 0) return true;
      return false;
    }
 
  template<class BH>
    void BANode<BH>::getLeaves(list<BANode<BH>*>& out_leaves) {
      if (m_children.size() > 0) {
	for (int i = 0; i < m_children.size(); i++) {
	  m_children[i]->getLeaves(out_leaves);
	}
      } else {
	out_leaves.push_back(this);
      }
    }
  
  template<class BH>
    inline void
    BANode<BH>::addChild(const BANode& child) {
      m_children.push_back(new BANode<BH>(child));
    }

  template<class BH>
    inline void
    BANode<BH>::addChild(BANode<BH>* child) {
      m_children.push_back(child);
    }

  template<class BH>
    inline void
    BANode<BH>::removeChild(const BANode& child) {
      //todo
    }

  template<class BH>
    inline void
    BANode<BH>::removeChild(BANode<BH>* child) {
      //todo
    }

  template<class BH>
    int BANode<BH>::depth() {
      int d = 0;
      for (int i = 0; i < m_children.size(); i++) {
	d = 1 + max(d, m_children[i]->depth());
      }
      return d;
    }

  template<class BH>
    inline void BANode<BH>::GetBBox(double* min, double* max) {
      min[0] = m_BBox.m_min[0];
      min[1] = m_BBox.m_min[1];
      min[2] = m_BBox.m_min[2];
      max[0] = m_BBox.m_max[0];
      max[1] = m_BBox.m_max[1];
      max[2] = m_BBox.m_max[2];
    }


  
  template<class BH>
    fastf_t BANode<BH>::getLinearEstimateOfV( fastf_t u ) {
      fastf_t v = m_start[Y] + m_slope*(u - m_start[X]);
      return v;
    }

  template<class BH>
    fastf_t BANode<BH>::getCurveEstimateOfV( fastf_t u, fastf_t tol ) const {
      ON_3dVector tangent;
      point_t A,B;
      double  Ta,Tb;
      
      VMOVE(A,m_start);
      VMOVE(B,m_end);
      Ta = m_t[0];
      Tb = m_t[1];
      
      fastf_t dU = B[X] - A[X];
      fastf_t dT = Tb - Ta;
      fastf_t guess = Ta + (u - A[X]) * dT/dU;
      ON_3dPoint p = m_curve->PointAt(guess);

      int cnt=0;
      while ((cnt < 50) && (!NEAR_ZERO(p[X]-u,tol))) {
	if (p[X] < u) {
	  Ta = guess;
	  VMOVE(A,p);
	} else {
	  Tb = guess;
	  VMOVE(B,p);
	}
	dU = B[X] - A[X];
	dT = Tb - Ta;
	guess = Ta + (u - A[X]) * dT/dU;
	p = m_curve->PointAt(guess);
	cnt++;
      }
      return p[Y];
    }

  template<class BH>
      int BANode<BH>::isTrimmed(const ON_2dPoint& uv, double &trimdist) {
	  point_t bmin,bmax;
	  BANode<BH>::GetBBox(bmin,bmax);
	  if ((bmin[X] <= uv[X]) && (uv[X] <= bmax[X])) { //if check trim and in BBox
	      fastf_t v = getCurveEstimateOfV(uv[X],0.0000001);
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
	  }
      }
      
   /**********************************************************************
   *                        Curve Tree Class
   *
   *  Now that Bounding Area Nodes are defined, define the
   *  tree that makes use of them.
   *
   **********************************************************************/

   class CurveTree {
    public:
        CurveTree(const ON_BrepFace* face);
        ~CurveTree();

        BRNode* getRootNode() const;

        // Return just the leaves of the tree
        void getLeaves(list<BRNode*>& out_leaves);

	// Return the list of leaf nodes in the curve tree that
	// are above the lowest uv edge (i.e. inside or above the uv box)
	// defined by u and v.
        void getLeavesAbove(list<BRNode*>& out_leaves, const ON_Interval& u, const ON_Interval& v);

	void GetBAChildren(BRNode*, int depth, double min, double max);
	void TrimBBBuild(BRNode*);
	void ParentBBBuild(BRNode*);

	int depth();

    private:
        fastf_t getVerticalTangent(const ON_Curve *curve,fastf_t min,fastf_t max);
        fastf_t getHorizontalTangent(const ON_Curve *curve,fastf_t min,fastf_t max);
        bool getHVTangents(const ON_Curve* curve, ON_Interval& t, list<fastf_t>& list);
        bool isLinear(const ON_Curve* curve, double min, double max);
        
        const ON_BrepFace* m_face;
        BRNode* m_root;
        list<BRNode*> m_sortedX;
        list<BRNode*> m_sortedY;
    };

   // These functions are needed in other definitions, so declare them now.
   extern bool sortX(BRNode* first, BRNode* second);
   extern bool sortY(BRNode* first, BRNode* second);


   /**********************************************************************
   *                  Bounding Volume Hierarchy Classes
   *
   *  As with Bounding Area Hierarchy Classes, use the basic classes to
   *  create a more specific node class intended to hold volumetric
   *  bounding information.
   *
   **********************************************************************/
  
  template<class BH>
    class BVNode;
   
  template<class BH>
    class BVNode {
      public:
	BVNode(const ON_Brep* brep);
	BVNode(const ON_BrepFace* face, const ON_Surface* surface,
	    ON_Interval& u, ON_Interval& v, CurveTree* ctree);
	BVNode(const ON_BrepFace* face, const ON_Surface* surface,
	    ON_Interval& u, ON_Interval& v, CurveTree* ctree,
	    ON_3dPoint& p0, ON_3dPoint& p2, ON_3dPoint& p6, 
	    ON_3dPoint& p10, ON_3dPoint& p12,
	    ON_3dVector& n0, ON_3dVector& n2, ON_3dVector& n6, 
	    ON_3dVector& n10, ON_3dVector& n12);
	~BVNode();

	// List of all children of a given node
	typedef vector<BVNode<BH>*> ChildList;
	ChildList m_children;

	// Bounding Box
	BH m_BBox;

	// Test if this node is a leaf node in the hierarchy
	bool isLeaf();

	// Return all leaves below this node that are leaf nodes
	void getLeaves(list<BVNode<BH>*>& out_leaves);
	
	// Functions to add and remove child nodes from a this node.
	void addChild(const BVNode& child);
	void addChild(BVNode<BH>* child);
	void removeChild(const BVNode& child);
	void removeChild(BVNode<BH>* child);

	// Report the depth of this node in the hierarchy
	int depth();

	// Get 2 points defining bounding box
	void GetBBox(double* min, double* max);
 
	// Surface Information
	const ON_BrepFace* m_face;
	const ON_Surface* m_surface;
	ON_Interval m_u;
	ON_Interval m_v;

	// Curve Information
	CurveTree* m_ctree;
	bool m_checkTrim;
	bool m_trimmed;
	
	// Point used for closeness testing - usually
	// based on evaluation of the curve/surface at
	// the center of the parametric domain
	ON_3dPoint m_estimate;

	// Array holding point and normal information needed at
	// each node - for leaf nodes not all of these
	// normals will need to be evaluated.
	ON_3dPoint m_corners[13];
	ON_3dVector m_normals[13];
	
	// Report if a given uv point is within the uv boundardies
	// defined by a node.  
	bool containsUV(const ON_2dPoint& uv);


	// Check if the surface portion bounded by a node satisfies
	// the flatness criteria
	bool isFlat();
	
	// Test whether a ray intersects the 3D bounding volume
	// of the node - if so, return a list of leaf nodes
	// below the current node that intersect the ray.  If the
	// current node is a leaf node and intersects, return self.
	bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
        bool intersectsHierarchy(ON_Ray& ray, list<BVNode<ON_BoundingBox>*>& results = 0);

	// Need to rework getClosestPointEstimate to handle the new
	// tree structure where leaf nodes aren't special cased...
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
	ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v);

	// Trimming related functions
	bool doTrimming() const;
	void getTrimsAbove(const ON_2dPoint& uv, list<BRNode*>& out_leaves);
	bool isTrimmed(const ON_2dPoint& uv);
	int NodeTrimmed();


     private:
	BVNode<BH>* closer(const ON_3dPoint& pt, BVNode<BH>* left, BVNode<BH>* right);
	list<BRNode*> m_trims_above;
    };

  // For the ON_BoundingBox definition see opennurbs_bounding_box.h lines 25-484. 
  typedef BVNode<ON_BoundingBox> BBNode;
 

  //**************************************************
  // Bounding Volume Hierarchy Template Implementation
  //**************************************************

#define SW 1
#define SE 2
#define NW 3
#define NE 4

  template<class BH>
     inline BVNode<BH>::BVNode(const ON_Brep* brep) {
	this->m_BBox = brep->BoundingBox();
     }

  template<class BH>
    inline BVNode<BH>::BVNode(const ON_BrepFace* face, const ON_Surface* surface,
	ON_Interval& u, ON_Interval& v, CurveTree* ctree) 
    : m_face(face), m_surface(surface), m_u(u), m_v(v), m_ctree(ctree) { 
	point_t min, max;
	vect_t delta;
	VSETALL(min, MAX_FASTF);
	VSETALL(max, -MAX_FASTF);
	surface->EvNormal(m_u.Min(),m_v.Min(), m_corners[0], m_normals[0]);
        surface->EvNormal(m_u.Max(),m_v.Min(), m_corners[2], m_normals[2]);
	surface->EvNormal(m_u.ParameterAt(0.25), m_v.ParameterAt(0.25), m_corners[3], m_normals[3]);
	surface->EvNormal(m_u.ParameterAt(0.75), m_v.ParameterAt(0.25), m_corners[4], m_normals[4]);
	surface->EvNormal(m_u.ParameterAt(0.5), m_v.ParameterAt(0.5), m_corners[6], m_normals[6]);
	surface->EvNormal(m_u.ParameterAt(0.25), m_v.ParameterAt(0.75), m_corners[8], m_normals[8]);
	surface->EvNormal(m_u.ParameterAt(0.75), m_v.ParameterAt(0.75), m_corners[9], m_normals[9]);
	surface->EvNormal(m_u.Min(), m_v.Max(), m_corners[10], m_normals[10]);
	surface->EvNormal(m_u.Max(), m_v.Max(), m_corners[12], m_normals[12]);
	surface->EvPoint(u.Mid(),v.Mid(),m_estimate);
        VMINMAX(min, max, ((double*)m_corners[0]));
        VMINMAX(min, max, ((double*)m_corners[2]));
        VMINMAX(min, max, ((double*)m_corners[3]));
        VMINMAX(min, max, ((double*)m_corners[4]));
        VMINMAX(min, max, ((double*)m_corners[6]));
        VMINMAX(min, max, ((double*)m_corners[8]));
        VMINMAX(min, max, ((double*)m_corners[9]));
        VMINMAX(min, max, ((double*)m_corners[10]));
        VMINMAX(min, max, ((double*)m_corners[12]));
	VSUB2(delta, max, min);
	VSCALE(delta, delta, BBOX_GROW_3D);
	VSUB2(min, min, delta);
	VADD2(max, max, delta);
	this->m_BBox = ON_BoundingBox(ON_3dPoint(min),ON_3dPoint(max));
	m_trims_above.clear();
	m_ctree->getLeavesAbove(m_trims_above, m_u, m_v);
	m_trims_above.sort(sortY);
    }
   
  template<class BH>
    inline BVNode<BH>::BVNode(const ON_BrepFace* face, const ON_Surface* surface, ON_Interval& u, ON_Interval& v, CurveTree* ctree, 
	    ON_3dPoint& p0, ON_3dPoint& p2, ON_3dPoint& p6, ON_3dPoint& p10, ON_3dPoint& p12,
	    ON_3dVector& n0, ON_3dVector& n2, ON_3dVector& n6, ON_3dVector& n10, ON_3dVector& n12
	    )
    : m_face(face), m_surface(surface), m_u(u), m_v(v), m_ctree(ctree) {
	m_corners[0] = p0;
	m_corners[2] = p2;
	m_corners[6] = p6;
	m_corners[10] = p10;
	m_corners[12] = p12;
	m_normals[0] = n0;
	m_normals[2] = n2;
	m_normals[6] = n6;
	m_normals[10] = n10;
	m_normals[12] = n12;
	point_t min, max;
	vect_t delta;
	VSETALL(min, MAX_FASTF);
	VSETALL(max, -MAX_FASTF);
	surface->EvNormal(m_u.ParameterAt(0.25), m_v.ParameterAt(0.25), m_corners[3], m_normals[3]);
	surface->EvNormal(m_u.ParameterAt(0.75), m_v.ParameterAt(0.25), m_corners[4], m_normals[4]);
	surface->EvNormal(m_u.ParameterAt(0.25), m_v.ParameterAt(0.75), m_corners[8], m_normals[8]);
	surface->EvNormal(m_u.ParameterAt(0.75), m_v.ParameterAt(0.75), m_corners[9], m_normals[9]);
	surface->EvPoint(u.Mid(),v.Mid(),m_estimate);
	VMINMAX(min, max, ((double*)m_corners[0]));
        VMINMAX(min, max, ((double*)m_corners[2]));
        VMINMAX(min, max, ((double*)m_corners[3]));
        VMINMAX(min, max, ((double*)m_corners[4]));
        VMINMAX(min, max, ((double*)m_corners[6]));
        VMINMAX(min, max, ((double*)m_corners[8]));
        VMINMAX(min, max, ((double*)m_corners[9]));
        VMINMAX(min, max, ((double*)m_corners[10]));
        VMINMAX(min, max, ((double*)m_corners[12]));
	VSUB2(delta, max, min);
	VSCALE(delta, delta, BBOX_GROW_3D);
	VSUB2(min, min, delta);
	VADD2(max, max, delta);
	this->m_BBox = ON_BoundingBox(ON_3dPoint(min),ON_3dPoint(max));
	m_trims_above.clear();
	m_ctree->getLeavesAbove(m_trims_above, m_u, m_v);
	m_trims_above.sort(sortY);
    }
 
  template<class BH>
    inline BVNode<BH>::~BVNode() {
      // When destroying a node, always destroy its children
      for (int i = 0; i < m_children.size(); i++) {
	delete m_children[i];
      }
    }


  template<class BH>
    inline bool
    BVNode<BH>::isLeaf() {
      if (m_children.size() == 0) return true;
      return false;
    }
 
  template<class BH>
    void BVNode<BH>::getLeaves(list<BVNode<BH>*>& out_leaves) {
      if (m_children.size() > 0) {
	for (int i = 0; i < m_children.size(); i++) {
	  m_children[i]->getLeaves(out_leaves);
	}
      } else {
	out_leaves.push_back(this);
      }
    }
  
  template<class BH>
    inline void
    BVNode<BH>::addChild(const BVNode& child) {
      m_children.push_back(new BVNode<BH>(child));
    }

  template<class BH>
    inline void
    BVNode<BH>::addChild(BVNode<BH>* child) {
      m_children.push_back(child);
    }

  template<class BH>
    inline void
    BVNode<BH>::removeChild(const BVNode& child) {
      //todo
    }

  template<class BH>
    inline void
    BVNode<BH>::removeChild(BVNode<BH>* child) {
      //todo
    }

  template<class BH>
    int BVNode<BH>::depth() {
      int d = 0;
      for (int i = 0; i < m_children.size(); i++) {
	d = 1 + max(d, m_children[i]->depth());
      }
      return d;
    }

  template<class BH>
    inline void BVNode<BH>::GetBBox(double* min, double* max) {
      min[0] = m_BBox.m_min[0];
      min[1] = m_BBox.m_min[1];
      min[2] = m_BBox.m_min[2];
      max[0] = m_BBox.m_max[0];
      max[1] = m_BBox.m_max[1];
      max[2] = m_BBox.m_max[2];
    }


  template<class BH>
    bool BVNode<BH>::containsUV(const ON_2dPoint& uv) {
      if ((uv[0] > m_u[0]) && (uv[0] < m_u[1]) && (uv[1] > m_v[0]) && (uv[1] < m_v[1])) {
	return true;
      } else {
	return false;
      }
    }

  // Private
  template<class BH>
    BVNode<BH>* BVNode<BH>::closer(const ON_3dPoint& pt, BVNode<BH>* left, BVNode<BH>* right) {
      double ldist = pt.DistanceTo(left->m_estimate);
      double rdist = pt.DistanceTo(right->m_estimate);
      if (ldist < rdist) return left;
      return right;
    }


  template<class BH>
    inline bool BVNode<BH>::intersectedBy(ON_Ray& ray, double* tnear_opt, double* tfar_opt) {
      double tnear = -real.infinity();
      double tfar = real.infinity();
      for (int i = 0; i < 3; i++) {
	if (ON_NearZero(ray.m_dir[i])) {
	  if (ray.m_origin[i] < this->m_BBox.m_min[i] || ray.m_origin[i] > this->m_BBox.m_max[i]) {
	    return false;
	  }
	} else {
	  double t1 = (this->m_BBox.m_min[i]-ray.m_origin[i]) / ray.m_dir[i];
	  double t2 = (this->m_BBox.m_max[i]-ray.m_origin[i]) / ray.m_dir[i];
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
  
    template<class BH>
      bool BVNode<BH>::intersectsHierarchy(ON_Ray& ray, list<BBNode*>& results_opt) {
	double tnear, tfar;
	bool intersects = intersectedBy(ray, &tnear, &tfar);
	if (intersects && this->isLeaf()) {
	  results_opt.push_back(this);
	} else if (intersects) {
	  for (int i = 0; i < this->m_children.size(); i++) {
	    this->m_children[i]->intersectsHierarchy(ray, results_opt);
	  }
	}
	return intersects;
      }


    /**
     * Determine whether a given surface is flat enough, i.e. it falls
     * beneath our simple flatness constraints. The flatness constraint in
     * this case is a sampling of normals across the surface such that
     * the product of their combined dot products is close to 1.
     *
     * @f[ \prod_{i=1}^{7} n_i \dot n_{i+1} = 1 @f]
     *
     * Would be a perfectly flat surface. Generally something in the range
     * 0.8-0.9 should suffice (according to Abert, 2005).
     *
     * The following sample pattern is chosen, instead of that used in
     * Abert, so that normals can be reused in child evals:
     *
     * (+ are normals passed from parent except at the root node,
     *  * are evaluated at the node level, and & are calculated
     *  if it is found that the node is not flat to be passed
     *  to the children to be created.)
     * 
     *    +-------------&------------+
     *    |                          |
     *    |      *            *      |
     *    |                          |
     *  V &             +            &
     *    |                          |
     *    |      *            *      |
     *    |                          |
     *    +-------------&------------+
     *                   U
     *
     * For the first (root) node this involves calculating 13 normals
     * instead of 8, but all of these node calculations can be
     * passed to various children, lessening the amount of work
     * done at each subsequent node.  If a node is a leaf node,
     * it will require only four Eval operations.  Otherwise,
     * it will require 8, and values will be shared among child
     * nodes.  
     *
     *    10------------11----------12
     *    |                          |
     *    |      8            9      |
     *    |                          |
     *  V 5             6            7
     *    |                          |
     *    |      3            4      |
     *    |                          |
     *    0-------------1------------2
     *                   U
     *
     * The actual flatness test will use normals 0, 2, 3, 4, 6, 8, 9, 10 and 12.  
     */

    template<class BH>
	bool BVNode<BH>::isFlat() {
	    ON_3dVector normals[9];
	    normals[0] = m_normals[0];
	    normals[1] = m_normals[2];
	    normals[2] = m_normals[3];
	    normals[3] = m_normals[4];
	    normals[4] = m_normals[6];
	    normals[5] = m_normals[8];
	    normals[6] = m_normals[9];
	    normals[7] = m_normals[10];
	    normals[8] = m_normals[12];
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
    		product *= dots.foldr(1,dvec<4>::mul());
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
    		product *= dots.foldr(1,dvec<4>::mul(),3);
    	    }
	    return product >= BREP_SURFACE_FLATNESS;
	}  

    
    template<class BH>
	ON_2dPoint BVNode<BH>::getClosestPointEstimate(const ON_3dPoint& pt) {
	    ON_Interval u, v;
	    return getClosestPointEstimate(pt, u, v);
	}

    template<class BH>
	ON_2dPoint BVNode<BH>::getClosestPointEstimate(const ON_3dPoint& pt, ON_Interval& u, ON_Interval& v) {
	    if (this->m_children.size() > 0) {
		BVNode* closestNode = this->m_children[0];
		for (int i = 1; i < this->m_children.size(); i++) {
		    closestNode = closer(pt, closestNode, this->m_children[i]);
		}
		return closestNode->getClosestPointEstimate(pt, u, v);
	    } else {
	 	double uvs[5][2] = {{m_u.Min(), m_v.Min()}, {m_u.Max(), m_v.Min()}, {m_u.Max(), m_v.Max()}, 
		    {m_u.Min(), m_v.Max()}, {m_u.Mid(), m_v.Mid()}}; // include corners and the estimate
	        ON_3dPoint corners[5];
		const ON_Surface* surf = m_face->SurfaceOf();
		u = m_u;
		v = m_v;
		// XXX - pass these in from SurfaceTree::surfaceBBox() to avoid this recalculation?
		if (!surf->EvPoint(uvs[0][0], uvs[0][1], corners[0]) || !surf->EvPoint(uvs[1][0], uvs[1][1], corners[1]) ||
		    !surf->EvPoint(uvs[2][0], uvs[2][1], corners[2]) || !surf->EvPoint(uvs[3][0], uvs[3][1], corners[3])) {
		    throw new exception(); // XXX - fix this
		}
		corners[4] = this->m_estimate;
		// find the point on the surface closest to pt
	        int mini = 0;
		double mindist = pt.DistanceTo(corners[mini]);
		double tmpdist;
		for (int i = 1; i < 5; i++) {
		    tmpdist = pt.DistanceTo(corners[i]);
		    if (tmpdist < mindist) {
			mini = i;
			mindist = tmpdist;
		    }
		}
		return ON_2dPoint(uvs[mini][0], uvs[mini][1]);
	    }
	}
    
    template<class BH>
      inline bool BVNode<BH>::doTrimming() const {
	return m_checkTrim;
      }
 
    template<class BH>
      void BVNode<BH>::getTrimsAbove(const ON_2dPoint& uv, list<BRNode*>& out_leaves) {
	point_t bmin,bmax;
	double dist;
	for (list<BRNode*>::iterator i = m_trims_above.begin(); i != m_trims_above.end(); i++) {
	  BRNode* br = dynamic_cast<BRNode*>(*i);
	  br->GetBBox(bmin,bmax);
	  dist = 0.000001; //0.03*DIST_PT_PT(bmin,bmax);
	  if ((uv[X] > bmin[X]-dist) && (uv[X] < bmax[X]+dist))
	    out_leaves.push_back(br);
	}
      }
  
    template<class BH>
      inline bool BVNode<BH>::isTrimmed(const ON_2dPoint& uv) {
	BRNode* br;
	list<BRNode*> trims;
	point_t bmin,bmax;
	double trimdist;
	
	if (m_checkTrim) {       
	  getTrimsAbove(uv,trims);
	  if (trims.empty()) {
	    return true;
	  } else {//find closest BB
	    list<BRNode*>::iterator i;
	    BRNode* closest = NULL;
	    fastf_t currHeight; //=999999.9;
	    point_t min,max;
	    for( i=trims.begin();i!=trims.end();i++) {
	      br = dynamic_cast<BRNode*>(*i);
	      fastf_t v = br->getLinearEstimateOfV(uv[X]); // - uv[Y];
	      v = br->getCurveEstimateOfV(uv[X],0.0000001);
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
	    }
	    if (closest == NULL) {
	      return true;
	    } else {
	      return closest->isTrimmed(uv, trimdist);
	    }
	  }
	} else {
	  return m_trimmed;
	}
      }


    template<class BH>
      inline int BVNode<BH>::NodeTrimmed() {
    	  point_t curvemin, curvemax, smin, smax;
  	  fastf_t dist = 0.00001;
  	  list<BRNode*>::iterator i, j;
  	  if (m_trims_above.empty()) { // Nothing above - node is trimmed
  	      m_trimmed = true;
  	      m_checkTrim = false;
  	      return 1;
  	  } else {
  	      i = m_trims_above.begin();
  	      j = i;
  	      (*i)->GetBBox(curvemin, curvemax);
  	      if (curvemin[Y] - dist < m_v[1]) { // Trim curve in node - must check
                m_checkTrim = true;
                return 2;
            }
            i++;
            if (i == m_trims_above.end()) { // Only one trim present above - base decision on m_XIncreasing
                if ((*j)->m_XIncreasing) {
                    m_trimmed = true;
                    m_checkTrim = false;
                    return 1;
                } else {
                    m_trimmed = false;
                    m_checkTrim = false;
                    return 0;
                }
            } else {
                // Multiple trimming curves in list.  Need to check for overlapping bounding boxes between the
                // first two entries in the sorted list - if they do overlap, can't determine node trimming
                // status and need to check trim.  Otherwise, make determination as above.
                (*i)->GetBBox(smin, smax);
                if ((smin[Y] >= curvemax[Y]) || (smin[X] >= curvemax[X]) || (smax[X] <= curvemin[X])) {
                    if ((*j)->m_XIncreasing) {
                        m_trimmed = true;
                        m_checkTrim = false;
                        return 1;
                    } else {
                        m_trimmed = false;
                        m_checkTrim = false;
                        return 0;
                    }
                } else {
                    m_checkTrim = true;
                    return 2;
		}
	    }
	  }
      }
            

    
   /**********************************************************************
   *                        Surface Tree Class
   *
   *  The surface tree is the top level construct that holds all the
   *  Bounding Volume Nodes for a BREP Face.
   *
   **********************************************************************/

  class SurfaceTree {
    public:
      SurfaceTree(const ON_BrepFace* face, CurveTree* ctree);
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
      void GetBVChildren(BBNode* parent, int depth);
      void BuildBBox(BBNode* currentnode);
      const ON_BrepFace* m_face;
      CurveTree* m_ctree;
      BBNode* m_root;
  };
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

