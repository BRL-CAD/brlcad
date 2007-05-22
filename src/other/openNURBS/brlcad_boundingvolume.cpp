#include "brlcad_boundingvolume.h"

namespace brlcad {


  //--------------------------------------------------------------------------------
  // implementation
  
  void swap(double& a, double& b) {
    double t = b;
    b = a;
    a = t;
  }      

//   inline float64_t
//   BoundingVolume::width() const { return _max[X]-_min[X]; }	
//   inline float64_t
//   BoundingVolume::height() const { return _max[Y]-_min[Y]; }
//   inline float64_t
//   BoundingVolume::depth() const { return _max[Z]-_min[Z]; }

//   inline void 
//   BoundingVolume::get_bbox(ON_3dPoint& min, ON_3dPoint& max) const {
//     VMOVE(min, _min);
//     VMOVE(max, _max);
//   }
  
//   inline bool 
//   BoundingVolume::intersected_by(ON_3dPoint& origin, const ON_3dVector& dir) {
//     float64_t tnear, tfar;
//     return intersected_by(origin, dir, &tnear, &tfar);
//   }

//   inline bool 
//   BoundingVolume::intersected_by(ON_3dPoint& origin, const ON_3dVector& dir, float64_t* tnear, float64_t* tfar) {
//     *tnear = -MAX_FASTF;
//     *tfar = MAX_FASTF;
//     for (int i = 0; i < 3; i++) {
//       if (NEAR_ZERO(dir[i],VUNITIZE_TOL)) {
// 	if (origin[i] < _min[i] || origin[i] > _max[i])
// 	  return false;
//       }
//       else {
// 	float64_t t1 = (_min[i]-origin[i]) / dir[i];
// 	float64_t t2 = (_max[i]-origin[i]) / dir[i];
// 	if (t1 > t2) swap(t1,t2);
// 	if (t1 > *tnear) *tnear = t1;
// 	if (t2 < *tfar) *tfar = t2;
// 	if (*tnear > *tfar) /* box is missed */ return false;
// 	if (*tfar < 0) /* box is behind ray */ return false;
//       }
//     }
//     return true;
//   }
  
//   inline float64_t
//   BoundingVolume::surface_area() const { return _area; }
  
//   inline float64_t
//   BoundingVolume::combined_surface_area(const BoundingVolume& vol) const { 
//     BoundingVolume combined(_min,_max,vol._min,vol._max);
//     return combined.surface_area();
//   }
  
//   inline BoundingVolume
//   BoundingVolume::combine(const BoundingVolume& vol) const {
//     return BoundingVolume(_min,_max,vol._min,vol._max);
//   }
  
//   BoundingVolume*
//   BoundingVolume::gs_insert(BoundingVolume* node)
//   {
//     // XXX todo - later!
//     if (is_leaf()) {
//       // create a new parent 
//     } else {

//     }

//     return this;
//   }

//   inline 
//   SurfaceBV::SurfaceBV(const ON_BrepFace& face, 
// 		       ON_3dPoint& min, 
// 		       ON_3dPoint& max, 
// 		       const ON_Interval& u, 
// 		       const ON_Interval& v,
// 		       bool checkTrim,
// 		       bool isTrimmed) 
//     : BoundingVolume(min,max), _face(face), _u(u), _v(v), _doTrim(checkTrim), _isTrimmed(isTrimmed)
//   {
//   }

//   inline bool
//   SurfaceBV::is_leaf() const { return true; }
//   typedef std::list<BoundingVolume*> BVList;
}
