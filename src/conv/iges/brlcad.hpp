#ifndef __BRLCAD_HPP
#define __BRLCAD_HPP

#include "n_iges.hpp"

namespace brlcad {

  class BRLCADBrepHandler : public BrepHandler {
  public:
    BRLCADBrepHandler();
    ~BRLCADBrepHandler();
    
    void handleShell(bool isVoid, bool orient);
    int handleFace(bool orient, int surfIndex);
    int handleLoop(bool isOuter, int faceIndex);
    int handleEdge(int edgeIndex);
    int handleVertex(int pointIndex);
    int handlePoint(double x, double y, double z); // return index


    int handleParametricSplineSurface();
    int handleRuledSurface();
    int handleSurfaceOfRevolution();
    int handleTabulatedCylinder();
    int handleRationalBSplineSurface();
    int handleOffsetSurface();
    int handlePlaneSurface();
    int handleRightCircularCylindricalSurface();
    int handleRightCircularConicalSurface();
    int handleSphericalSurface();
    int handleToroidalSurface();    

    int handleCircularArc();
    int handleCompositeCurve();
    int handleConicArc();
    int handle2DPath();
    int handle3DPath();
    int handleSimpleClosedPlanarCurve();
    int handleLine();
    int handleParametricSplineCurve();
    int handleRationalBSplineCurve();
    int handleOffsetCurve();    

  private:
  };

}

#endif
