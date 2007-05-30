#ifndef __OPENNURBS_EXT
#define __OPENNURBS_EXT

//--------------------------------------------------------------------------------
// brep/surface utilities
//
// XXX: these should probably be migrated to openNURBS package proper
// but there are a lot of dependency issues (e.g. where do the math
// routines go?)

#include "opennurbs.h"
#include <vector>
#include <list>
#include <limits>

/* Maximum per-surface BVH depth */
#define BREP_MAX_FT_DEPTH 8
/* Surface flatness parameter, Abert says between 0.8-0.9 */
#define BREP_SURFACE_FLATNESS 0.8

static std::numeric_limits<double> real;

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
    BV m_node;

    typedef vector<BVNode<BV>*> ChildList;
    ChildList m_children;
    
    typedef BVSegment<BV> segment;
    typedef list<BVSegment<BV> > IsectList;

    BVNode();
    BVNode(const BV& node);
    ~BVNode();

    void addChild(const BV& child);
    void addChild(BVNode<BV>* child);
    void removeChild(const BV& child);
    void removeChild(BVNode<BV>* child);
    virtual bool isLeaf() const;

    void GetBBox(double* min, double* max);

    virtual bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    virtual bool intersectsHierarchy(ON_Ray& ray, std::list<BVNode<BV>::segment>* results = 0);
  };

  template<class BV>
  class SubsurfaceBVNode : public BVNode<BV> {
  public:
    SubsurfaceBVNode(const BV& node, 
		     const ON_BrepFace& face, 
		     const ON_Interval& u, 
		     const ON_Interval& v,
		     bool checkTrim = true,
		     bool trimmed = false);

    bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    bool isLeaf() const;
    bool doTrimming() const;
    
    const ON_BrepFace& m_face;
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
  }

  template<class BV>
  inline SubsurfaceBVNode<BV>::SubsurfaceBVNode(const BV& node, 
						const ON_BrepFace& face, 
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

  /*                  p u l l b a c k _ c u r v e
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
  pullback_curve(const ON_Surface* surface, 
		 const ON_Curve* curve, 
		 double tolerance = 1.0e-6, 
		 double flatness = 1.0e-3);
  

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
        
  private:
    bool isFlat(const ON_Surface* surf, const ON_Interval& u, const ON_Interval& v);
    BBNode* subdivideSurface(const ON_BrepFace& face, const ON_Interval& u, const ON_Interval& v, int depth);
    BBNode* surfaceBBox(bool leaf, const ON_BrepFace& face, const ON_Interval& u, const ON_Interval& v);

    ON_BrepFace* m_face;
    BBNode* m_root;
  };

}


#endif
