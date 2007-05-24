#ifndef __BRLCAD_BOUNDINGVOLUME
#define __BRLCAD_BOUNDINGVOLUME


#include "opennurbs.h"
#include <list>
#include <limits>

static std::numeric_limits<double> real;

//--------------------------------------------------------------------------------
// Bounding volume hierarchy classes
namespace brlcad {
  
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
  };

  template<class BV>
  class BVNode {
  public:
    BV* m_node;
    std::list<BVNode<BV>*> m_children;
    
    typedef std::list<BVNode<BV>*> childIterator;
    typedef BVSegment<BV> segment;

    BVNode();
    BVNode(BV* node);
    ~BVNode();

    void addChild(BV* child);
    void removeChild(BV* child);
    virtual bool isLeaf();

    virtual bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    bool intersectsHierarchy(ON_Ray& ray, std::list<BVNode<BV>::segment>* results = 0);
  };

  template<class BV>
  class SubsurfaceBVNode : public BVNode<BV> {
  public:
    SubsurfaceBVNode(BV* node, 
		     const ON_BrepFace& face, 
		     const ON_Interval& u, 
		     const ON_Interval& v,
		     bool checkTrim,
		     bool trimmed);

    bool intersectedBy(ON_Ray& ray, double* tnear = 0, double* tfar = 0);
    bool isLeaf();
    
    ON_BrepFace& m_face;
    ON_Interval m_u;
    ON_Interval m_v;
    bool m_checkTrim;
    bool m_trimmed;
  };  
  
  typedef BVNode<ON_BoundingBox> BBNode;

  //--------------------------------------------------------------------------------
  // Template Implementation
  template<class BV>
  inline 
  BVNode<BV>::BVNode() { }
  
  template<class BV>
  inline
  BVNode<BV>::BVNode(BV* node) : m_node(node) { }
  
  template<class BV>
  inline
  BVNode<BV>::~BVNode() {}

  template<class BV>
  inline void 
  BVNode<BV>::addChild(BV* child) {}

  template<class BV>
  inline void 
  BVNode<BV>::removeChild(BV* child) {}

  template<class BV>
  inline bool 
  BVNode<BV>::isLeaf() { return false; }
  
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
  inline bool
  BVNode<BV>::intersectsHierarchy(ON_Ray& ray, std::list<BVNode<BV>::segment>* results_opt) {
    double tnear, tfar;
    bool intersects = intersectedBy(ray, &tnear, &tfar);
    if (intersects && isLeaf()) {
      if (results_opt != 0) results_opt->push_back(segment(m_node, tnear, tfar));
    } else if (intersects) {
      // XXX: bug in g++? had to typedef the below to get it to work!
      //       for (std::list<BVNode<BV>*>::iterator j = m_children.begin(); j != m_children.end(); j++) {
      // 	(*j)->intersectsHierarchy(ray, results_opt);
      //       }
      for (childIterator i = m_children.begin(); i != m_children.end(); i++) {
	i->intersectsHierarchy(ray, results_opt);
      }
    }
  }

  template<class BV>
  inline SubsurfaceBVNode<BV>::SubsurfaceBVNode(BV* node, 
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
  SubsurfaceBVNode<BV>::isLeaf() {
    return true;
  }

}

#endif
