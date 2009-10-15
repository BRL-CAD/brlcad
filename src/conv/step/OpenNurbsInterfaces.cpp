/*                 OpenNurbsInterfaces.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file OpenNurbsInterfaces.cpp
 *
 * Routines to convert STEP "OpenNurbsInterfaces" to BRL-CAD BREP
 * structures.
 *
 */
#include  "opennurbs.h"
#include "opennurbs_ext.h"
#include  "nurb.h"

#define P23_NAMESPACE_F(x) SDAI##x
#define SCLP23(x) P23_NAMESPACE_F(_##x)
class SCLP23(Application_instance);


#include "STEPEntity.h"
#include "Axis1Placement.h"
#include "Factory.h"
#include "LocalUnits.h"
#include "PullbackCurve.h"
#include "Point.h"
#include "CartesianPoint.h"
#include "VertexPoint.h"
#include "Vector.h"
#include "EdgeCurve.h"
#include "OrientedEdge.h"

// Curve includes
#include "BezierCurve.h"
#include "BSplineCurve.h"
#include "BSplineCurveWithKnots.h"
#include "QuasiUniformCurve.h"
#include "RationalBezierCurve.h"
#include "RationalBSplineCurve.h"
#include "RationalBSplineCurveWithKnots.h"
#include "RationalQuasiUniformCurve.h"
#include "RationalUniformCurve.h"
#include "UniformCurve.h"

// Surface includes
#include "Line.h"
#include "Circle.h"
#include "Ellipse.h"
#include "Hyperbola.h"
#include "Parabola.h"
#include "CylindricalSurface.h"
#include "ConicalSurface.h"
#include "SweptSurface.h"
#include "SurfaceOfLinearExtrusion.h"
#include "SurfaceOfRevolution.h"
#include "Path.h"
#include "Plane.h"
#include "Loop.h"
#include "VertexLoop.h"
#include "Face.h"
#include "FaceBound.h"
#include "FaceOuterBound.h"
#include "FaceSurface.h"
#include "BezierSurface.h"
#include "BSplineSurface.h"
#include "BSplineSurfaceWithKnots.h"
#include "QuasiUniformSurface.h"
#include "RationalBezierSurface.h"
#include "RationalBSplineSurface.h"
#include "RationalBSplineSurfaceWithKnots.h"
#include "RationalQuasiUniformSurface.h"
#include "RationalUniformSurface.h"
#include "UniformSurface.h"

#include "AdvancedBrepShapeRepresentation.h"
#include "PullbackCurve.h"

using namespace brlcad;

ON_Brep *
AdvancedBrepShapeRepresentation::GetONBrep()
{
	ON_Brep *brep = ON_Brep::New();

	if (!LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << endl;
		//still return brep may contain something useful to diagnose
		return brep;
	}

	return brep;
}
bool
AdvancedBrepShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
	LIST_OF_REPRESENTATION_ITEMS::iterator i;

	for(i=items.begin(); i!=items.end();i++) {
		if ( !(*i)->LoadONBrep(brep)) {
			cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
			return false;
		}
	}

	return true;
}

//
// Curve handlers
//
bool
BezierCurve::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
	return false;
}

bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New( 3, false,degree+1,t_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	curve->SetKnot(knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	curve->SetKnot(i, 1.0);
    }

	LIST_OF_POINTS::iterator i;
	int t=0;
	for(i=control_points_list.begin(); i != control_points_list.end(); ++i) {
		curve->SetCV(t, ON_3dPoint((*i)->X()*LocalUnits::length,(*i)->Y()*LocalUnits::length,(*i)->Z()*LocalUnits::length));
		t++;
	}
	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New( 3, false,degree+1,t_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
    LIST_OF_REALS::iterator r = knots.begin();
    int knot_index = 0;
    while (m != knot_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > degree) multiplicity = degree;
    	for (int j = 0; j < multiplicity; j++,knot_index++) {
        	curve->SetKnot(knot_index, knot_value);
    	}
    	r++;
    	m++;
    }

	LIST_OF_POINTS::iterator i;
	int t=0;
	for(i=control_points_list.begin(); i != control_points_list.end(); ++i) {
		curve->SetCV(t, ON_3dPoint((*i)->X()*LocalUnits::length,(*i)->Y()*LocalUnits::length,(*i)->Z()*LocalUnits::length));
		t++;
	}
	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	if (!BSplineCurve::LoadONBrep(brep)) {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
		return false;
	}
	return true;
}

bool
RationalBezierCurve::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
	return false;
}

bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New( 3, true,degree+1,t_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	curve->SetKnot(knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	curve->SetKnot(i, 1.0);
    }

	LIST_OF_POINTS::iterator i;
	LIST_OF_REALS::iterator r = weights_data.begin();
	int t=0;
	for(i=control_points_list.begin(); i != control_points_list.end(); ++i) {
		double w = (*r);
		curve->SetCV(t, ON_4dPoint((*i)->X()*LocalUnits::length*w,(*i)->Y()*LocalUnits::length*w,(*i)->Z()*LocalUnits::length*w,w));
		t++;
		r++;
	}

	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0 )
		return true;

	int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New( 3, true,degree+1,t_size);

	// knot index ( >= 0 and < Order + CV_count - 2 )
    LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
    LIST_OF_REALS::iterator r = knots.begin();
    int knot_index = 0;
    while (m != knot_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > degree) multiplicity = degree;
    	for (int j = 0; j < multiplicity; j++,knot_index++) {
        	curve->SetKnot(knot_index, knot_value);
    	}
    	r++;
    	m++;
    }

	LIST_OF_POINTS::iterator i;
	r = weights_data.begin();
	int t=0;
	for(i=control_points_list.begin(); i != control_points_list.end(); ++i) {
		double w = (*r);
		curve->SetCV(t, ON_4dPoint((*i)->X()*LocalUnits::length*w,(*i)->Y()*LocalUnits::length*w,(*i)->Z()*LocalUnits::length*w,w));
		t++;
		r++;
	}

	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	if (!RationalBSplineCurve::LoadONBrep(brep)) {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
		return false;
	}
	return true;
}

bool
RationalUniformCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	if (!RationalBSplineCurve::LoadONBrep(brep)) {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
		return false;
	}
	return true;
}

bool
UniformCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true;

	if (!BSplineCurve::LoadONBrep(brep)) {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << " id: " << id << endl;
		return false;
	}
	return true;
}

//
// Surface handlers
//
bool
BezierSurface::LoadONBrep(ON_Brep *brep)
{
	//TODO: add bezier surface
	//ON_BezierSurface* surf = ON_BezierSurface::New( 3, false,u_degree+1,v_degree+1);
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}

bool
BSplineSurface::LoadONBrep(ON_Brep *brep)
{
	int u_size = control_points_list->size();
	int v_size = (*control_points_list->begin())->size();

    ON_NurbsSurface* surf = ON_NurbsSurface::New( 3, false,u_degree+1,v_degree+1,u_size,v_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	surf->SetKnot(1, i, 1.0);
    }

	LIST_OF_LIST_OF_POINTS::iterator i;
	int u=0;
	for(i=control_points_list->begin(); i != control_points_list->end(); ++i) {
		LIST_OF_POINTS::iterator j;
		LIST_OF_POINTS *p = *i;
		int v=0;
		for(j=p->begin(); j != p->end(); ++j) {
			surf->SetCV(u, v, ON_3dPoint((*j)->X()*LocalUnits::length,(*j)->Y()*LocalUnits::length,(*j)->Z()*LocalUnits::length));
			v++;
		}
		u++;
	}
    ON_id = brep->AddSurface(surf);

	return true;
}

bool
BSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
	int u_size = control_points_list->size();
	int v_size = (*control_points_list->begin())->size();

    ON_NurbsSurface* surf = ON_NurbsSurface::New( 3, false,u_degree+1,v_degree+1,u_size,v_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > u_degree) multiplicity = u_degree;
    	for (int j = 0; j < multiplicity; j++) {
    		surf->SetKnot(0, knot_index++, knot_value);
    	}
    	r++;
    	m++;
    }
    m = v_multiplicities.begin();
    r = v_knots.begin();
    knot_index = 0;
    while (m != v_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > v_degree) multiplicity = v_degree;
    	for (int j = 0; j < multiplicity; j++,knot_index++) {
    		surf->SetKnot(1, knot_index, knot_value);
    	}
    	r++;
    	m++;
    }
	LIST_OF_LIST_OF_POINTS::iterator i;
	int u=0;
	for(i=control_points_list->begin(); i != control_points_list->end(); ++i) {
		LIST_OF_POINTS::iterator j;
		LIST_OF_POINTS *p = *i;
		int v=0;
		for(j=p->begin(); j != p->end(); ++j) {
			surf->SetCV(u, v, ON_3dPoint((*j)->X()*LocalUnits::length,(*j)->Y()*LocalUnits::length,(*j)->Z()*LocalUnits::length));
			v++;
		}
		u++;
	}
    ON_id = brep->AddSurface(surf);

	return true;
}

bool
QuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
	if (!BSplineSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	return true;
}

bool
RationalBezierSurface::LoadONBrep(ON_Brep *brep)
{
	//TODO: add rational bezier surface
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return false;
}

bool
RationalBSplineSurface::LoadONBrep(ON_Brep *brep)
{
	int u_size = control_points_list->size();
	int v_size = (*control_points_list->begin())->size();

    ON_NurbsSurface* surf = ON_NurbsSurface::New( 3, false,u_degree+1,v_degree+1,u_size,v_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
    	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n-p; j++) {
    	double x = (double)j/(double)(n-p);
    	int knot_index = j+p-1;
    	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m-p; i < m; i++) {
    	surf->SetKnot(1, i, 1.0);
    }

	LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
	LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
	LIST_OF_REALS::iterator r;
	int u=0;
	for(i=control_points_list->begin(); i != control_points_list->end(); ++i) {
		LIST_OF_POINTS::iterator j;
		LIST_OF_POINTS *p = *i;
		r = (*w)->begin();
		int v=0;
		for(j=p->begin(); j != p->end(); ++j,r++,v++) {
			double w = (*r);
			surf->SetCV(u, v, ON_4dPoint((*j)->X()*LocalUnits::length*w,(*j)->Y()*LocalUnits::length*w,(*j)->Z()*LocalUnits::length*w,w));
		}
		u++;
		w++;
	}
    ON_id = brep->AddSurface(surf);

	return true;
}

bool
RationalBSplineSurfaceWithKnots::LoadONBrep(ON_Brep *brep)
{
	int u_size = control_points_list->size();
	int v_size = (*control_points_list->begin())->size();

	if (self_intersect) {
	}
	if (u_closed) {
	}
	if (v_closed) {
	}

    ON_NurbsSurface* surf = ON_NurbsSurface::New( 3, true,u_degree+1,v_degree+1,u_size,v_size);

    // knot index ( >= 0 and < Order + CV_count - 2 )
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > u_degree) multiplicity = u_degree;
    	for (int j = 0; j < multiplicity; j++,knot_index++) {
    		surf->SetKnot(0, knot_index, knot_value);
    	}
    	r++;
    	m++;
    }
    m = v_multiplicities.begin();
    r = v_knots.begin();
    knot_index = 0;
    while (m != v_multiplicities.end()) {
    	int multiplicity = (*m);
		double knot_value = (*r);
		if (multiplicity > v_degree) multiplicity = v_degree;
    	for (int j = 0; j < multiplicity; j++) {
    		surf->SetKnot(1, knot_index++, knot_value);
    	}
    	r++;
    	m++;
    }

	LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
	LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
	int u=0;
	for(i=control_points_list->begin(); i != control_points_list->end(); ++i) {
		LIST_OF_POINTS::iterator j;
		LIST_OF_POINTS *p = *i;
		r = (*w)->begin();
		int v=0;
		for(j=p->begin(); j != p->end(); ++j,r++,v++) {
			double w = (*r);
			surf->SetCV(u, v, ON_4dPoint((*j)->X()*LocalUnits::length*w,(*j)->Y()*LocalUnits::length*w,(*j)->Z()*LocalUnits::length*w,w));
		}
		u++;
		w++;
	}

	ON_id = brep->AddSurface(surf);

	return true;
}

bool
RationalQuasiUniformSurface::LoadONBrep(ON_Brep *brep)
{
	if (!RationalBSplineSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	return true;
}

bool
RationalUniformSurface::LoadONBrep(ON_Brep *brep)
{
	if (!RationalBSplineSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	return true;
}

bool
UniformSurface::LoadONBrep(ON_Brep *brep)
{
	if (!BSplineSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	return true;
}
void
FaceSurface::AddFace(ON_Brep *brep)
{
	ON_BrepFace& face = brep->NewFace(face_geometry->GetONId());
	if (same_sense == 1) {
		face.m_bRev = false;
	} else {
		face.m_bRev = true;
		face.Reverse(0);
	}

	ON_id = face.m_face_index;
}

bool
FaceSurface::LoadONBrep(ON_Brep *brep)
{
	// need edge bounds to determine extents for some of the infinitely
	// defined surfaces like cones/cylinders/planes
	ON_BoundingBox *bb = GetEdgeBounds(brep);
	face_geometry->SetCurveBounds(bb);

	if (!face_geometry->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	AddFace(brep);

	if (!Face::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	return true;
}

void
Point::AddVertex(ON_Brep *brep)
{
	cerr << "Warning: " << entityname << "::AddVertex()  should be overridden by parent class." << endl;
}

void
CartesianPoint::AddVertex(ON_Brep *brep)
{
	if (vertex_index < 0) {
		ON_3dPoint p(coordinates[0]*LocalUnits::length,coordinates[1]*LocalUnits::length,coordinates[2]*LocalUnits::length);
		ON_BrepVertex& v = brep->NewVertex(p);
		vertex_index = v.m_vertex_index;
		v.m_tolerance = 1e-3;
		ON_id = v.m_vertex_index;
	}
}

void
BSplineCurve::AddPolyLine(ON_Brep *brep)
{
	if (ON_id < 0) {
		int num_control_points = control_points_list.size();

	    if ((degree == 1) && (num_control_points >= 2)) {
	    	LIST_OF_POINTS::iterator i = control_points_list.begin();
	    	CartesianPoint *cp = (*i);
	    	while ((++i) != control_points_list.end()) {
	    		ON_3dPoint start(cp->X()*LocalUnits::length,cp->Y()*LocalUnits::length,cp->Z()*LocalUnits::length);
	    		cp = (*i);
	    		ON_3dPoint end(cp->X()*LocalUnits::length,cp->Y()*LocalUnits::length,cp->Z()*LocalUnits::length);
	    		ON_LineCurve* line = new ON_LineCurve(ON_3dPoint(start),ON_3dPoint(end));
	    		brep->m_C3.Append(line);
	    	}
	    } else if (num_control_points > 2) {
			ON_NurbsCurve* c = ON_NurbsCurve::New(3,
							  false,
							  degree+1,
							  num_control_points);
	    } else {
			cerr << "Error: " << entityname << "::LoadONBrep() - Error loading polyLine." << endl;
	    }
	}
}

void
VertexPoint::AddVertex(ON_Brep *brep)
{
	vertex_geometry->AddVertex(brep);
}

ON_BoundingBox *
Face::GetEdgeBounds(ON_Brep *brep) {
	ON_BoundingBox *u = NULL;
	LIST_OF_FACE_BOUNDS::iterator i;
	for(i=bounds.begin();i!=bounds.end();i++){
		ON_BoundingBox *bb = (*i)->GetEdgeBounds(brep);
		if (bb != NULL) {
			if (u == NULL)
				u = new ON_BoundingBox();
			u->Union(*bb);
		}
		delete bb;
	}
	return u;
}

bool
Face::LoadONBrep(ON_Brep *brep)
{
	//TODO: Check for Outer bound if none check for
	// direction perhaps offer input option possibly
	// check for outer spanning to bounds
	int cnt = 0;
	LIST_OF_FACE_BOUNDS::reverse_iterator i;
	for(i=bounds.rbegin();i!=bounds.rend();i++){
		if (cnt == 0) {
			(*i)->SetOuter();
		}
		(*i)->SetFaceIndex(ON_id);
		if (!(*i)->LoadONBrep(brep)) {
			//(*i)->GetONId()
			cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
			return false;
		}
		ON_BrepFace& face = brep->m_F[GetONId()];
		cnt++;
	}
	return true;
}

bool
FaceOuterBound::LoadONBrep(ON_Brep *brep)
{
	SetOuter();

	if (!FaceBound::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	return true;
}

ON_BoundingBox *
FaceBound::GetEdgeBounds(ON_Brep *brep) {
	return bound->GetEdgeBounds(brep);
}

bool
FaceBound::LoadONBrep(ON_Brep *brep)
{
	if (ON_id < 0) {
		enum ON_BrepLoop::TYPE btype;
		if (IsInner()) {
			btype = ON_BrepLoop::inner;
		} else {
			btype = ON_BrepLoop::outer;
		}
		ON_BrepLoop& loop = brep->NewLoop(btype, brep->m_F[ON_face_index]);
		ON_id = loop.m_loop_index;
	}
	bound->SetLoopIndex(ON_id);
	if (!bound->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	if (IsInner()) {
		ON_BrepLoop& loop = brep->m_L[ON_id];
		if (brep->LoopDirection((const ON_BrepLoop&)loop) > 0) {
			brep->FlipLoop(loop);
		}
	} else {
		ON_BrepLoop& loop = brep->m_L[ON_id];
		if (brep->LoopDirection((const ON_BrepLoop&)loop) < 0) {
			brep->FlipLoop(loop);
		}
	}
	return true;
}

bool
EdgeCurve::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; // already loaded

	edge_geometry->Start(edge_start);
	edge_geometry->End(edge_end);
	if (!edge_geometry->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	int se,ee,eg;
	se = edge_start->GetONId();
	ee = edge_end->GetONId();
	eg = edge_geometry->GetONId();

	ON_BrepEdge& edge = brep->NewEdge(brep->m_V[edge_start->GetONId()], brep->m_V[edge_end->GetONId()], edge_geometry->GetONId());
    edge.m_tolerance = 1e-3; //TODO: - need tolerance definition or constant
    ON_id = edge.m_edge_index;

	return true;
}

bool
OrientedEdge::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; //already loaded

	if (!edge_start->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	if (!edge_end->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}
	if ( !edge_element->LoadONBrep(brep) ) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	ON_id = edge_element->GetONId();

	return true;
}

ON_BoundingBox *
Path::GetEdgeBounds(ON_Brep *brep) {
	ON_BoundingBox *u = NULL;
	LIST_OF_ORIENTED_EDGES::iterator i;
	for(i=edge_list.begin();i!=edge_list.end();i++) {
		if (!(*i)->LoadONBrep(brep)) {
			cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
			return false;
		}
		if (u == NULL)
			u = new ON_BoundingBox();
	    const ON_BrepEdge* edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve* curve = edge->EdgeCurveOf();
	    u->Union(curve->BoundingBox());
	}

	return u;
}

bool
Path::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;
	LIST_OF_ORIENTED_EDGES::iterator i;

	for(i=edge_list.begin();i!=edge_list.end();i++) {
		if (!(*i)->LoadONBrep(brep)) {
			cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
			return false;
		}
	}
	if (!LoadONTrimmingCurves(brep)) {
		return false;
	}

	return true;
}

bool
Path::ShiftSurfaceSeam(ON_Brep *brep, double *t)
{
    const ON_BrepLoop* loop = &brep->m_L[ON_path_index];
    const ON_BrepFace* face = loop->Face();
    const ON_Surface* surface = face->SurfaceOf();
    double ang_min = 0.0;
    double smin,smax;

    if ( surface->IsCone() || surface->IsCylinder()) {
    	if (surface->IsClosed(0) ) {
			surface->GetDomain(0,&smin,&smax);
    	} else {
    		surface->GetDomain(1,&smin,&smax);
    	}

		LIST_OF_ORIENTED_EDGES::iterator i;
		for (i = edge_list.begin(); i != edge_list.end(); i++) {
			// grab the curve for this edge,face and surface
			const ON_BrepEdge* edge = &brep->m_E[(*i)->GetONId()];
			const ON_Curve* curve = edge->EdgeCurveOf();
			double tmin, tmax;
			curve->GetDomain(&tmin, &tmax);

			if (((tmin < 0.0) && (tmax > 0.0)) &&
					((tmin > smin) ) || (tmax < smax)) {
				if (tmin < ang_min)
					ang_min = tmin;
			}

		}

		if (ang_min < 0.0) {
			*t = ang_min;
			return true;
		}
    }
	return false;
}

bool
Path::LoadONTrimmingCurves(ON_Brep *brep)
{
    ON_TextLog tl;
	LIST_OF_ORIENTED_EDGES::iterator i;
	list<PBCData *> curve_pullback_samples;
    const ON_BrepLoop* loop = &brep->m_L[ON_path_index];
    const ON_BrepFace* face = loop->Face();
    const ON_Surface* surface = face->SurfaceOf();


/*
    //First let's check to see if we have to shift the surface seam
	double t;
	if ( surface->IsClosed(0) || surface->IsClosed(1) )
	{
		//TODO: look for other closed surfaces to assess like this
		if (ShiftSurfaceSeam(brep,&t)) {
			ON_Surface* surf = (ON_Surface*)surface;
			ON_Cylinder cylinder;
			ON_Cone cone;
			if ( surface->IsCylinder(&cylinder) ) {
				surf->Rotate(t,cylinder.Axis(),cylinder.Center());
			} //else if ( surface->IsCone(&cone) ) {
				//surf->Rotate(t,cone.Axis(),cone.BasePoint());
			//}
		}
	}
*/


    // build surface tree making sure not to remove trimmed subsurfaces
    // since currently building trims and need full tree
    bool removeTrimmed = false;
	SurfaceTree* st = new SurfaceTree((ON_BrepFace*)face, removeTrimmed);


    PBCData *data = NULL;
	LIST_OF_ORIENTED_EDGES::iterator prev,next;
	for(i=edge_list.begin();i!=edge_list.end();i++) {
	    // grab the curve for this edge,face and surface
	    const ON_BrepEdge* edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve* curve = edge->EdgeCurveOf();
	    ON_BoundingBox bb = curve->BoundingBox();
	    ON_Curve* c2d;
	    bool orientWithCurve;
	    double t,nudge;
		if (false && isSeam(i)) {
			ON_2dPoint prev_uv,next_uv;
			prev = getPrev(i);
			next = getNext(i);
		    const ON_BrepEdge* edge = &brep->m_E[(*prev)->GetONId()];
		    const ON_Curve* prev_curve = edge->EdgeCurveOf();
		    orientWithCurve = (*prev)->OrientWithEdge();
		    if (!orientWithCurve) {
		    	t = 0.0;
		    	nudge = 0.02;
		    } else {
		    	t = 1.0;
		    	nudge = -0.02;
		    }
		    ON_3dPoint P = prev_curve->PointAt(t);
		    toUV(st, (const ON_Curve *)prev_curve,  prev_uv, t, nudge);

		    edge = &brep->m_E[(*next)->GetONId()];
		    const ON_Curve* next_curve = edge->EdgeCurveOf();
		    orientWithCurve = (*next)->OrientWithEdge();
		    if (!orientWithCurve) {
		    	t = 1.0;
		    	nudge = -0.02;
		    } else {
		    	t = 0.0;
		    	nudge = 0.02;
		    }
		    ON_3dPoint N = next_curve->PointAt(t);
		    toUV(st, (const ON_Curve *)next_curve,  next_uv, t, nudge);

		    ON_2dPoint uv1 = st->getClosestPointEstimate(P);
		    ON_2dPoint uv2 = st->getClosestPointEstimate(N);
		    enum seam_direction dir = seam_direction(prev_uv,next_uv);

		    c2d = pullback_seam_curve(dir, st, curve);
		    orientWithCurve = (*i)->OrientWithEdge();
		} else {
			ON_2dPoint uv;
		    orientWithCurve = (*i)->OrientWithEdge();
		    if (!orientWithCurve) {
		    	t = 1.0;
		    	nudge = -0.0000001;
		    } else {
		    	t = 0.0;
		    	nudge = 0.0000001;
		    }
		    toUV(st, (const ON_Curve *)curve,  uv, t + nudge,0.0);
		    ON_Interval dom = curve->Domain();
		    const ON_3dPoint p = curve->PointAt(dom[0]);
		    const ON_3dPoint from = curve->PointAt(dom[0]+nudge);
		    if ( !st->getSurfacePoint(p,uv,from) > 0 ) {
		      	cerr << "Error: Can not get surface point." << endl;
		    }
//		    ON_Interval u = surface->Domain(0);
//		    ON_Interval v = surface->Domain(1);
//		    ON_3dPoint startuv(u[0],v[0],0.0);
//		    ON_3dPoint enduv(u[1],v[0],0.0);
//		    c2d = surface->Pullback( *curve,1.0e-6,NULL,startuv,enduv);
		    //c2d = pullback_curve(st, curve);
		    //c2d = test2_pullback_curve(st, curve);
		    data = test2_pullback_samples(st, curve);
		    if (!orientWithCurve) {
		      data->order_reversed = true;
		    } else {
		    	data->order_reversed = false;
		    }
		    data->edge = edge;
		    curve_pullback_samples.push_back(data);
		    if (!orientWithCurve) {
		    	list<ON_2dPointArray*>::iterator si;
		    	si = data->segments.begin();
		    	list<ON_2dPointArray*> rsegs;
		    	while (si != data->segments.end()) {
					ON_2dPointArray* samples = (*si);
					samples->Reverse();
					rsegs.push_front(samples);
					si++;
		    	}
		    	data->segments.clear();
		    	si = rsegs.begin();
		    	while (si != rsegs.end()) {
		    		ON_2dPointArray* samples = (*si);
		    		data->segments.push_back(samples);
		    		si++;
		    	}
		    	rsegs.clear();
		    }

		    //ON_Curve* testcurve = c2d->Duplicate();
		    //c2d = brlcad::pullback_curve((ON_BrepFace*)face,curve);
		}
		/*
	    if (!orientWithCurve) {
	      c2d->Reverse();
	      data->order_reversed = true;
	    } else {
	    	data->order_reversed = false;
	    }
	    */
	}
	// check for seams and singularities
	if ( !check_pullback_data(curve_pullback_samples) ) {
		cerr << "Error: Can not resolve seam or singularity issues." << endl;
	}
	list<PBCData *>::iterator cs = curve_pullback_samples.begin();
	list<PBCData *>::iterator next_cs;
	int trimcnt = 0;
//	while(cs!=curve_pullback_samples.end()) {
//		PBCData *data = (*cs);
//		int ilast = data->samples.Count() - 1;
//		cerr << "T:" << trimcnt++ << endl;
//		cerr << "    start - " << data->samples[0].x << "," << data->samples[0].y << endl;
//		cerr << "    end -   " << data->samples[ilast].x << "," << data->samples[ilast].y << endl;
//		cs++;
//	}
//	cs = curve_pullback_samples.begin();

/*
	//TODO: check validate routine with boggle
	//ugly hack to get trim endpoint tolerance happy
	while(cs!=curve_pullback_samples.end()) {
		ON_2dPoint end_current,start_next;
		PBCData *data = (*cs);

		next_cs = cs;
		next_cs++;
		if (next_cs == curve_pullback_samples.end())
			next_cs = curve_pullback_samples.begin();
	    end_current=data->samples[data->samples.Count()-1];
	    start_next=(*next_cs)->samples[0];
	    //TODO: check over tolerance here
	    double d = end_current.DistanceTo(start_next);
	    if (d <= 0.0001) {
	    	data->samples[data->samples.Count()-1].x = start_next.x;
	    	data->samples[data->samples.Count()-1].y = start_next.y;
	    }
	    cs++;
	}
*/
	cs = curve_pullback_samples.begin();
	while(cs!=curve_pullback_samples.end()) {
		next_cs = cs;
		next_cs++;
		if (next_cs == curve_pullback_samples.end())
			next_cs = curve_pullback_samples.begin();
		PBCData *data = (*cs);
    	list<ON_2dPointArray*>::iterator si;
    	si = data->segments.begin();
		PBCData *ndata = (*next_cs);
    	list<ON_2dPointArray*>::iterator nsi;
    	nsi = ndata->segments.begin();
    	ON_2dPointArray* nsamples = (*nsi);

    	while (si != data->segments.end()) {
			ON_2dPointArray* samples = (*si);

			//TODO:Fix this shouldn't have sample counts less than 2
			if (samples->Count() < 2) {
				si++;
				continue;
			}
			ON_Curve* c2d = interpolateCurve(*samples);
			int trimCurve = brep->m_C2.Count();
			brep->m_C2.Append(c2d);

			ON_BrepTrim& trim = brep->NewTrim((ON_BrepEdge&)*data->edge, data->order_reversed, (ON_BrepLoop&)*loop, trimCurve);
			ON_BrepTrim::TYPE ttype = brep->TrimType( trim, false );
			//debug("handleEdgeUse: trim " << (orientWithCurve ? "is not " : "is ") << "reversed");
	//	    trim.m_type = ON_BrepTrim::mated; // closed solids!
	//		if ((trim.m_trim_index == 113)||(trim.m_trim_index == 115) ||
	//				(trim.m_trim_index == 117)||(trim.m_trim_index == 119)) {
	//		    trim.m_type = ON_BrepTrim::seam; // closed solids!
	//			cout << "debugging" << endl;
	//		}
	//	    if (trim.m_trim_index == 112) {
	//	      trim.m_bRev3d = true;
	//	    }
			trim.m_tolerance[0] = 1e-3; // XXX: tolerance?
			trim.m_tolerance[1] = 1e-3;
			ON_Interval PD = trim.ProxyCurveDomain();
			trim.m_iso = surface->IsIsoparametric(*c2d, &PD);
			//trim.Reverse();
			trim.m_type = ttype;

			trim.IsValid(&tl);

			int ilast = samples->Count() - 1;

			// check for bridging trim, trims along singularities
			// are implicitly expected
			ON_2dPoint end_current,start_next;
			end_current=(*samples)[samples->Count()-1];
	//	    //TODO:Fix this shouldn't have sample counts less than 2
	//	    while ((*next_cs)->samples.Count() < 2) {
	//	    	next_cs++;
	//			if (next_cs == curve_pullback_samples.end())
	//				next_cs = curve_pullback_samples.begin();
	//	    }
			start_next=(*nsamples)[0];

			if (true && (end_current.DistanceTo(start_next) > PBC_TOL)) {
				//cerr << "endpoints don't connect" << endl;
				int is;
				const ON_Surface *surf = data->surftree->getSurface();
				if ( (is=check_pullback_singularity_bridge(surf,end_current,start_next)) >= 0) {
					// insert trim
					//cerr << "insert singular trim along ";
					// 0 = south, 1 = east, 2 = north, 3 = west
					ON_Surface::ISO iso;
					switch (is) {
					case 0:
						//south
						iso = ON_Surface::S_iso;
						break;
					case 1:
						//east
						iso = ON_Surface::E_iso;
						break;
					case 2:
						//north
						iso = ON_Surface::N_iso;
						break;
					case 3:
						//west
						iso = ON_Surface::W_iso;
					}

					ON_Curve* c2d = new ON_LineCurve(end_current,start_next);
					trimCurve = brep->m_C2.Count();
					brep->m_C2.Append(c2d);

					int vi;
					if (data->order_reversed)
						vi = data->edge->m_vi[0];
					else
						vi = data->edge->m_vi[1];

					ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[vi],(ON_BrepLoop&)*loop,iso,trimCurve);

					trim.m_tolerance[0] = 1e-3; //TODO: need constant tolerance?
					trim.m_tolerance[1] = 1e-3;
					ON_Interval PD = trim.ProxyCurveDomain();
					trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
					trim.m_iso = iso;
					//trim.Reverse();
					trim.IsValid(&tl);
				} /*else if ((is=check_pullback_seam_bridge(surf,end_current,start_next)) > 0) {
					ON_Surface::ISO iso;
					switch (is) {
					case 1:
						//east
						iso = ON_Surface::x_iso;
						break;
					case 2:
						//north
						iso = ON_Surface::y_iso;
						break;
					case 3:
						//west
						iso = ON_Surface::not_iso;
					}

					ON_Curve* c2d = new ON_LineCurve(end_current,start_next);
					trimCurve = brep->m_C2.Count();
					brep->m_C2.Append(c2d);

					int vi;
					ON_BrepVertex& v = brep->NewVertex(end_current,1e-6);

					ON_BrepTrim& trim = brep->NewSingularTrim(v,(ON_BrepLoop&)*loop,iso,trimCurve);

					trim.m_tolerance[0] = 1e-3; //TODO: need constant tolerance?
					trim.m_tolerance[1] = 1e-3;
					ON_Interval PD = trim.ProxyCurveDomain();
					trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
					trim.m_iso = iso;
					//trim.Reverse();
					trim.IsValid(&tl);
				}*/
			}
			si++;
		}

	    cs++;
	}

	return true;
}

bool
Plane::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();
	ON_3dVector norm=GetNormal();

	origin = origin*LocalUnits::length;

	ON_Plane p(origin,xaxis,yaxis);


	ON_PlaneSurface *s= new ON_PlaneSurface(p);

	double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
	// origin may not lie within face so include in extent
	double maxdist = origin.DistanceTo(trim_curve_3d_bbox->m_max);
	double mindist = origin.DistanceTo(trim_curve_3d_bbox->m_min);
	bbdiag += MAX(maxdist,mindist);

	//TODO: look into line curves that are just point and direction
	ON_Interval extents(-bbdiag, bbdiag);
    s->SetExtents(0, extents );
    s->SetExtents(1, extents );
    s->SetDomain(0, 0.0, 1.0 );
    s->SetDomain(1, 0.0, 1.0 );

	ON_id = brep->AddSurface(s);

	return true;
}

bool
CylindricalSurface::LoadONBrep(ON_Brep *brep)
{
	// new surface if reused because of bounding
	//if (ON_id >= 0)
	//	return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin*LocalUnits::length;

	double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
	origin=origin - bbdiag*norm;
	ON_Plane p(origin,xaxis,yaxis);

	// Creates a circle parallel to the plane
	// with given center and radius.
	ON_Circle c(p,origin,radius*LocalUnits::length);


	//ON_Cylinder cyl(c,ON_DBL_MAX);
	ON_Cylinder cyl(c,2.0*bbdiag);

	ON_RevSurface* s = cyl.RevSurfaceForm();
	if ( s )
	{
		double r = fabs(cyl.circle.radius);
		if ( r <= ON_SQRT_EPSILON )
			r = 1.0;
		s->SetDomain(0,0.0,2.0*ON_PI*r);
	}
	ON_id = brep->AddSurface(s);

	return true;
}

bool
ConicalSurface::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin*LocalUnits::length;

	double tan_semi_angle = tan(semi_angle*LocalUnits::planeangle);
	double height = (radius*LocalUnits::length)/tan_semi_angle;
	double hplus = height*1.01;
	double r1 = hplus*tan_semi_angle;

	origin = origin + norm*(-height);
	ON_Plane p(origin,xaxis,yaxis);
	ON_Cone c(p,hplus,r1);

	ON_RevSurface* s = c.RevSurfaceForm();
	if ( s )
	{
		double r = fabs(c.radius);
		if ( r <= ON_SQRT_EPSILON )
			r = 1.0;
		s->SetDomain(0,0.0,2.0*ON_PI);
	}

	ON_id = brep->AddSurface(s);

	return true;
}

void
Circle::SetParameterTrim(double start, double end) {
	double startpoint[3];
	double endpoint[3];

	t = start*LocalUnits::planeangle;
	s = end*LocalUnits::planeangle;

	if (s < t) {
		t = t - 2*ON_PI;
	}
	ON_3dPoint origin=GetOrigin();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();
	ON_Plane p(origin,xaxis,yaxis);

	// Creates a circle parallel to the plane
	// with given center and radius.
	ON_Circle c(p,origin,radius);

	ON_3dPoint P = c.PointAt(t);
	startpoint[0] = P.x;
	startpoint[1] = P.y;
	startpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	P = c.PointAt(s);
	endpoint[0] = P.x;
	endpoint[1] = P.y;
	endpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	SetPointTrim(startpoint, endpoint);
}

bool
Circle::LoadONBrep(ON_Brep *brep)
{
	// this curve may be used in multiple trimming instances
	//if (ON_id >= 0)
	//	return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin*LocalUnits::length;

	ON_Plane p(origin,xaxis,yaxis);

	// Creates a circle parallel to the plane
	// with given center and radius.
	ON_Circle c(p,origin,radius*LocalUnits::length);
	ON_Curve *curve = NULL;
	if (trimmed) { //explicitly trimmed
		if (parameter_trim) {
			//load params
			if (s < t) {
				t = t - 2*ON_PI;
			}
			//TODO: lookat ellipse code seems simpler
			ON_Interval i(t,s);
			ON_Arc a(c,i);
			ON_ArcCurve *arccurve = new ON_ArcCurve(a);
			arccurve->SetDomain(t,s);
			curve = arccurve;
		} else {
			//must be point trim so calc t,s from points
			ON_3dPoint pnt1 = trim_startpoint;
			ON_3dPoint pnt2 = trim_endpoint;
			c.ClosestPointTo(pnt1,&t);
			c.ClosestPointTo(pnt2,&s);
			if (s < t) {
				t = t - 2*ON_PI;
			}
			//TODO: lookat ellipse code seems simpler
			ON_Interval i(t,s);
			ON_Arc a(c,i);
			ON_ArcCurve *arccurve = new ON_ArcCurve(a);
			arccurve->SetDomain(t,s);
			curve = arccurve;
		}
	} else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
		int v1 = start->GetONId();
		int v2 = end->GetONId();
		if (v1 != v2) {
			ON_3dPoint pnt1 = start->Point3d();
			ON_3dPoint pnt2 = end->Point3d();

			pnt1 = pnt1*LocalUnits::length;
			pnt2 = pnt2*LocalUnits::length;

			c.ClosestPointTo(pnt1,&t);
			c.ClosestPointTo(pnt2,&s);
			if (s < t) {
				t = t - 2*ON_PI;
			}
			//TODO: lookat ellipse code seems simpler
			ON_Interval i(t,s);
			ON_Arc a(c,i);
			ON_ArcCurve *arccurve = new ON_ArcCurve(a);
			arccurve->SetDomain(t,s);
			curve = arccurve;
		} else {
			ON_NurbsCurve *nurbcurve = new ON_NurbsCurve();
			if (!c.GetNurbForm(*nurbcurve)) {
				cerr << "Error: ::LoadONBrep(ON_Brep *brep) error generating NURB form of " << entityname << endl;
				return false;
			}
			curve = nurbcurve;
		}
	} else {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not endpoints for specified for curve " << entityname << endl;
		return false;
	}

	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

void
Ellipse::SetParameterTrim(double start, double end) {
	double startpoint[3];
	double endpoint[3];

	t = start*LocalUnits::planeangle;
	s = end*LocalUnits::planeangle;

	if (s < t) {
		t = t - 2*ON_PI;
	}
	ON_3dPoint origin=GetOrigin();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();
	ON_Plane p(origin,xaxis,yaxis);
	ON_Ellipse e(p,semi_axis_1,semi_axis_2);
	ON_3dPoint P = e.PointAt(t);
	startpoint[0] = P.x;
	startpoint[1] = P.y;
	startpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	P = e.PointAt(s);
	endpoint[0] = P.x;
	endpoint[1] = P.y;
	endpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	SetPointTrim(startpoint, endpoint);
}

bool
Ellipse::LoadONBrep(ON_Brep *brep)
{
	//if (ON_id >= 0)
	//	return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin*LocalUnits::length;

	ON_Plane p(origin,xaxis,yaxis);
	ON_Ellipse e(p,semi_axis_1*LocalUnits::length,semi_axis_2*LocalUnits::length);

	ON_Curve *curve;
	if (trimmed) { //explicitly trimmed
		if (parameter_trim) {
			if (s < t) {
				t = t - 2*ON_PI;
			}
			ON_Interval i(t,s);
			ON_NurbsCurve ecurve;
		    int success = e.GetNurbForm(ecurve);
		    ON_NurbsCurve *nurbcurve = new ON_NurbsCurve();
		    ecurve.GetNurbForm(*nurbcurve,0.0,&i);
			curve = nurbcurve;
		} else {
			//must be point trim so calc t,s from points
			ON_3dPoint pnt1 = trim_startpoint;
			ON_3dPoint pnt2 = trim_endpoint;

			pnt1 = pnt1*LocalUnits::length;
			pnt2 = pnt2*LocalUnits::length;
			e.ClosestPointTo(pnt1,&t);
			e.ClosestPointTo(pnt2,&s);
			if (s < t) {
				t = t - 2*ON_PI;
			}
			ON_Interval i(t,s);
			ON_NurbsCurve ecurve;
		    int success = e.GetNurbForm(ecurve);
		    ON_NurbsCurve *nurbcurve = new ON_NurbsCurve();
		    ecurve.GetNurbForm(*nurbcurve,0.0,&i);
			curve = nurbcurve;
		}
	} else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
		int v1 = start->GetONId();
		int v2 = end->GetONId();
		if (v1 != v2) {
			ON_3dPoint pnt1 = start->Point3d();
			ON_3dPoint pnt2 = end->Point3d();

			pnt1 = pnt1*LocalUnits::length;
			pnt2 = pnt2*LocalUnits::length;

			e.ClosestPointTo(pnt1,&t);
			e.ClosestPointTo(pnt2,&s);
			if (s < t) {
				t = t - 2*ON_PI;
			}
			ON_Interval i(t,s);
			ON_NurbsCurve ecurve;
		    int success = e.GetNurbForm(ecurve);
		    ON_NurbsCurve *nurbcurve = new ON_NurbsCurve();
		    ecurve.GetNurbForm(*nurbcurve,0.0,&i);
			curve = nurbcurve;
		} else {
			ON_NurbsCurve *nurbcurve = new ON_NurbsCurve();
			if (!e.GetNurbForm(*nurbcurve)) {
				cerr << "Error: ::LoadONBrep(ON_Brep *brep) error generating NURB form of " << entityname << endl;
				return false;
			}
			curve = nurbcurve;
		}
	} else {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not endpoints for specified for curve " << entityname << endl;
		return false;
	}

	ON_id = brep->AddEdgeCurve(curve);

	return true;
}

void
Hyperbola::SetParameterTrim(double start, double end) {
	double startpoint[3];
	double endpoint[3];

	t = start;
	s = end;

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin;

	ON_Plane p(origin,xaxis,yaxis);

	ON_3dPoint center = origin;
	double a = semi_axis;
	double b = semi_imag_axis;

	double e = sqrt(1.0 + (b*b)/(a*a));
	double fd = a/e;

	double theta = atan(b/a);

	double sint = sin(theta);
	double cost = cos(theta);

	if (s < t) {
		double tmp = s;
		s = t;
		t = s;
	}

	double y = b*tan(t);
	double x = a/cos(t);

	ON_3dVector X = x * xaxis;
	ON_3dVector Y = y * yaxis;
	ON_3dPoint P = center + X + Y;

	startpoint[0] = P.x;
	startpoint[1] = P.y;
	startpoint[2] = P.z;

	y = b*tan(s);
	x = a/cos(s);

	X = x * xaxis;
	Y = y * yaxis;
	P = center + X + Y;

	endpoint[0] = P.x;
	endpoint[1] = P.y;
	endpoint[2] = P.z;

	SetPointTrim(startpoint, endpoint);
}

int
intersectLines(ON_Line &l1,ON_Line &l2, ON_3dPoint &out) {
	fastf_t t,u;
	point_t p,a;
	vect_t d,c;
	struct bn_tol tol;

	tol.magic = BN_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	VMOVE(p,l1.from);
	VMOVE(a,l2.from);
	VMOVE(d,l1.Direction());
	VMOVE(c,l2.Direction());
	int i = bn_isect_line3_line3(&t, &u, p, d, a, c, &tol);
	if (i == 1) {
		VMOVE(out,l1.from);
		out = out + t*l1.Direction();
	}
	return i;
}

bool
Hyperbola::LoadONBrep(ON_Brep *brep)
{
	   ON_TextLog dump;

	   //if (ON_id >= 0)
		//	return true; // already loaded

		ON_3dPoint origin=GetOrigin();
		ON_3dVector norm=GetNormal();
		ON_3dVector xaxis=GetXAxis();
		ON_3dVector yaxis=GetYAxis();

		origin = origin*LocalUnits::length;

		ON_Plane p(origin,xaxis,yaxis);

		ON_3dPoint center = origin;
		double a = semi_axis*LocalUnits::length;
		double b = semi_imag_axis*LocalUnits::length;

		double eccentricity = sqrt(1.0 + (b*b)/(a*a));
		double e = eccentricity;
		ON_3dPoint vertex = center + a*xaxis;
		ON_3dPoint directrix = center + (a/eccentricity)*xaxis;
		ON_3dPoint focus = center + (eccentricity*a)*xaxis;
		ON_3dPoint focusprime = center - (eccentricity*a)*xaxis;

		ON_3dPoint pnt1;
		ON_3dPoint pnt2;
		if (trimmed) { //explicitly trimmed
			if (parameter_trim) {
				if (s < t) {
					double tmp = s;
					s = t;
					t = s;
				}
				//TODO: check sense agreement
			} else {
				//must be point trim so calc t,s from points
				pnt1 = trim_startpoint;
				pnt2 = trim_endpoint;

				pnt1 = pnt1*LocalUnits::length;
				pnt2 = pnt2*LocalUnits::length;

				ON_3dVector fp = pnt1 - focus;
				double ydot = fp*yaxis;
				t = atan2(ydot,b);

				fp = pnt2 - focus;
				ydot = fp*yaxis;
				s = atan2(ydot,b);

				if (s < t) {
					double tmp = s;
					s = t;
					t = s;
				}
			}
		} else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
			pnt1 = start->Point3d();
			pnt2 = end->Point3d();

			pnt1 = pnt1*LocalUnits::length;
			pnt2 = pnt2*LocalUnits::length;

			ON_3dVector fp = pnt1 - focus;
			double ydot = fp*yaxis;
			t = atan2(ydot,b);

			fp = pnt2 - focus;
			ydot = fp*yaxis;
			s = atan2(ydot,b);

			if (s < t) {
				double tmp = s;
				s = t;
				t = s;
			}
		} else {
			cerr << "Error: ::LoadONBrep(ON_Brep *brep) not endpoints for specified for curve " << entityname << endl;
			return false;
		}
#ifdef HYPERBOLA_EXTENTS
		double extent = MAX(fabs(t),fabs(s));
		double x = a*tan(extent);
		double y = sqrt(a*a * (1 + (x*x)/(b*b)));

		ON_3dVector X = x * yaxis;
		ON_3dVector Y = y * xaxis;
		ON_3dPoint P0 = center - X + Y;
		ON_3dPoint P1 = directrix;
		ON_3dPoint P2 = center + X + Y;


		// calc tang intersect with transverse axis
		ON_3dVector ToFocus;
		ToFocus.x = -x;
		ToFocus.y = (eccentricity*a) - y;
		ToFocus.z = 0.0;
		ToFocus.Unitize();

		ON_3dVector ToFocusPrime;
		ToFocusPrime.x = -x;
		ToFocusPrime.y = -(eccentricity*a) - y;
		ToFocusPrime.z = 0.0;
		ToFocusPrime.Unitize();

		ON_3dVector bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		double isect = y - (bisector.y/bisector.x)*x;
		ON_3dPoint A = center + isect*xaxis;

		P1 = A;

		ON_3dPoint directrixY = directrix - X;
		ON_3dPoint directrixMinusY = directrix + X;
		ON_3dPoint M = center + Y;
		double mx = M.DistanceTo(vertex);
		double mp1 = M.DistanceTo(P1);
		double R = mx/mp1;
		double w = R/(1-R);

		/////
		double C = center.DistanceTo(vertex);
		double B = M.DistanceTo(vertex);
	    double intercept_calc = C*C/(B + C);
	    double intercept_dist = B + C - intercept_calc;
	    double intercept_length = intercept_dist - B;
	    bu_log("intercept_dist: %f\n", intercept_dist);
	    bu_log("intercept_length: %f\n", intercept_length);
	    double MX = B;
	    double MP = MX + intercept_length;
	    w = (MX/MP)/(1-MX/MP);

		/////
		P1 = (w)*P1; // must pre-weight before putting into NURB

		double d1 = focus.DistanceTo(P0);
		double d2 = eccentricity*directrixY.DistanceTo(P0);

/*
		x = s;
		y = sqrt(a*a * (1 + (x*x)/(b*b)));
		X = x * yaxis;
		Y = y * xaxis;
		P0 = center - X + Y;

		x = t;
		y = sqrt(a*a * (1 + (x*x)/(b*b)));
		X = x * yaxis;
		Y = y * xaxis;
		P2 = center - X + Y;
*/

//
/*
		ON_NurbsCurve* hypernurbscurve = ON_NurbsCurve::New(3,true,3,3);
		hypernurbscurve->SetKnot(0, 0);
		hypernurbscurve->SetKnot(1, 0);
		hypernurbscurve->SetKnot(2, 1);
		hypernurbscurve->SetKnot(3, 1);
		hypernurbscurve->SetCV(0,P0);
		hypernurbscurve->SetCV(1,P1);
		hypernurbscurve->SetCV(2,P2);
		//hypernurbscurve->MakeRational();
		hypernurbscurve->SetWeight(0,1.0);
		hypernurbscurve->SetWeight(1,w);
		hypernurbscurve->SetWeight(2,1.0);
		//hypernurbscurve->ClampEnd(2);
*/


		// add hyperbola weightings
	    ON_3dPointArray cpts(3);
	    cpts.Append(P0);
	    cpts.Append(P1);
	    cpts.Append(P2);
	    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
	    bcurve->MakeRational();
	    bcurve->SetWeight(1,w);

/*
		double tp,sp;
		bcurve->GetLocalClosestPoint(pnt1,-extent,&tp);
		bcurve->GetLocalClosestPoint(pnt2,extent,&sp);
*/

	    ON_NurbsCurve fullhypernurbscurve;
	    ON_NurbsCurve* hypernurbscurve = ON_NurbsCurve::New();

	    bcurve->GetNurbForm(fullhypernurbscurve);
	    ON_3dPoint bp1 = bcurve->PointAt(a*tan(t));
	    ON_3dPoint bp2 = bcurve->PointAt(a*tan(s));
	    double t0,t1;
	    fullhypernurbscurve.GetDomain(&t0,&t1);
	    fullhypernurbscurve.SetDomain(-x,x);
	    double xt = a*tan(t);
	    double xs = a*tan(s);
		double ys = sqrt(a*a * (1 + (xs*xs)/(b*b)));

		ON_3dVector XS = xs * yaxis;
		ON_3dVector YS = ys * xaxis;
		ON_3dPoint PS = center + XS + YS;

		ON_3dPoint p1 = fullhypernurbscurve.PointAt(a*tan(t));
	    ON_3dPoint p2 = fullhypernurbscurve.PointAt(a*tan(s));

	    double xdelta = PS.DistanceTo(p2);
		ON_3dVector fp = PS - p2;
		double dotp = fp*yaxis;

	    while ( xdelta > 0.0001) {

	    		s = atan((xs + dotp)/a);
	    		xs = a*tan(s);
	    	p2=fullhypernurbscurve.PointAt(a*tan(s));
	    	xdelta = PS.DistanceTo(p2);
	    	fp = PS - p2;
	    	dotp = fp*yaxis;
	    }
	    ON_Interval subdomain(a*tan(t),a*tan(s));
	    fullhypernurbscurve.GetNurbForm(*hypernurbscurve,0.000001,&subdomain);

	    //fullhypernurbscurve.GetNurbForm(*hypernurbscurve);
	    //fullhypernurbscurve.CloseTo(pnt1,0.0001);
#else
#if TRY1
		double yt = b*tan(t);
		double xt = a/cos(t);

		double ys = b*tan(s);
		double xs = a/cos(s);

		ON_3dVector X = xt * xaxis;
		ON_3dVector Y = yt * yaxis;
		ON_3dPoint P0 = center + X + Y;
		X = xs * xaxis;
		Y = ys * yaxis;
		ON_3dPoint P2 = center + X + Y;

		// calc tangent P0,P2 intersection
		ON_3dVector ToFocus = focus - P0;
		ToFocus.Unitize();

		ON_3dVector ToFocusPrime = focusprime - P0;
		ToFocusPrime.Unitize();

		ON_3dVector bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		ON_Line bs0(P0,P0 + bisector);

		ToFocus = focus - P2;
		ToFocus.Unitize();

		ToFocusPrime = focusprime - P2;
		ToFocusPrime.Unitize();

		bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		ON_Line bs2(P2,P2 + bisector);
		ON_3dPoint P1;
		if (intersectLines(bs0,bs2,P1) != 1) {
			cerr << entityname << ": Error: Control point can not be calculated." << endl;
			return false;
		}

		ON_Line l1(focus,P1);
		ON_Line l2(P0,P2);
		ON_3dPoint M;

		if (intersectLines(l1,l2,M) != 1) {
			cerr << entityname << ": Error: Control point can not be calculated." << endl;
			return false;
		}
		ON_3dVector v1;
		v1 = l1.Direction();
		v1.Unitize();
		double ydot = v1*yaxis;
		double xdot = v1*xaxis;
		double ang = atan2(ydot,xdot);
		double r = a*(e*e - 1)/(1 - e*cos(ang));
		double ar = focus.DistanceTo(P0);
		double x = e*a + r*cos(ang);
		double y = b*sqrt((x*x)/(a*a)-1.0);
		if (ydot < 0.0)
			y *= -1.0;

		X = x * xaxis;
		Y = y * yaxis;
		ON_3dPoint Pv = center + X + Y;
		double mx = M.DistanceTo(Pv);
		double mp1 = M.DistanceTo(P1);
		double R = mx/mp1;
		double w = R/(1-R);

/*
		/////
		double C = center.DistanceTo(vertex);
		double B = M.DistanceTo(vertex);
	    double intercept_calc = C*C/(B + C);
	    double intercept_dist = B + C - intercept_calc;
	    double intercept_length = intercept_dist - B;
	    bu_log("intercept_dist: %f\n", intercept_dist);
	    bu_log("intercept_length: %f\n", intercept_length);
	    double MX = B;
	    double MP = MX + intercept_length;
	    w = (MX/MP)/(1-MX/MP);
*/

		/////
		P1 = (w)*P1; // must pre-weight before putting into NURB
#else
#ifdef try2
		double yt = b*tan(t);
		double xt = a/cos(t);

		double ys = b*tan(s);
		double xs = a/cos(s);

		ON_3dVector X = xt * xaxis;
		ON_3dVector Y = yt * yaxis;
		ON_3dPoint P0 = center + X + Y;
		X = xs * xaxis;
		Y = ys * yaxis;
		ON_3dPoint P2 = center + X + Y;

		ON_3dPoint maxP;
		if (fabs(t) > fabs(s)) {
			maxP = P0;
		} else {
			maxP = P2;
		}
		// calc tangent P0,P2 intersection
		ON_3dVector ToFocus = focus - maxP;
		ToFocus.Unitize();

		ON_3dVector ToFocusPrime = focusprime - maxP;
		ToFocusPrime.Unitize();

		ON_3dVector bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		ON_Line bs0(maxP,maxP + bisector);
		ON_Line centerline(center,focus);

		ON_3dPoint P1;
		if (intersectLines(bs0,centerline,P1) != 1) {
			cerr << entityname << ": Error: Control point can not be calculated." << endl;
			return false;
		}

		ON_Line l1(focus,P1);
		ON_Line l2(P0,P2);
		ON_3dPoint M;

		if (intersectLines(l1,l2,M) != 1) {
			cerr << entityname << ": Error: Control point can not be calculated." << endl;
			return false;
		}
		ON_3dVector v1;
		v1 = l1.Direction();
		v1.Unitize();
		double ydot = v1*yaxis;
		double xdot = v1*xaxis;
		double ang = atan2(ydot,xdot);
		double r = a*(e*e - 1)/(1 - e*cos(ang));
		double ar = focus.DistanceTo(P0);
		double x = e*a + r*cos(ang);
		double y = b*sqrt((x*x)/(a*a)-1.0);
		if (ydot < 0.0)
			y *= -1.0;

		X = x * xaxis;
		Y = y * yaxis;
		ON_3dPoint Pv = center + X + Y;
		double mx = M.DistanceTo(Pv);
		double mp1 = M.DistanceTo(P1);
		double R = mx/mp1;
		double w = R/(1-R);

		P1 = (w)*P1; // must pre-weight before putting into NURB
#else
		double yt = b*tan(t);
		double xt = a/cos(t);

		double ys = b*tan(s);
		double xs = a/cos(s);

		ON_3dVector X = xt * xaxis;
		ON_3dVector Y = yt * yaxis;
		ON_3dPoint P0 = center + X + Y;
		X = xs * xaxis;
		Y = ys * yaxis;
		ON_3dPoint P2 = center + X + Y;

		// calc tangent P0,P2 intersection
		ON_3dVector ToFocus = focus - P0;
		ToFocus.Unitize();

		ON_3dVector ToFocusPrime = focusprime - P0;
		ToFocusPrime.Unitize();

		ON_3dVector bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		ON_Line bs0(P0,P0 + bisector);

		ToFocus = focus - P2;
		ToFocus.Unitize();

		ToFocusPrime = focusprime - P2;
		ToFocusPrime.Unitize();

		bisector = ToFocus + ToFocusPrime;
		bisector.Unitize();

		ON_Line bs2(P2,P2 + bisector);
		ON_3dPoint P1;
		if (intersectLines(bs0,bs2,P1) != 1) {
			cerr << entityname << ": Error: Control point can not be calculated." << endl;
			return false;
		}

		ON_Line l1(focus,P1);
		ON_Line l2(P0,P2);
		ON_3dPoint M = (P0 + P2)/2.0;

		ON_Line ctom(center,M);
		ON_3dVector dtom = ctom.Direction();
		dtom.Unitize();
		double m = (dtom*yaxis)/(dtom*xaxis);
		double x1 = a*b*sqrt(1.0/(b*b - m*m*a*a));
		double y1 = b*sqrt((x1*x1)/(a*a) - 1.0);
		if (m < 0.0) {
			y1 *= -1.0;
		}
		X = x1*xaxis;
		Y = y1*yaxis;
		ON_3dPoint Pv = center + X + Y;
		double mx = M.DistanceTo(Pv);
		double mp1 = M.DistanceTo(P1);
		double R = mx/mp1;
		double w = R/(1-R);

		P1 = (w)*P1; // must pre-weight before putting into NURB
#endif
#endif
		//double d1 = focus.DistanceTo(P0);
		//double d2 = eccentricity*directrixY.DistanceTo(P0);

/*
		x = s;
		y = sqrt(a*a * (1 + (x*x)/(b*b)));
		X = x * yaxis;
		Y = y * xaxis;
		P0 = center - X + Y;

		x = t;
		y = sqrt(a*a * (1 + (x*x)/(b*b)));
		X = x * yaxis;
		Y = y * xaxis;
		P2 = center - X + Y;
*/

//
/*
		ON_NurbsCurve* hypernurbscurve = ON_NurbsCurve::New(3,true,3,3);
		hypernurbscurve->SetKnot(0, 0);
		hypernurbscurve->SetKnot(1, 0);
		hypernurbscurve->SetKnot(2, 1);
		hypernurbscurve->SetKnot(3, 1);
		hypernurbscurve->SetCV(0,P0);
		hypernurbscurve->SetCV(1,P1);
		hypernurbscurve->SetCV(2,P2);
		//hypernurbscurve->MakeRational();
		hypernurbscurve->SetWeight(0,1.0);
		hypernurbscurve->SetWeight(1,w);
		hypernurbscurve->SetWeight(2,1.0);
		//hypernurbscurve->ClampEnd(2);
*/


		// add hyperbola weightings
	    ON_3dPointArray cpts(3);
	    cpts.Append(P0);
	    cpts.Append(P1);
	    cpts.Append(P2);
	    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
	    bcurve->MakeRational();
	    bcurve->SetWeight(1,w);

/*
		double tp,sp;
		bcurve->GetLocalClosestPoint(pnt1,-extent,&tp);
		bcurve->GetLocalClosestPoint(pnt2,extent,&sp);
*/

	    ON_NurbsCurve fullhypernurbscurve;
	    ON_NurbsCurve* hypernurbscurve = ON_NurbsCurve::New();

	    bcurve->GetNurbForm(fullhypernurbscurve);
	    ON_3dPoint bp1 = bcurve->PointAt(a*tan(t));
	    ON_3dPoint bp2 = bcurve->PointAt(a*tan(s));
	    double t0,t1;
	    fullhypernurbscurve.GetDomain(&t0,&t1);
/*
	    fullhypernurbscurve.SetDomain(xt,xs);
	    xt = a*tan(t);
	    xs = a*tan(s);
		ys = sqrt(a*a * (1 + (xs*xs)/(b*b)));

		ON_3dVector XS = xs * yaxis;
		ON_3dVector YS = ys * xaxis;
		ON_3dPoint PS = center + XS + YS;

		ON_3dPoint p1 = fullhypernurbscurve.PointAt(a*tan(t));
	    ON_3dPoint p2 = fullhypernurbscurve.PointAt(a*tan(s));

	    double xdelta = PS.DistanceTo(p2);
		ON_3dVector fp = PS - p2;
		double dotp = fp*yaxis;

	    while ( xdelta > 0.0001) {

	    		s = atan((xs + dotp)/a);
	    		xs = a*tan(s);
	    	p2=fullhypernurbscurve.PointAt(a*tan(s));
	    	xdelta = PS.DistanceTo(p2);
	    	fp = PS - p2;
	    	dotp = fp*yaxis;
	    }
	    ON_Interval subdomain(a*tan(t),a*tan(s));
*/
	    //fullhypernurbscurve.GetNurbForm(*hypernurbscurve,0.000001,&subdomain);
	    fullhypernurbscurve.GetNurbForm(*hypernurbscurve);

	    //fullhypernurbscurve.GetNurbForm(*hypernurbscurve);
	    //fullhypernurbscurve.CloseTo(pnt1,0.0001);
#endif

		ON_id = brep->AddEdgeCurve(hypernurbscurve);
		cerr << "ON_id: " << ON_id << endl;
		//cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
		return true;
}

void
Parabola::SetParameterTrim(double start, double end) {
	double startpoint[3];
	double endpoint[3];

	t = start;
	s = end;

	ON_3dPoint center=GetOrigin();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	double fd = focal_dist;

	if (s < t) {
		double tmp = s;
		s = t;
		t = s;
	}
	double x = 2.0*fd*t; // tan(t);
	double y = (x*x)/(4*fd);

	ON_3dVector X = x * yaxis;
	ON_3dVector Y = y * xaxis;
	ON_3dPoint P = center - X + Y;

	startpoint[0] = P.x;
	startpoint[1] = P.y;
	startpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	x = 2.0*fd*s; //tan(s);
	y = (x*x)/(4*fd);

	X = x * yaxis;
	Y = y * xaxis;
	P = center - X + Y;

	endpoint[0] = P.x;
	endpoint[1] = P.y;
	endpoint[2] = P.z;

	//TODO: debugging
	P = P * LocalUnits::length;

	SetPointTrim(startpoint, endpoint);
}

bool
Parabola::LoadONBrep(ON_Brep *brep)
{

    ON_TextLog dump;

	//if (ON_id >= 0)
	//	return true; // already loaded

	ON_3dPoint origin=GetOrigin();
	ON_3dVector norm=GetNormal();
	ON_3dVector xaxis=GetXAxis();
	ON_3dVector yaxis=GetYAxis();

	origin = origin*LocalUnits::length;

	ON_Plane p(origin,xaxis,yaxis);

	//  Next, create a parabolic NURBS curve
	ON_3dPoint vertex = origin;

	double fd = focal_dist*LocalUnits::length;
	ON_3dPoint focus = vertex + fd*xaxis;
	ON_3dPoint directrix = vertex - fd*xaxis;
	double eccentricity = 1.0; // for parabola eccentricity is always 1.0

	if (trimmed) { //explicitly trimmed
		if (parameter_trim) {
			if (s < t) {
				double tmp = s;
				s = t;
				t = s;
			}
		} else {
			//must be point trim so calc t,s from points
			ON_3dPoint pnt1 = trim_startpoint;
			ON_3dPoint pnt2 = trim_endpoint;

			pnt1 = pnt1*LocalUnits::length;
			pnt2 = pnt2*LocalUnits::length;

			ON_3dVector fp = pnt1 - focus;
			double dotp = fp*yaxis;

			double F = pnt1.DistanceTo(focus);
			double D = F/eccentricity;
			double H = pnt1.DistanceTo(directrix);
			double x = sqrt(H*H - D*D);
			//t  = atan(x/fd);
			t = x / (2.0 * fd);

			if (dotp < 0.0)
				t *=-1.0;

			fp = pnt2 - focus;
			dotp = fp*yaxis;
			F = pnt2.DistanceTo(focus);
			D = F/eccentricity;
			H = pnt2.DistanceTo(directrix);
			x = sqrt(H*H - D*D);
			//s  = atan(x/fd);
			s = x / (2.0 * fd);

			if (dotp < 0.0)
				s *=-1.0;

			if (s < t) {
				double tmp = s;
				s = t;
				t = s;
			}
		}
	} else if ((start != NULL) && (end != NULL)){ //not explicit let's try edge vertices
		ON_3dPoint pnt1 = start->Point3d();
		ON_3dPoint pnt2 = end->Point3d();

		pnt1 = pnt1*LocalUnits::length;
		pnt2 = pnt2*LocalUnits::length;

		ON_3dVector fp = pnt1 - focus;
		double dotp = fp*yaxis;

		double F = pnt1.DistanceTo(focus);
		double D = F/eccentricity;
		double H = pnt1.DistanceTo(directrix);
		double x = sqrt(H*H - D*D);
		//t  = atan(x/fd);
		t = x / (2.0 * fd);

		if (dotp < 0.0)
			t *=-1.0;

		fp = pnt2 - focus;
		dotp = fp*yaxis;
		F = pnt2.DistanceTo(focus);
		D = F/eccentricity;
		H = pnt2.DistanceTo(directrix);
		x = sqrt(H*H - D*D);
		//s  = atan(x/fd);
		s = x / (2.0 * fd);

		if (dotp < 0.0)
			s *=-1.0;

		if (s < t) {
			double tmp = s;
			s = t;
			t = s;
		}
	} else {
		cerr << "Error: ::LoadONBrep(ON_Brep *brep) not endpoints for specified for curve " << entityname << endl;
		return false;
	}
	double extent = MAX(fabs(t),fabs(s));
	double x = 2.0*fd*extent;
	double y = (x*x)/(4*fd);

	ON_3dVector X = x * yaxis;
	ON_3dVector Y = y * xaxis;
	ON_3dPoint P0 = vertex - X + Y;
	ON_3dPoint P1 = vertex;
	ON_3dPoint P2 = vertex + X + Y;

	// calc tang intersect with transverse axis
	ON_3dVector ToFocus;
	ToFocus.x = -x;
	ToFocus.y = fd - y;
	ToFocus.z = 0.0;
	ToFocus.Unitize();

	ON_3dVector ToDirectrix;
	ToDirectrix.x = 0.0;
	ToDirectrix.y = -fd - y;
	ToDirectrix.z = 0.0;
	ToDirectrix.Unitize();

	ON_3dVector bisector = ToFocus + ToDirectrix;
	bisector.Unitize();

	double isect = y - (bisector.y/bisector.x)*x;
	ON_3dPoint A = vertex + isect*xaxis;

	P1 = A;

	// make parabola from bezier
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();

    ON_NurbsCurve fullparabnurbscurve;
    ON_NurbsCurve* parabnurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(fullparabnurbscurve);
    fullparabnurbscurve.SetDomain(-extent,extent);
    ON_Interval subdomain(t,s);
    fullparabnurbscurve.GetNurbForm(*parabnurbscurve,0.0,&subdomain);

	ON_id = brep->AddEdgeCurve(parabnurbscurve);

	return true;
}

bool
Line::LoadONBrep(ON_Brep *brep)
{
	//if (ON_id >= 0)
	//	return true; // already loaded

	ON_3dPoint startpnt=start->Point3d();
	ON_3dPoint endpnt=end->Point3d();

	startpnt=startpnt*LocalUnits::length;
	endpnt = endpnt*LocalUnits::length;

	ON_LineCurve *l = new ON_LineCurve(startpnt,endpnt);

	ON_id = brep->AddEdgeCurve(l);

	return true;
}

bool
SurfaceOfLinearExtrusion::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

	if (ON_id >= 0)
		return true; // already loaded

	// load parent class
	if (!SweptSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	ON_Curve *curve = brep->m_C3[swept_curve->GetONId()];

	ON_3dPoint dir = extrusion_axis->Orientation();
	double mag = extrusion_axis->Magnitude()*LocalUnits::length;
	ON_3dPoint startpnt = swept_curve->PointAtStart();

	startpnt = startpnt*LocalUnits::length;

	ON_3dPoint extrusion_endpnt = startpnt + mag*dir;
	ON_3dPoint extrusion_startpnt = startpnt;

	ON_LineCurve *l = new ON_LineCurve(extrusion_startpnt,extrusion_endpnt);

	// the following extrude code lifted from OpenNURBS ON_BrepExtrude()
	ON_Line path_line;
	path_line.from = extrusion_startpnt;
	path_line.to = extrusion_endpnt;
	ON_3dVector path_vector = path_line.Direction();
	if( path_vector.IsZero() )
		return false;

	ON_SumSurface* sum_srf = 0;

	ON_Curve* srf_base_curve = brep->m_C3[swept_curve->GetONId()]->Duplicate();
	ON_3dPoint sum_basepoint = -ON_3dVector(l->PointAtStart());
	sum_srf = new ON_SumSurface();
	sum_srf->m_curve[0] = srf_base_curve;
	sum_srf->m_curve[1] = l; //srf_path_curve;
	sum_srf->m_basepoint = sum_basepoint;
	sum_srf->BoundingBox(); // fills in sum_srf->m_bbox

	if ( !sum_srf )
		return false;

	ON_id = brep->AddSurface(sum_srf);

	return true;
}

bool
SurfaceOfRevolution::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

	if (ON_id >= 0)
		return true; // already loaded

	// load parent class
	if (!SweptSurface::LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	ON_3dPoint start = axis_position->GetOrigin();
	ON_3dVector dir = axis_position->GetNormal();
	ON_3dPoint end = start + dir;

	ON_Line axisline(start,end);
    ON_RevSurface* revsurf = ON_RevSurface::New();
	revsurf->m_curve = brep->m_C3[swept_curve->GetONId()]->DuplicateCurve();
	revsurf->m_axis = axisline;
	revsurf->BoundingBox(); // fills in sum_srf->m_bbox

	//ON_Brep* b = ON_BrepRevSurface( revsurf, true, true);

	if ( !revsurf )
		return false;

	ON_id = brep->AddSurface(revsurf);

	//cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
	return true;
}

bool
SweptSurface::LoadONBrep(ON_Brep *brep)
{
	if (ON_id >= 0)
		return true; // already loaded

	if (!swept_curve->LoadONBrep(brep)) {
		cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << endl;
		return false;
	}

	return true;
}


bool
VertexLoop::LoadONBrep(ON_Brep *brep)
{
	//load vertex
	loop_vertex->LoadONBrep(brep);

	ON_3dPoint vertex = loop_vertex->Point3d();

	// create singular trim;
	ON_BrepLoop& loop = brep->m_L[ON_loop_index];
	ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face->SurfaceOf();

	ON_Interval U = surface->Domain(0);
	ON_Interval V = surface->Domain(1);
	ON_3dPoint corner[4];
	corner[0] = surface->PointAt(U.m_t[0],V.m_t[0]);
	corner[1] = surface->PointAt(U.m_t[1],V.m_t[0]);
	corner[2] = surface->PointAt(U.m_t[0],V.m_t[1]);
	corner[3] = surface->PointAt(U.m_t[1],V.m_t[1]);

	ON_2dPoint start,end;
	ON_Surface::ISO iso;
    if ( VAPPROXEQUAL(vertex,corner[0],POINT_CLOSENESS_TOLERANCE) ) {
    	start = ON_2dPoint(U.m_t[0],V.m_t[0]);
        if ( VAPPROXEQUAL(vertex,corner[1],POINT_CLOSENESS_TOLERANCE) ) {
          	//south;
        	end = ON_2dPoint(U.m_t[1],V.m_t[0]);
        	iso = ON_Surface::S_iso;
        } else if ( VAPPROXEQUAL(vertex,corner[2],POINT_CLOSENESS_TOLERANCE) ) {
        	//west
        	end = ON_2dPoint(U.m_t[0],V.m_t[1]);
        	iso = ON_Surface::W_iso;
        }
    } else if ( VAPPROXEQUAL(vertex,corner[1],POINT_CLOSENESS_TOLERANCE) ) {
    	start = ON_2dPoint(U.m_t[1],V.m_t[0]);
        if ( VAPPROXEQUAL(vertex,corner[3],POINT_CLOSENESS_TOLERANCE) ) {
        	//east
        	end = ON_2dPoint(U.m_t[1],V.m_t[1]);
        	iso = ON_Surface::E_iso;
        }
    } else if ( VAPPROXEQUAL(vertex,corner[2],POINT_CLOSENESS_TOLERANCE) ) {
    	start = ON_2dPoint(U.m_t[0],V.m_t[1]);
    	if ( VAPPROXEQUAL(vertex,corner[3],POINT_CLOSENESS_TOLERANCE) ) {
    	    //north
        	end = ON_2dPoint(U.m_t[1],V.m_t[1]);
        	iso = ON_Surface::N_iso;
    	}
    }
	ON_Curve* c2d = new ON_LineCurve(start,end);
	int trimCurve = brep->m_C2.Count();
	brep->m_C2.Append(c2d);

	ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[loop_vertex->GetONId()],loop,iso,trimCurve);

	return true;
}
