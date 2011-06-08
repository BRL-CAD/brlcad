/*                 B R E P H A N D L E R . C P P
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

#include "n_iges.hpp"

#include <iostream>
#include <assert.h>


namespace brlcad {

BrepHandler::BrepHandler() {}

BrepHandler::~BrepHandler() {}

void
BrepHandler::extract(IGES* iges, const DirectoryEntry* de) {
    _iges = iges;
    extractBrep(de);
}


void
BrepHandler::extractBrep(const DirectoryEntry* de) {
    debug("########################### E X T R A C T   B R E P");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    Pointer shell = params.getPointer(1);
    extractShell(_iges->getDirectoryEntry(shell), false, params.getLogical(2));
    int numVoids = params.getInteger(3);

    if (numVoids <= 0) return;

    int index = 4;
    for (int i = 0; i < numVoids; i++) {
	shell = params.getPointer(index);
	extractShell(_iges->getDirectoryEntry(shell),
		     true,
		     params.getLogical(index+1));
	index += 2;
    }
}


void
BrepHandler::extractShell(const DirectoryEntry* de, bool isVoid, bool orientWithFace) {
    debug("########################### E X T R A C T   S H E L L");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    handleShell(isVoid, orientWithFace);

    int numFaces = params.getInteger(1);
    for (int i = 1; i <= numFaces; i++) {
	Pointer facePtr = params.getPointer(i*2);
	bool orientWithSurface = params.getLogical(i*2+1);
	DirectoryEntry* faceDE = _iges->getDirectoryEntry(facePtr);
	extractFace(faceDE, orientWithSurface);
    }
}


void
BrepHandler::extractFace(const DirectoryEntry* de, bool orientWithSurface) {
    // spec says the surface can be:
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

    debug("########################## E X T R A C T   F A C E");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    Pointer surfaceDE = params.getPointer(1);
    int surf = extractSurface(_iges->getDirectoryEntry(surfaceDE));

    int face = handleFace(orientWithSurface, surf);

    int numLoops = params.getInteger(2);
    bool isOuter = params.getLogical(3) || true; // outer is not set in IGES from Pro/E!
    for (int i = 4; (i-4) < numLoops; i++) {
	Pointer loopDE = params.getPointer(i);
	extractLoop(_iges->getDirectoryEntry(loopDE), isOuter, face);
	isOuter = false;
    }
}


int
BrepHandler::extractLine(const DirectoryEntry* de, const ParameterData& params)
{
    point_t start, end;
    start[X] = params.getReal(1);
    start[Y] = params.getReal(2);
    start[Z] = params.getReal(3);
    end[X] = params.getReal(4);
    end[Y] = params.getReal(5);
    end[Z] = params.getReal(6);

    // probably need to transform this line?

    return handleLine(start, end);
}


int
BrepHandler::extractLine(const Pointer& ptr)
{
    DirectoryEntry* de = _iges->getDirectoryEntry(ptr);
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    return extractLine(de, params);
}


int
BrepHandler::extractSurfaceOfRevolution(const ParameterData& params) {
    Pointer linePtr = params.getPointer(1);
    Pointer curvePtr = params.getPointer(2);
    double startAngle = params.getReal(3);
    double endAngle = params.getReal(4);

    // load the line (axis of revolution)
    int line = extractLine(linePtr);

    // load the curve (generatrix)
    int curve = extractCurve(_iges->getDirectoryEntry(curvePtr), false);

    return handleSurfaceOfRevolution(line, curve, startAngle, endAngle);
}


int
BrepHandler::extractRationalBSplineSurface(const ParameterData& params) {
    // possible to do optimization of form type???
    // see spec
    const int ui = params.getInteger(1);
    const int vi = params.getInteger(2);
    int u_degree = params.getInteger(3);
    int v_degree = params.getInteger(4);
    bool u_closed = params.getInteger(5)() == 1;
    bool v_closed = params.getInteger(6)() == 1;
    bool rational = params.getInteger(7)() == 0;
    bool u_periodic = params.getInteger(8)() == 1;
    bool v_periodic = params.getInteger(9)() == 1;

    const int n1 = 1+ui-u_degree;
    const int n2 = 1+vi-v_degree;

    const int u_num_knots = n1 + 2 * u_degree;
    const int v_num_knots = n2 + 2 * v_degree;
    const int num_weights = (1+ui)*(1+vi);

    // read the u knots
    int i = 10; // first u knot
    double* u_knots = new double[u_num_knots+1];
    for (int _i = 0; _i <= u_num_knots; _i++) {
	u_knots[_i] = params.getReal(i);
	i++;
    }
    i = 11 + u_num_knots; // first v knot
    double* v_knots = new double[v_num_knots+1];
    for (int _i = 0; _i <= v_num_knots; _i++) {
	v_knots[_i] = params.getReal(i);
	i++;
    }

    // read the weights (w)
    i = 11 + u_num_knots + v_num_knots;
    double* weights = new double[num_weights];
    for (int _i = 0; _i < num_weights; _i++) {
	weights[_i] = params.getReal(i);
	i++;
    }

    // read the control points
    i = 12 + u_num_knots + v_num_knots + num_weights;
    double* ctl_points = new double[(ui+1)*(vi+1)*3];
    const int numu = ui+1;
    const int numv = vi+1;
    for (int _v = 0; _v < numv; _v++) {
	for (int _u = 0; _u < numu; _u++) {
	    int index = _v*numu*3 + _u*3;
	    ctl_points[index+0] = params.getReal(i);
	    ctl_points[index+1] = params.getReal(i+1);
	    ctl_points[index+2] = params.getReal(i+2);
	    i += 3;
	}
    }

    // read the domain intervals
    double umin = params.getReal(i);
    double umax = params.getReal(i+1);
    double vmin = params.getReal(i+2);
    double vmax = params.getReal(i+3);

    debug("u: [" << umin << ", " << umax << "]");
    debug("v: [" << vmin << ", " << vmax << "]");

    int controls[] = {ui+1, vi+1};
    int degrees[] = {u_degree, v_degree};
    int index = handleRationalBSplineSurface(controls,
					     degrees,
					     u_closed,
					     v_closed,
					     rational,
					     u_periodic,
					     v_periodic,
					     u_num_knots+1,
					     v_num_knots+1,
					     u_knots,
					     v_knots,
					     weights,
					     ctl_points);
    delete [] ctl_points;
    delete [] weights;
    delete [] v_knots;
    delete [] u_knots;
    return index;
}


int
BrepHandler::extractSurface(const DirectoryEntry* de) {
    debug("########################## E X T R A C T   S U R F A C E");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    // determine the surface type to extract
    switch (de->type()) {
	case ParametricSplineSurface:
	    debug("\tparametric spline surface");
	    break;
	case RuledSurface:
	    debug("\truled surface");
	    break;
	case SurfaceOfRevolution:
	    debug("\tsurface of rev.");
	    return extractSurfaceOfRevolution(params);
	case TabulatedCylinder:
	    debug("\ttabulated cylinder");
	    break;
	case RationalBSplineSurface:
	    debug("\trational b-spline surface");
	    return extractRationalBSplineSurface(params);
	case OffsetSurface:
	    debug("\toffset surface");
	    break;
	case PlaneSurface:
	    debug("\tplane surface");
	    break;
	case RightCircularCylindricalSurface:
	    debug("\tright circular cylindrical surface");
	    break;
	case RightCircularConicalSurface:
	    debug("\tright circular conical surface");
	    break;
	case SphericalSurface:
	    debug("\tspherical surface");
	    break;
	case ToroidalSurface:
	    debug("\ttoroidal surface");
	    break;
    }
    return 0;
}


class PSpaceCurve {
public:
    PSpaceCurve(IGES* _iges, BrepHandler* _brep, Logical& iso, Pointer& c)
    {
	DirectoryEntry* curveDE = _iges->getDirectoryEntry(c);
	ParameterData param;
	_iges->getParameter(curveDE->paramData(), param);
    }
    PSpaceCurve(const PSpaceCurve& ps)
	: isIso(ps.isIso), curveIndex(ps.curveIndex) {}

    Logical isIso;
    int curveIndex;
};


int
BrepHandler::extractEdge(const DirectoryEntry* edgeListDE, int index) {
    //    assert(index > 0);
    if (index <= 0) return index;

    EdgeKey k = make_pair(edgeListDE, index);
    EdgeMap::iterator i = edges.find(k);
    if (i == edges.end()) {
	Pointer initVertexList;
	Integer initVertexIndex;
	Pointer termVertexList;
	Integer termVertexIndex;
	debug("########################## E X T R A C T   E D G E");
	ParameterData params;
	_iges->getParameter(edgeListDE->paramData(), params);
	int paramIndex = (index-1)*5 + 2;
	Pointer msCurvePtr = params.getPointer(paramIndex);
	initVertexList = params.getPointer(paramIndex+1);
	initVertexIndex = params.getInteger(paramIndex+2);
	termVertexList = params.getPointer(paramIndex+3);
	termVertexIndex = params.getInteger(paramIndex+4);

	// extract the model space curves
	int mCurveIndex = extractCurve(_iges->getDirectoryEntry(msCurvePtr), false);

	// extract the vertices
	int initVertex = extractVertex(_iges->getDirectoryEntry(initVertexList),
				       initVertexIndex);
	int termVertex = extractVertex(_iges->getDirectoryEntry(termVertexList),
				       termVertexIndex);

	edges[k] = handleEdge(mCurveIndex, initVertex, termVertex);
	return edges[k];
    } else {
	return i->second;
    }
}


int
BrepHandler::extractLoop(const DirectoryEntry* de, bool isOuter, int face) {
    debug("########################## E X T R A C T   L O O P");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);

    int loop = handleLoop(isOuter, face);
    int numberOfEdges = params.getInteger(1);

    int i = 2; // extract the edge uses!
    for (int _i = 0; _i < numberOfEdges; _i++) {
	bool isVertex = (1 == params.getInteger(i)) ? true : false;
	Pointer edgePtr = params.getPointer(i+1);
	int index = params.getInteger(i+2);
	// need to get the edge list, and extract the edge info
	int edge = extractEdge(_iges->getDirectoryEntry(edgePtr), index);
	bool orientWithCurve = params.getLogical(i+3);

	// handle this edge
	handleEdgeUse(edge, orientWithCurve);

	// deal with param-space curves (not generally included in Pro/E output)
	int numCurves = params.getInteger(i+4);
	int j = i+5;
	list<PSpaceCurve> pCurveIndices;
	for (int _j = 0; _j < numCurves; _j++) {
	    // handle the param-space curves, which are generally not included in MSBO
	    Logical iso = params.getLogical(j);
	    Pointer ptr = params.getPointer(j+1);
	    pCurveIndices.push_back(PSpaceCurve(_iges,
						this,
						iso,
						ptr));
	    j += 2;
	}
	i = j;
    }
    return loop;
}


int
BrepHandler::extractVertex(const DirectoryEntry* de, int index) {
    //    assert(index > 0);
    if (index <= 0) return index;

    VertKey k = make_pair(de, index);
    VertMap::iterator i = vertices.find(k);
    if (i == vertices.end()) {
	// FIXME: ...

	ParameterData params;
	_iges->getParameter(de->paramData(), params);
	int num_verts = params.getInteger(1);
	debug("num verts: " << num_verts);
	debug("index    : " << index);
	assert(index <= num_verts);

	int i = 3*index-1;

	point_t pt;
	pt[X] = params.getReal(i);
	pt[Y] = params.getReal(i+1);
	pt[Z] = params.getReal(i+2);

	// xform matrix application?
	vertices[k] = handleVertex(pt);
	return vertices[k];
    } else {
	return i->second;
    }
}


int
BrepHandler::extractCircularArc(const DirectoryEntry* de, const ParameterData& params) {
    point_t center, start, end;
    double offset_z = params.getReal(1);
    center[X] = params.getReal(2);
    center[Y] = params.getReal(3);
    center[Z] = offset_z;
    start[X]  = params.getReal(4);
    start[Y]  = params.getReal(5);
    start[Z]  = offset_z;
    end[X]    = params.getReal(6);
    end[Y]    = params.getReal(7);
    end[Z]    = offset_z;

    mat_t xform;
    MAT_IDN(xform);
    _iges->getTransformation(de->xform(), xform);

    // choose the circle/interval representation
    // so, calculate the circle params, and then the angle the arc subtends
    double dx = start[X] - center[X];
    double dy = start[Y] - center[Y];
    double radius = sqrt(dx*dx + dy*dy);

    point_t tcenter, tstart, tend;
    MAT4X3PNT(tcenter, xform, center);
    MAT4X3PNT(tstart, xform, start);
    MAT4X3PNT(tend, xform, end);
    vect_t normal = {0, 0, 1};
    vect_t tnormal;
    MAT4X3VEC(tnormal, xform, normal);

    return handleCircularArc(radius, tcenter, tnormal, tstart, tend);
}


int
BrepHandler::extractRationalBSplineCurve(const DirectoryEntry* de, const ParameterData& params) {
    int k       = params.getInteger(1);
    int degree  = params.getInteger(2);
    bool planar = (params.getInteger(3)() == 1) ? true : false;
    bool closed = (params.getInteger(4)() == 1) ? true : false;
    bool rational = (params.getInteger(5)() == 0) ? true : false;
    bool periodic = (params.getInteger(6)() == 1) ? true : false;

    int num_control_points = k + 1;
    int n = k + 1 - degree;
    int num_knots = n + 2 * degree + 1;

    double* knots = new double[num_knots];
    int i = 7;
    for (int _i = 0; _i < num_knots; _i++) {
	knots[_i] = params.getReal(i);
	i++;
    }

    double* weights = new double[num_control_points];
    for (int _i = 0; _i < num_control_points; _i++) {
	weights[_i] = params.getReal(i);
	i++;
    }

    double* ctl_points = new double[num_control_points * 3];
    for (int _i = 0; _i < num_control_points; _i++) {
	ctl_points[_i*3]   = params.getReal(i);
	ctl_points[_i*3+1] = params.getReal(i+1);
	ctl_points[_i*3+2] = params.getReal(i+2);
	i += 3;
    }

    double umin = params.getReal(i); i++;
    double umax = params.getReal(i); i++;

    vect_t unit_normal;
    if (planar) {
	VSET(unit_normal, params.getReal(i), params.getReal(i+1), params.getReal(i+2));
	i += 3;
    }

    int val = handleRationalBSplineCurve(degree,
					 umin,
					 umax,
					 planar,
					 unit_normal,
					 closed,
					 rational,
					 periodic,
					 num_knots,
					 knots,
					 num_control_points,
					 weights,
					 ctl_points);
    delete [] knots;
    delete [] weights;
    delete [] ctl_points;
    return val;
}


int
BrepHandler::extractCurve(const DirectoryEntry* de, bool isISO) {
    debug("########################## E X T R A C T   C U R V E");
    ParameterData params;
    _iges->getParameter(de->paramData(), params);
    switch (de->type()) {
	case CircularArc:
	    debug("\tcircular arc");
	    return extractCircularArc(de, params);
	case CompositeCurve:
	    debug("\tcomposite curve");
	    break;
	case ConicArc:
	    debug("\tconic arc");
	    break;
	case CopiousData:
	    debug("\tcopious data");
	    // 11: 2d path
	    // 12: 3d path
	    // 63: simple closed planar curve
	    break;
	case Line:
	    debug("\tline");
	    return extractLine(de, params);
	case ParametricSplineCurve:
	    debug("\tparametric spline curve");
	    break;
	case RationalBSplineCurve:
	    debug("\trational b-spline curve");
	    return extractRationalBSplineCurve(de, params);
	case OffsetCurve:
	    debug("\toffset curve");
	    break;
    }
    return 0;
}


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
