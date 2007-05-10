
#include "brlcad.hpp"

namespace brlcad {

  BRLCADBrepHandler::BRLCADBrepHandler() {

  }

  BRLCADBrepHandler::~BRLCADBrepHandler() {

  }

  void 
  BRLCADBrepHandler::handleShell(bool isVoid, bool orient) {
    
  }

  int 
  BRLCADBrepHandler::handleFace(bool orient, int surfIndex) {
    return 0;
  }


  int 
  BRLCADBrepHandler::handleLoop(bool isOuter, int faceIndex) {
    return 0;
  }

  int 
  BRLCADBrepHandler::handleEdge(int edgeIndex) {
    
  }

  int
  BRLCADBrepHandler::handleVertex(int pointIndex) {
    return 0;
  }

  int 
  BRLCADBrepHandler::handlePoint(double x, double y, double z) {
    // return index
    return 0;
  }

  int
  BRLCADBrepHandler::handleParametricSplineSurface() { return 0; }

  int
  BRLCADBrepHandler::handleRuledSurface() { return 0; }

  int
  BRLCADBrepHandler::handleSurfaceOfRevolution() { return 0; }

  int
  BRLCADBrepHandler::handleTabulatedCylinder() { return 0; }

  int
  BRLCADBrepHandler::handleRationalBSplineSurface() { return 0; }

  int
  BRLCADBrepHandler::handleOffsetSurface() { return 0; }

  int
  BRLCADBrepHandler::handlePlaneSurface() { return 0; }

  int
  BRLCADBrepHandler::handleRightCircularCylindricalSurface() { return 0; }

  int
  BRLCADBrepHandler::handleRightCircularConicalSurface() { return 0; }

  int
  BRLCADBrepHandler::handleSphericalSurface() { return 0; }

  int
  BRLCADBrepHandler::handleToroidalSurface() { return 0; }    

  int
  BRLCADBrepHandler::handleCircularArc() { return 0; }

  int
  BRLCADBrepHandler::handleCompositeCurve() { return 0; }

  int
  BRLCADBrepHandler::handleConicArc() { return 0; }

  int
  BRLCADBrepHandler::handle2DPath() { return 0; }

  int
  BRLCADBrepHandler::handle3DPath() { return 0; }

  int
  BRLCADBrepHandler::handleSimpleClosedPlanarCurve() { return 0; }

  int
  BRLCADBrepHandler::handleLine() { return 0; }

  int
  BRLCADBrepHandler::handleParametricSplineCurve() { return 0; }

  int
  BRLCADBrepHandler::handleRationalBSplineCurve() { return 0; }

  int
  BRLCADBrepHandler::handleOffsetCurve() { return 0; }    

}
