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
    int handleSurfaceOfRevolution(int line, int curve, double start, double end);
    int handleTabulatedCylinder();
    int handleRationalBSplineSurface(int num_control[2], 
				     int degree[2], 
				     bool u_closed, 
				     bool v_closed, 
				     bool rational, 
				     bool u_periodic, 
				     bool v_periodic, 
				     int u_num_knots,
				     int v_num_knots,
				     double u_knots[],
				     double v_knots[],
				     double weights[],
				     double* ctl_points);
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
    int handleLine(point_t start, point_t end);
    int handleParametricSplineCurve();
    int handleRationalBSplineCurve();
    int handleOffsetCurve();    

  private:
  };

}

#endif
