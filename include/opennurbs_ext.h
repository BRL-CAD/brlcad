#ifndef __OPENNURBS_EXT
#define __OPENNURBS_EXT

/**-------------------------------------------------------------------------------
 // brep/surface utilities
 //
 // XXX: these should probably be migrated to openNURBS package proper
 // but there are a lot of dependency issues (e.g. where do the math
 // routines go?)
 */

#include "opennurbs.h"
#include <vector>
#include <list>
#include <limits>
#include <iostream>

/* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
/* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.8
/* Max newton iterations when finding closest point */
#define BREP_MAX_FCP_ITERATIONS 10
/* Root finding epsilon */
#define BREP_FCP_ROOT_EPSILON 0.00001

static std::numeric_limits<double> real;

#define PT(p) "(" << (p)[0] << "," << (p)[1] << "," << (p)[2] << ")"
#define PT2(p) "(" << (p)[0] << "," << (p)[1] << ")"
#define TRACE(s) std::cout << s << endl;

namespace brlcad {
  
  //--------------------------------------------------------------------------------
  // Bounding volume hierarchy classes
  class ON_Ray {
  public:
    ON_3dPoint m_origin;
    ON_3dVector m_dir;

    ON_Ray(ON_3dPoint& origin, ON_3dVector& dir) : m_origin(origin), m_dir(dir) {}
  };  

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
    typedef BVSegment<BV> segment;
    typedef list<BVSegment<BV> > IsectList;

    ChildList m_children;
    BV m_node;
    ON_3dPoint m_estimate;

    BVNode();
    BVNode(const BV& node);
    ~BVNode();

    void addChild(const BV& child);
    void addChild(BVNode<BV>* child);
    void removeChild(const BV& child);
    void removeChild(BVNode<BV>* child);
    virtual bool isLeaf() const;

    virtual int depth();
    virtual void getLeaves(list<BVNode<BV>*> out_leaves);
    virtual ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    void GetBBox(double* min, double* max);

    virtual bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    virtual bool intersectsHierarchy(ON_Ray& ray, std::list<segment>* results = 0);
    
  private:
    BVNode<BV>* closer(const ON_3dPoint& pt, BVNode<BV>* left, BVNode<BV>* right);
  };

  template<class BV>
  class SubsurfaceBVNode : public BVNode<BV> {
  public:
    SubsurfaceBVNode(const BV& node, 
		     const ON_BrepFace* face, 
		     const ON_Interval& u, 
		     const ON_Interval& v,
		     bool checkTrim = true,
		     bool trimmed = false);

    bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    bool isLeaf() const;
    bool doTrimming() const;
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    
    const ON_BrepFace* m_face;
    ON_Interval m_u;
    ON_Interval m_v;
    bool m_checkTrim;
    bool m_trimmed;
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
  BVNode<BV>::BVNode(const BV& node) : m_node(node) { }
  
  template<class BV>
  inline
  BVNode<BV>::~BVNode() {
    // delete the children
    for (int i = 0; i < m_children.size(); i++) {
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
    double tnear = real.min();
    double tfar = real.max();
    for (int i = 0; i < 3; i++) {
      if (ON_NearZero(ray.m_dir[i])) {
    	if (ray.m_origin[i] < m_node.m_min[i] || ray.m_origin[i] > m_node.m_max[i])
    	  return false;
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
  BVNode<BV>::intersectsHierarchy(ON_Ray& ray, std::list<BVNode<BV>::segment>* results_opt) {
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
      if (results_opt != 0) results_opt->push_back(segment(this, tnear, tfar));
    } else if (intersects) {
      // XXX: bug in g++? had to typedef the below to get it to work!
      //       for (std::list<BVNode<BV>*>::iterator j = m_children.begin(); j != m_children.end(); j++) {
      // 	(*j)->intersectsHierarchy(ray, results_opt);
      //       }
      for (int i = 0; i < m_children.size(); i++) {	
	m_children[i]->intersectsHierarchy(ray, results_opt);
      }
    }

    return intersects;
  }

  template<class BV>
  int 
  BVNode<BV>::depth() {
    int d = 0;
    for (int i = 0; i < m_children.size(); i++) {
      d = 1 + max(d, m_children[i]->depth());
    }
    return d;
  }

  template<class BV>
  void 
  BVNode<BV>::getLeaves(list<BVNode<BV>*> out_leaves) {
    if (m_children.size() > 0) {
      for (int i = 0; i < m_children.size(); i++) {
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
    TRACE("getClosestPointEstimate(" << PT(pt) << ")");
    if (m_children.size() > 0) {
      BBNode* closestNode = m_children[0];
      for (int i = 1; i < m_children.size(); i++) {
	closestNode = closer(pt, closestNode, m_children[i]);
	TRACE("\t" << PT(closestNode->m_estimate));
      }
      return closestNode->getClosestPointEstimate(pt);
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
    : BVNode<BV>(node), m_face(face), m_u(u), m_v(v), m_checkTrim(checkTrim), m_trimmed(trimmed)
  {
  }
  
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
  SubsurfaceBVNode<BV>::doTrimming() const {
    return m_checkTrim;
  }

  template<class BV>
  ON_2dPoint
  SubsurfaceBVNode<BV>::getClosestPointEstimate(const ON_3dPoint& pt) {
    double uvs[5][2] = {{m_u.Min(),m_v.Min()},  // include the corners for an easy refinement
			{m_u.Max(),m_v.Min()},
			{m_u.Max(),m_v.Max()},
			{m_u.Min(),m_v.Max()},
			{m_u.Mid(),m_v.Mid()}}; // include the estimate
    ON_3dPoint corners[5];
    const ON_Surface* surf = m_face->SurfaceOf();
    
    // XXX - pass these in from SurfaceTree::surfaceBBox() to avoid this recalculation?
    if (!surf->EvPoint(uvs[0][0],uvs[0][1],corners[0]) ||
	!surf->EvPoint(uvs[1][0],uvs[1][1],corners[1]) ||
	!surf->EvPoint(uvs[2][0],uvs[2][1],corners[2]) ||
	!surf->EvPoint(uvs[3][0],uvs[3][1],corners[3])) {
      throw new exception(); // XXX - fix this
    }
    corners[4] = BVNode<BV>::m_estimate;
			
    // find the point on the surface closest to pt
    int mini = 0;
    double mindist = pt.DistanceTo(corners[mini]);
    double tmpdist;
    for (int i = 1; i < 5; i++) {
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
    /**
     * Return just the leaves of the surface tree
     */
    void getLeaves(list<BBNode*> out_leaves);
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
   * - find a point (u,v) for which S(u,v) is closest to _point_
   *                                                     _      __
   *   -- minimize the distance function: D(u,v) = sqrt(|S(u,v)-pt|^2)
   *                                       _      __
   *   -- simplify by minimizing f(u,v) = |S(u,v)-pt|^2
   *   
   *   -- minimum occurs when the gradient is zero, i.e.
   *     \f[ \nabla f(u,v) = |\vec{S}(u,v)-\vec{p}|^2 = 0 \f]
   */
  bool  
  get_closest_point(ON_2dPoint& outpt,
		    ON_BrepFace* face,
		    const ON_3dPoint& point,
		    SurfaceTree* tree = NULL,
		    double tolerance = -1.0);

    
  /**                  p u l l b a c k _ c u r v e
   *
   * Pull an arbitrary model-space *curve* onto the given *surface* as a
   * curve within the surface's domain when, for each point c = C(t) on
   * the curve and the closest point s = S(u,v) on the surface, we have:
   * distance(c,s) <= *tolerance*.
   *
   * The resulting 2-dimensional curve will be approximated using the
   * following process:
   *
   * 1. Adaptively sample the 3d curve in the domain of the surface
   * (ensure tolerance constraint). Sampling terminates when the
   * following flatness criterion is met:
   *    given two parameters on the curve t1 and t2 (which map to points p1 and p2 on the curve)
   *    let m be a parameter randomly chosen near the middle of the interval [t1,t2]
   *                                                              ____
   *    then the curve between t1 and t2 is flat if distance(C(m),p1p2) < flatness
   *
   * 2. Use the sampled points to perform a global interpolation using 
   *    universal knot generation to build a B-Spline curve.
   *
   * 3. If the curve is a line or an arc (determined with openNURBS routines), 
   *    return the appropriate ON_Curve subclass (otherwise, return an ON_NurbsCurve).
   *
   * 
   */
  extern ON_Curve*
  pullback_curve(ON_BrepFace* face, 
		 const ON_Curve* curve, 
		 SurfaceTree* tree = NULL,
		 double tolerance = 1.0e-6, 
		 double flatness = 1.0e-3);
  


}


#endif
