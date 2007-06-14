#ifndef __BRLCAD_HPP
#define __BRLCAD_HPP


#include "common.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "n_iges.hpp"
#include "brep.h"


namespace brlcad {

  class BRLCADBrepHandler : public BrepHandler {
  public:
    BRLCADBrepHandler();
    ~BRLCADBrepHandler();
    
    int handleShell(bool isVoid, bool orient);
    int handleFace(bool orient, int surfIndex);
    int handleLoop(bool isOuter, int faceIndex);
    int handleEdge(int curve, int initVertex, int endVertex);
    int handleEdgeUse(int edge, bool orientWithCurve);
    int handleVertex(point_t pt);
    int handlePoint(double x, double y, double z);


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

    int handleCircularArc(double radius, point_t center, vect_t normal, point_t start, point_t end);
    int handleCompositeCurve();
    int handleConicArc();
    int handle2DPath();
    int handle3DPath();
    int handleSimpleClosedPlanarCurve();
    int handleLine(point_t start, point_t end);
    int handleParametricSplineCurve();
    int handleRationalBSplineCurve(int degree,
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
				   double* ctl_points);

    int handleOffsetCurve();    

    void write();

  private:
    struct rt_wdb* outfp;
    string id_name;
    string geom_name;    

    vector<ON_Geometry*> _objects;    
    vector<int> _topology;

    ON_BrepFace&   face() { return _brep->m_F[_face]; }
    ON_BrepLoop&   loop() { return _brep->m_L[_loop]; }
    ON_BrepEdge&   edge() { return _brep->m_E[_edge]; }
    ON_BrepEdge&   edge(int i) { return _brep->m_E[_topology[i]]; }
    ON_BrepVertex& vertex() { return _brep->m_V[_vertex]; }
    ON_BrepVertex& vertex(int i) { return _brep->m_V[_topology[i]]; }

    // need to support outer and void shells!
    ON_Brep* _brep;
    int _face;
    int _loop;
    int _edge;
    int _trim;
    int _vertex;
    ON_Point* _pt;
  };

}

#endif
