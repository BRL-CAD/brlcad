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
    int handleSurface(IGESEntity surfType, const ParameterData& data);
    int handleLoop(bool isOuter, int faceIndex);
    void handleEdge(int edgeIndex);
    int handleCurve();
    int handleVertex(int pointIndex);
    int handlePoint(double x, double y, double z); // return index


  private:
  };

}

#endif
