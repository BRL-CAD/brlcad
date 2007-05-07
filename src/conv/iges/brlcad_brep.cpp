
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


  // surftype should be:
  //   parametric spline surface
  //   ruled surface
  //   surface of revolution
  //   tabulated cylinder
  //   rational b-spline surface
  //   offset surface
  //   plane surface
  //   rccyl surface
  //   rccone surface
  //   spherical surface
  //   toroidal surface
  int BRLCADBrepHandler::handleSurface(IGESEntity surfType, 
				       const ParameterData& data) {
    return 0;
  }

  int 
  BRLCADBrepHandler::handleLoop(bool isOuter, int faceIndex) {
    return 0;
  }

  void 
  BRLCADBrepHandler::handleEdge(int edgeIndex) {
    
  }

  int 
  BRLCADBrepHandler::handleCurve() {
    return 0;
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

}
