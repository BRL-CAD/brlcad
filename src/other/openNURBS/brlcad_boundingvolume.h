#ifndef __BRLCAD_BOUNDINGVOLUME
#define __BRLCAD_BOUNDINGVOLUME


#include "opennurbs.h"
#include <list>

//--------------------------------------------------------------------------------
// Bounding volume hierarchy classes
namespace brlcad {
  
  template<class BV>
  class BVNode {
  public:
    BV* m_node;
    std::list<BVNode<BV>*> m_children;
    
    BVNode();
    BVNode(BV* node);
    ~BVNode();

    void addChild(BV* child);
    void removeChild(BV* child);
    virtual bool isLeaf();
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

    bool isLeaf();
    
    ON_Interval m_u;
    ON_Interval m_v;
    bool m_checkTrim;
    bool m_trimmed;
  };

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
  inline SubsurfaceBVNode<BV>::SubsurfaceBVNode(BV* node, 
				     const ON_BrepFace& face, 
				     const ON_Interval& u, 
				     const ON_Interval& v,
				     bool checkTrim,
				     bool trimmed) {
  }
  
  template<class BV>
  inline bool 
  SubsurfaceBVNode<BV>::isLeaf() {
    return false;
  }
  

//   class BoundingVolume {	
//   public:

//     BoundingVolume(const ON_3dPoint& min, const ON_3dPoint& max);
//     BoundingVolume(const ON_3dPoint& mina, const ON_3dPoint& maxa, const ON_3dPoint& minb, const ON_3dPoint& maxb);
//     BoundingVolume(const BoundingVolume& bv);
//     virtual ~BoundingVolume();

//     BoundingVolume& operator=(const BoundingVolume& bv);

//     virtual bool is_leaf() const;

//     float64_t width() const;
//     float64_t height() const;
//     float64_t depth() const;
//     void get_bbox(ON_3dPoint& min, ON_3dPoint& max) const;

//     float64_t surface_area() const;
//     float64_t combined_surface_area(const BoundingVolume& vol) const;
//     BoundingVolume combine(const BoundingVolume& vol) const;

//     // slab intersection routine
//     bool intersected_by(const ON_3dPoint& origin, const ON_3dVector& dir);
//     bool intersected_by(const ON_3dPoint& origin, const ON_3dVector& dir, float64_t* tnear, float64_t* tfar);

//     // Goldsmith & Salmon "Automatic generation of trees"
//     BoundingVolume* gs_insert(BoundingVolume* node);
	
//     std::list<BoundingVolume*> children;

//   private:
//     ON_3dPoint _min;
//     ON_3dPoint _max;
//     float64_t _area;
//   };

//   class SurfaceBV : public BoundingVolume {
//   public:

//     SurfaceBV(const ON_BrepFace& face, 
// 	      const ON_3dPoint& min, const ON_3dPoint& max, 
// 	      const ON_Interval& u, 
// 	      const ON_Interval& v, 
// 	      bool checkTrim, 
// 	      bool trimmed);
//     bool is_leaf() const; // override BoundingVolume::is_leaf();
//     const ON_BrepFace& face() const { return _face; }
//     const float64_t u_center() const { return _u.Mid(); }
//     const float64_t v_center() const { return _v.Mid(); }
//     const bool doTrimming() const { return _doTrim; }
//     const bool isTrimmed() const { return _isTrimmed; }

//   private:
//     const ON_BrepFace& _face;
//     ON_Interval _u;
//     ON_Interval _v;
//     bool _doTrim;
//     bool _isTrimmed;
//   }; 
  
//   typedef std::list<BoundingVolume*> BVList;
}

#endif
