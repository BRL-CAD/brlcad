
#include "brlcad.hpp"

#define PT(p) p[X] << "," << p[Y] << "," << p[Z]

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
  BRLCADBrepHandler::handleEdge(int curve, int initVert, int termVert) {    
    debug("handleEdge");
    debug("curve: " << curve);
    debug("init : " << initVert);
    debug("term : " << termVert);
    return 0;
  }

  int
  BRLCADBrepHandler::handleEdgeUse(int edge, bool orientWithCurve) {
    debug("handleEdgeUse");
    debug("edge  : " << edge);
    debug("orient: " << orientWithCurve);
    return 0;
  }

  int
  BRLCADBrepHandler::handleVertex(point_t pt) {
    debug("handleVertex");
    debug("point: " << PT(pt));
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
  BRLCADBrepHandler::handleSurfaceOfRevolution(int lineIndex, int curveIndex, double startAngle, double endAngle) { 
    debug("handleSurfaceOfRevolution");
    debug("line  : " << lineIndex);
    debug("curve : " << curveIndex);
    debug("start : " << startAngle);
    debug("end   : " << endAngle);
    return 0; 
  }

  int
  BRLCADBrepHandler::handleTabulatedCylinder() { return 0; }

  int
  BRLCADBrepHandler::handleRationalBSplineSurface(int num_control[2], 
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
						  double* ctl_points) { 
    debug("handleRationalBSplineSurface()");
    debug("u controls: " << num_control[0]);
    debug("v controls: " << num_control[1]);
    debug("u degree  : " << degree[0]);
    debug("v degree  : " << degree[1]);
    return 0; 
  }

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
  BRLCADBrepHandler::handleCircularArc(double radius,
				       point_t center,
				       point_t start,
				       point_t end) { 
    debug("handleCircularArc");
    debug("radius: " << radius);
    debug("center: " << PT(center));
    return 0; 
  }

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
  BRLCADBrepHandler::handleLine(point_t start, point_t end) {
    debug("handleLine");
    debug("start: " << start[0] << "," << start[1] << "," << start[2]);
    debug("end  : " << end[0] << "," << end[1] << "," << end[2]);    
    return 0; 
  }

  int
  BRLCADBrepHandler::handleParametricSplineCurve() { return 0; }

  int 
  BRLCADBrepHandler::handleRationalBSplineCurve(int degree,
						double tmin,
						double tmax,
						bool planar,
						vect_t unit_normal,
						bool closed,
						bool rational,
						bool periodic,
						int num_knots,
						double* knots,
						int num_control_points,
						double* weights,
						double* ctl_points) {
    debug("handleRationalBSplineCurve");
    debug("degree: " << degree);
    debug("domain: " << tmin << " --> " << tmax);
    debug("planar: " << planar);
    debug("# ctls: " << num_control_points);
    return 0;
  }

  int
  BRLCADBrepHandler::handleOffsetCurve() { return 0; }    

}
