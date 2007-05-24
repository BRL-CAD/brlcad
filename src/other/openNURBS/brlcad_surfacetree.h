#ifndef __BRLCAD_SURFACETREE
#define __BRLCAD_SURFACETREE

#include "opennurbs.h"
#include "brlcad_boundingvolume.h"

namespace brlcad {
  
  class SurfaceTree {
  public:
    SurfaceTree(ON_Surface* surf);
    ~SurfaceTree();

    BBNode* getRootNode();    
    /** 
     * Calculate, using the surface bounding volume hierarchy, a uv
     * estimate for the closest point on the surface to the point in
     * 3-space.
    */
    ON_2dPoint getClosestPointEstimate(const ON_3dPoint& pt);
    
  private:
    ON_Surface* m_surf;
    BBNode* m_root;
  };

}

#endif
