/*                 B R L C A D _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include <assert.h>

#include "brlcad_brep.hpp"
#include "vmath.h"

/* FIXME: should not be peeking into a private header and cannot be a
 * public header (of librt).
 */
#include "../../librt/opennurbs_ext.h"


namespace brlcad {

BRLCADBrepHandler::BRLCADBrepHandler() {
    id_name = "Test B-Rep from IGES";
    geom_name = "piston";
    ON::Begin();
    _written = false;
}


BRLCADBrepHandler::~BRLCADBrepHandler() {
    if (!_written) delete _brep;
    // can only call delete here if write wasn't called!
    ON::End();
}


void
BRLCADBrepHandler::write(const string& filename) {
    debug("write");
    _written = true;
    outfp = wdb_fopen(filename.c_str());
    mk_id(outfp, id_name.c_str());

    if (!_brep) {
	debug("no brep to write");
	return;
    }

    ON_TextLog tl;
    // ??? join trims? iterate and call CloseTrimGap
    for (int faces = 0; faces < _brep->m_F.Count(); ++faces) {
	ON_BrepFace* face = _brep->Face(faces);
	for (int i = 0; i < face->LoopCount(); i++) {
	    ON_BrepLoop* loop = face->Loop(i);
	    int trimCount = loop->TrimCount();
	    for (int j = 0; j < trimCount; j++) {
		ON_BrepTrim* trimA = loop->Trim(j);
		ON_BrepTrim* trimB = loop->Trim((j+1) % trimCount);
		int curveAIndex = trimA->TrimCurveIndexOf();
		int curveBIndex = trimB->TrimCurveIndexOf();

		ON_Curve* curveA = _brep->m_C2[curveAIndex];
		ON_Curve* curveB = _brep->m_C2[curveBIndex];

		curveB->SetStartPoint(curveA->PointAtEnd());
	    }
	}
    }

    _brep->DeleteFace(_brep->m_F[56], true);
    _brep->DeleteTrim(_brep->m_T[29], true);
    _brep->DeleteTrim(_brep->m_T[30], true);

    string sol = geom_name+".s";
    string reg = geom_name+".r";
    if (_brep_flip) _brep->Flip();
    mk_brep(outfp, sol.c_str(), _brep);
    unsigned char rgb[] = {200, 180, 180};
    mk_region1(outfp, reg.c_str(), sol.c_str(), "plastic", "", rgb);
    wdb_close(outfp);
}


int
BRLCADBrepHandler::handleShell(bool isVoid, bool orient) {
    _brep_flip = !orient;
    _brep = ON_Brep::New();
    _objects.push_back(_brep);
    return _objects.size()-1;
}


int
BRLCADBrepHandler::handleFace(bool orient, int surfIndex) {
    ON_BrepFace& face = _brep->NewFace(_topology[surfIndex]);
    face.m_bRev = !orient;

    _face = face.m_face_index;
    _topology.push_back(_face);
    return _topology.size()-1;
}


int
BRLCADBrepHandler::handleLoop(bool isOuter, int faceIndex) {
    debug("handleLoop");
    ON_BrepLoop::TYPE type = (isOuter) ? ON_BrepLoop::outer : ON_BrepLoop::inner;
    ON_BrepLoop& loop = _brep->NewLoop(type, face());

    _loop = loop.m_loop_index;
    _topology.push_back(_loop);
    return _topology.size()-1;
}


int
BRLCADBrepHandler::handleEdge(int curve, int initVert, int termVert) {
    debug("handleEdge");
    debug("curve: " << curve);
    debug("init : " << initVert);
    debug("term : " << termVert);

    ON_BrepVertex& from = vertex(initVert);
    ON_BrepVertex& to   = vertex(termVert);
    ON_Curve* c = ON_Curve::Cast(_objects[curve]);
    int curveIndex = _brep->AddEdgeCurve(c);
    ON_BrepEdge& edge = _brep->NewEdge(from, to, curveIndex);
    edge.m_tolerance = 1e-3; // FIXME: look into it
    _edge = edge.m_edge_index;

    _topology.push_back(_edge);
    return _topology.size()-1;
}


int
BRLCADBrepHandler::handleEdgeUse(int edgeIndex, bool orientWithCurve) {
    ON_TextLog tl;

    debug("handleEdgeUse: edge  : " << edgeIndex);
    debug("handleEdgeUse: orient: " << orientWithCurve);

    ON_BrepEdge& e = edge(edgeIndex);

    debug("handleEdgeUse: " << e.m_vi[0] << " --> " << e.m_vi[1]);

    // grab the curve for this edge
    const ON_Curve* c = e.EdgeCurveOf();
    if (!c) return _topology.size()-1;

    // grab the surface for the face
    const ON_Surface* s = face().SurfaceOf();
    if (!s) return _topology.size()-1;

    // get a 2d parameter-space curve that lies on the surface for this edge
    // hopefully this works!
    ON_Curve* c2d = pullback_curve(&face(), c);
    if (!orientWithCurve) {
	c2d->Reverse();
    }

    int trimCurve = _brep->m_C2.Count();
    _brep->m_C2.Append(c2d);

    ON_BrepTrim& trim = _brep->NewTrim(e, !orientWithCurve, loop(), trimCurve);
    debug("handleEdgeUse: trim " << (orientWithCurve ? "is not " : "is ") << "reversed");
    trim.m_type = ON_BrepTrim::mated; // closed solids!
    trim.m_tolerance[0] = 1e-3; // FIXME: tolerance?
    trim.m_tolerance[1] = 1e-3;
    ON_Interval PD = trim.ProxyCurveDomain();
    trim.m_iso = s->IsIsoparametric(*c2d, &PD);

    trim.IsValid(&tl);

    _trim = trim.m_trim_index;
    _topology.push_back(_trim);
    return _topology.size()-1;
}


int
BRLCADBrepHandler::handleVertex(point_t pt) {
    debug("handleVertex");
    debug("handleVertex point: " << PT(pt));
    int vi = _brep->m_V.Count();
    ON_BrepVertex& b = _brep->NewVertex(ON_3dPoint(pt));
    b.m_tolerance = 1e-3; // FIXME: use exact tolerance?

    _topology.push_back(b.m_vertex_index);
    return _topology.size()-1;
}


int
BRLCADBrepHandler::handlePoint(double x, double y, double z) {
    // NOTE: may be deprecated
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

    // get the line and curve
    ON_Line& line = ((ON_LineCurve*)_objects[lineIndex])->m_line;
    ON_Curve* curve = (ON_Curve*)_objects[curveIndex];

    ON_RevSurface* rev = ON_RevSurface::New();
    rev->m_curve = curve;
    rev->m_axis = line;
    rev->SetAngleRadians(startAngle, endAngle);

    int sid = _brep->AddSurface(rev);
    assert(sid != -1);
    _topology.push_back(sid);
    return _topology.size()-1;
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

    ON_NurbsSurface* surf = ON_NurbsSurface::New(3, rational, degree[0]+1, degree[1]+1, num_control[0], num_control[1]);

    debug("Num u knots: " << surf->KnotCount(0));
    debug("Num v knots: " << surf->KnotCount(1));
    debug("Mult u knots: " << surf->KnotMultiplicity(0, 0) << ", " << surf->KnotMultiplicity(0, 1));
    debug("Mult v knots: " << surf->KnotMultiplicity(1, 0) << ", " << surf->KnotMultiplicity(1, 1));

    // openNURBS handles the knots differently (than most other APIs
    // I've seen, which is admittedly not many)
    //
    // it implicitly represents multiplicities based on a "base" knot
    // vector. IOW, if you had a degree 3 surface, then you'd have a
    // multiplicity of 3 for the first and last knots if it was
    // clamped... (most I've seen are...) Well, openNURBS let's you
    // specify the SINGLE knot value and then you tell it the
    // multiplicity is 3. IGES on the other hand (and Pro/E as well),
    // represent the multiplicities explicitly, i.e. they have 3 of
    // the same value (which means I need to handle the conversion
    // here!)

//     int u_offset = degree[0];
//     int u_count  = u_num_knots - 2*degree[0];

//     int v_offset = degree[1];
//     int v_count  = v_num_knots - 2*degree[1];

    for (int i = 0; i < (u_num_knots-2); i++) {
	surf->SetKnot(0, i, u_knots[i+1]);
	//surf->SetKnot(0, i, u_knots[i]);
    }
    for (int i = 0; i < (v_num_knots-2); i++) {
	surf->SetKnot(1, i, v_knots[i+1]);
	//surf->SetKnot(1, i, v_knots[i]);
    }

    //surf->ClampEnd(0, 2);
    //surf->ClampEnd(1, 2);

    debug("Num u knots: " << surf->KnotCount(0));
    debug("Num v knots: " << surf->KnotCount(1));
    debug("Mult u knots: " << surf->KnotMultiplicity(0, 0) << ", " << surf->KnotMultiplicity(0, 1));
    debug("Mult v knots: " << surf->KnotMultiplicity(1, 0) << ", " << surf->KnotMultiplicity(1, 1));

    for (int u = 0; u < num_control[0]; u++) {
	for (int v = 0; v < num_control[1]; v++) {
	    int index = v*num_control[0]*3 + u*3;
	    if (rational)
		surf->SetCV(u, v, ON_4dPoint(ctl_points[index+0],
					     ctl_points[index+1],
					     ctl_points[index+2],
					     weights[v*num_control[0]+u]));
	    else {
		surf->SetCV(u, v, ON_3dPoint(ctl_points[index+0],
					     ctl_points[index+1],
					     ctl_points[index+2]));
		double* p = &ctl_points[index];
		debug("ctl: " << PT(p));
	    }
	}
    }

    ON_Interval u = surf->Domain(0);
    ON_Interval v = surf->Domain(1);
    debug("u: [" << u[0] << ", " << u[1] << "]");
    debug("v: [" << v[0] << ", " << v[1] << "]");

    int sid = _brep->AddSurface(surf);
    assert(sid != -1);
    _topology.push_back(sid);
    return _topology.size()-1;
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
				     vect_t normal,
				     point_t start,
				     point_t end) {
    debug("handleCircularArc");
    debug("radius: " << radius);
    debug("center: " << PT(center));
    debug("start : " << PT(start));
    debug("end   : " << PT(end));
    debug("normal: " << PT(normal));

    ON_Plane plane = ON_Plane(ON_3dPoint(center), ON_3dVector(normal));
    ON_Circle circle = ON_Circle(plane, ON_3dPoint(center), radius);
    double a, b;
    circle.ClosestPointTo(start, &a); // !
    circle.ClosestPointTo(end, &b);   // !
    debug("a = " << a);
    debug("b = " << b);
    if (b < a) {
	b = 2*M_PI+b;
    }
    ON_Arc arc(circle, ON_Interval(a, b));

    ON_TextLog tl;
    arc.Dump(tl);
    debug("arc valid: " << arc.IsValid());
    ON_ArcCurve* curve = new ON_ArcCurve(arc, arc.Domain().Min(), arc.Domain().Max());
    curve->IsValid(&tl);

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

    ON_LineCurve* line = new ON_LineCurve(ON_3dPoint(start), ON_3dPoint(end));
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
    //c->ReserveKnotCapacity(num_knots);
    for (int i = 0; i < num_knots-2; i++) {
	c->m_knot[i] = knots[i+1];
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


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
