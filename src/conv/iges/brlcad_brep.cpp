
#include "brlcad.hpp"
#include "bn.h"

#define PT(p) p[X] << "," << p[Y] << "," << p[Z]

namespace brlcad {

  BRLCADBrepHandler::BRLCADBrepHandler() {
  }

  BRLCADBrepHandler::~BRLCADBrepHandler() {
  }

  int 
  BRLCADBrepHandler::handleShell(bool isVoid, bool orient) {
    _brep = ON_Brep::New();
    _objects.push_back(_brep);
    return _objects.size()-1;
  }

  int 
  BRLCADBrepHandler::handleFace(bool orient, int surfIndex) {    
    _face = &_brep->NewFace(surfIndex);
    _objects.push_back(_face);
    return _objects.size()-1;
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
    _vertex = &_brep->NewVertex(ON_3dPoint(pt));
    _vertex->m_tolerance = 0.0; // XXX use exact tolerance?
    _objects.push_back(_vertex);
    return _objects.size()-1;
  }

  int 
  BRLCADBrepHandler::handlePoint(double x, double y, double z) {
    // XXX may be deprecated
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

    ON_Plane plane = ON_Plane(ON_3dPoint(center), ON_3dPoint(start), ON_3dPoint(end));
    ON_Circle circle = ON_Circle(plane, ON_3dPoint(center), radius);
    double a, b;
    circle.ClosestPointTo(start, &a);
    circle.ClosestPointTo(end, &b);
    ON_Arc arc(circle, ON_Interval(a, b));    
    ON_ArcCurve* curve = new ON_ArcCurve(arc);
    _objects.push_back(curve);

    return _objects.size()-1; 
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
    debug("start: " << PT(start));
    debug("end  : " << PT(end));
    
    ON_LineCurve* line = new ON_LineCurve(ON_3dPoint(start),ON_3dPoint(end));
    _objects.push_back(line);
    return _objects.size()-1;
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

    ON_NurbsCurve* c = ON_NurbsCurve::New(3,
					  rational,
					  degree+1,
					  num_control_points);
    c->ReserveKnotCapacity(num_knots);
    for (int i = 0; i < num_knots; i++) {
      c->m_knot[i] = knots[i];
    }
    
    int stride = c->m_cv_stride;
    for (int i = 0; i < num_control_points; i++) {
      c->m_cv[i*stride] = ctl_points[i*stride];
      c->m_cv[i*stride+1] = ctl_points[i*stride+1];
      c->m_cv[i*stride+2] = ctl_points[i*stride+2];
      if (rational) c->m_cv[i*stride+3] = weights[i];
    }

    _objects.push_back(c);
    return _objects.size()-1;
  }

  int
  BRLCADBrepHandler::handleOffsetCurve() { return 0; }    

}
