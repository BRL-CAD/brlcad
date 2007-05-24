#include "brlcad_surfacetree.h"

namespace brlcad {
  
  SurfaceTree::SurfaceTree(ON_Surface* surf) {

  }
  
  SurfaceTree::~SurfaceTree() {

  }

  BBNode* 
  SurfaceTree::getRootNode() {
    return m_root;
  }

  ON_2dPoint
  SurfaceTree::getClosestPointEstimate(const ON_3dPoint& pt) {
    return ON_2dPoint(0,0);
  }
}
