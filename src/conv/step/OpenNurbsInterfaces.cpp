/*                 OpenNurbsInterfaces.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file step/OpenNurbsInterfaces.cpp
 *
 * Routines to convert STEP "OpenNurbsInterfaces" to BRL-CAD BREP
 * structures.
 *
 */

#include "common.h"

#include "opennurbs.h"

#include "sdai.h"
class SCLP23(Application_instance);

/* must come after nist step headers */
#include "brep.h"
#include "nurb.h"

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
#include "SphericalSurface.h"
#include "ToroidalSurface.h"
#include "UniformSurface.h"

#include "AdvancedBrepShapeRepresentation.h"
#include "PullbackCurve.h"


/* FIXME: should not be peeking into a private header and cannot be a
 * public header (of librt).
 */
#include "../../librt/opennurbs_ext.h"


ON_Brep *
AdvancedBrepShapeRepresentation::GetONBrep()
{
    ON_Brep *brep = ON_Brep::New();

    if (!brep) {
	std::cerr << "ERROR: INTERNAL MEMORY ALLOCATION FAILURE in " << __FILE__ << ":" << __LINE__ << std::endl;
	return NULL;
    }

    if (!LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::GetONBrep() - Error loading openNURBS brep." << std::endl;
	//still return brep may contain something useful to diagnose
	return brep;
    }

    return brep;
}


bool
AdvancedBrepShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    LIST_OF_REPRESENTATION_ITEMS::iterator i;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    for (i = items.begin(); i != items.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }
    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end()))
		multiplicity = degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	// knot index (>= 0 and < Order + CV_count - 2)
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if ((multiplicity > degree) || (n == knot_multiplicities.end()))
		multiplicity = degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_POINTS::iterator i;
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	curve->SetCV(cv_index, ON_3dPoint((*i)->X() * LocalUnits::length, (*i)->Y() * LocalUnits::length, (*i)->Z() * LocalUnits::length));
	cv_index++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	curve->SetKnot(knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	curve->SetKnot(i, 1.0);
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    int t_size = control_points_list.size();

    ON_NurbsCurve* curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    if (closed_curve == 1) {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int multiplicity = (*m);
	double knot_value = (*r);

	if ((multiplicity < degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;
	    multiplicity = (*m);
	    knot_value = (*r);
	    if (n == knot_multiplicities.end() && (multiplicity < degree) && (knot_value > 1.0)) {
		break;
	    }
	    if ((multiplicity > degree) || (n == knot_multiplicities.end()))
		multiplicity = degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = knot_multiplicities.begin();
	LIST_OF_REALS::iterator r = knots.begin();
	int knot_index = 0;
	while (m != knot_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if (multiplicity > degree)
		multiplicity = degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		curve->SetKnot(knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }

    LIST_OF_POINTS::iterator i;
    LIST_OF_REALS::iterator r = weights_data.begin();
    int cv_index = 0;
    for (i = control_points_list.begin(); i != control_points_list.end(); ++i) {
	double w = (*r);
	curve->SetCV(cv_index, ON_4dPoint((*i)->X() * LocalUnits::length * w, (*i)->Y() * LocalUnits::length * w, (*i)->Z() * LocalUnits::length * w, w));
	cv_index++;
	r++;
    }

    ON_id = brep->AddEdgeCurve(curve);

    return true;
}


bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
    }
    return true;
}


bool
UniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true;

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add bezier surface
    //ON_BezierSurface* surf = ON_BezierSurface::New(3, false, u_degree+1, v_degree+1);
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
BSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface* surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
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

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface* surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    if (u_closed == 1) {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < u_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == this->u_multiplicities.end() && (multiplicity < u_degree) && (knot_value > 1.0)) {
		break;
	    }

	    if (multiplicity > u_degree)
		multiplicity = u_degree;
	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
	LIST_OF_REALS::iterator r = u_knots.begin();
	int knot_index = 0;
	while (m != u_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if (multiplicity > u_degree)
		multiplicity = u_degree;
	    for (int j = 0; j < multiplicity; j++) {
		surf->SetKnot(0, knot_index++, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    if (v_closed == 1) {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();

	int multiplicity = (*m);
	double knot_value = (*r);
	if ((multiplicity < v_degree) && (knot_value < 0.0)) {
	    //skip fist multiplicity and knot value
	    m++;
	    r++;
	}
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    LIST_OF_INTEGERS::iterator n = m;
	    n++;

	    multiplicity = (*m);
	    knot_value = (*r);

	    if (n == v_multiplicities.end() && (multiplicity < v_degree) && (knot_value > 1.0)) {
		break;
	    }

	    if (multiplicity > v_degree)
		multiplicity = v_degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    } else {
	LIST_OF_INTEGERS::iterator m = v_multiplicities.begin();
	LIST_OF_REALS::iterator r = v_knots.begin();
	int knot_index = 0;
	while (m != v_multiplicities.end()) {
	    int multiplicity = (*m);
	    double knot_value = (*r);
	    if (multiplicity > v_degree)
		multiplicity = v_degree;
	    for (int j = 0; j < multiplicity; j++, knot_index++) {
		surf->SetKnot(1, knot_index, knot_value);
	    }
	    r++;
	    m++;
	}
    }
    LIST_OF_LIST_OF_POINTS::iterator i;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j) {
	    surf->SetCV(u, v, ON_3dPoint((*j)->X() * LocalUnits::length, (*j)->Y() * LocalUnits::length, (*j)->Z() * LocalUnits::length));
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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalBezierSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //TODO: add rational bezier surface
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
    return false;
}


bool
RationalBSplineSurface::LoadONBrep(ON_Brep *brep)
{
    int u_size = control_points_list->size();
    int v_size = (*control_points_list->begin())->size();

    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_NurbsSurface* surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(0, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(0, i, 1.0);
    }
    // generate v-knots
    n = v_size;
    p = v_degree;
    m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(1, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double) j / (double) (n - p);
	int knot_index = j + p - 1;
	surf->SetKnot(1, knot_index, x);
    }
    for (int i = m - p; i < m; i++) {
	surf->SetKnot(1, i, 1.0);
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    LIST_OF_REALS::iterator r;
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *pnts = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = pnts->begin(); j != pnts->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
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

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (self_intersect) {
    }
    if (u_closed) {
    }
    if (v_closed) {
    }

    ON_NurbsSurface* surf = ON_NurbsSurface::New(3, true, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);
	if (multiplicity > u_degree)
	    multiplicity = u_degree;
	for (int j = 0; j < multiplicity; j++, knot_index++) {
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
	if (multiplicity > v_degree)
	    multiplicity = v_degree;
	for (int j = 0; j < multiplicity; j++) {
	    surf->SetKnot(1, knot_index++, knot_value);
	}
	r++;
	m++;
    }

    LIST_OF_LIST_OF_POINTS::iterator i = control_points_list->begin();
    LIST_OF_LIST_OF_REALS::iterator w = weights_data.begin();
    int u = 0;
    for (i = control_points_list->begin(); i != control_points_list->end(); ++i) {
	LIST_OF_POINTS::iterator j;
	LIST_OF_POINTS *p = *i;
	r = (*w)->begin();
	int v = 0;
	for (j = p->begin(); j != p->end(); ++j, r++, v++) {
	    double weight = (*r);
	    surf->SetCV(u, v, ON_4dPoint((*j)->X() * LocalUnits::length * weight, (*j)->Y() * LocalUnits::length * weight, (*j)->Z() * LocalUnits::length * weight, weight));
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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
RationalUniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!RationalBSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


bool
UniformSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (!BSplineSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}
void
FaceSurface::AddFace(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // need edge bounds to determine extents for some of the infinitely
    // defined surfaces like cones/cylinders/planes
    ON_BoundingBox *bb = GetEdgeBounds(brep);
    if (!bb) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error calculating openNURBS brep bounds." << std::endl;
	return false;
    }

    face_geometry->SetCurveBounds(bb);

    if (!face_geometry->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    AddFace(brep);

    //TODO: remove debugging code
    if ((false) && (ON_id == 72)) {
	std::cerr << "Debug:LoadONBrep for FaceSurface:" << ON_id << std::endl;
    }
    if (!Face::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    return true;
}


void
Point::AddVertex(ON_Brep *UNUSED(brep))
{
    std::cerr << "Warning: " << entityname << "::AddVertex() should be overridden by parent class." << std::endl;
}


void
CartesianPoint::AddVertex(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (vertex_index < 0) {
	ON_3dPoint p(coordinates[0] * LocalUnits::length, coordinates[1] * LocalUnits::length, coordinates[2] * LocalUnits::length);
	ON_BrepVertex& v = brep->NewVertex(p);
	vertex_index = v.m_vertex_index;
	v.m_tolerance = 1e-3;
	ON_id = v.m_vertex_index;
    }
}


void
BSplineCurve::AddPolyLine(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return;
    }

    if (ON_id < 0) {
	int num_control_points = control_points_list.size();

	if ((degree == 1) && (num_control_points >= 2)) {
	    LIST_OF_POINTS::iterator i = control_points_list.begin();
	    CartesianPoint *cp = (*i);
	    while ((++i) != control_points_list.end()) {
		ON_3dPoint start_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		cp = (*i);
		ON_3dPoint end_point(cp->X() * LocalUnits::length, cp->Y() * LocalUnits::length, cp->Z() * LocalUnits::length);
		ON_LineCurve* line = new ON_LineCurve(ON_3dPoint(start_point), ON_3dPoint(end_point));
		brep->m_C3.Append(line);
	    }
	} else if (num_control_points > 2) {
	    ON_NurbsCurve* c = ON_NurbsCurve::New(3, false, degree + 1, num_control_points);
	    /* FIXME: do something with c */
	    delete c;
	} else {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading polyLine." << std::endl;
	}
    }
}


void
VertexPoint::AddVertex(ON_Brep *brep)
{
    vertex_geometry->AddVertex(brep);
}


ON_BoundingBox *
Face::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;
    LIST_OF_FACE_BOUNDS::iterator i;
    for (i = bounds.begin(); i != bounds.end(); i++) {
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
    for (i = bounds.rbegin(); i != bounds.rend(); i++) {
	(*i)->SetFaceIndex(ON_id);
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
	cnt++;
    }
    return true;
}


bool
FaceOuterBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    SetOuter();

    if (!FaceBound::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    return true;
}


ON_BoundingBox *
FaceBound::GetEdgeBounds(ON_Brep *brep)
{
    return bound->GetEdgeBounds(brep);
}


bool
FaceBound::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

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
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!Oriented()) {
	ON_BrepLoop& loop = brep->m_L[ON_id];
	brep->FlipLoop(loop);
    }
/* lets use the cues from STEP (Oriented) for now but leave this here
 * in case we have to go back to brute force

 if (IsInner()) {
 ON_BrepLoop& loop = brep->m_L[ON_id];
 if (brep->LoopDirection((const ON_BrepLoop&) loop) > 0) {
 brep->FlipLoop(loop);
 }
 } else {
 ON_BrepLoop& loop = brep->m_L[ON_id];
 if (brep->LoopDirection((const ON_BrepLoop&) loop) < 0) {
 brep->FlipLoop(loop);
 }
 }
*/

    return true;
}

bool
EdgeCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; // already loaded

    if ((false) && (brep->m_E.Count() == 484)) {
	std::cerr << "Debug:LoadONBrep for EdgeCurve:" << brep->m_E.Count() << std::endl;
    }

    Vertex *start = NULL;
    Vertex *end = NULL;
    if (same_sense == 1) {
	start = edge_start;
	end = edge_end;
    } else {
	start = edge_end;
	end = edge_start;
    }
    edge_geometry->Start(start);
    edge_geometry->End(end);

    if (!edge_geometry->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_BrepEdge& edge = brep->NewEdge(brep->m_V[start->GetONId()], brep->m_V[end->GetONId()], edge_geometry->GetONId());
    edge.m_tolerance = 1e-3; //TODO: - need tolerance definition or constant
    ON_id = edge.m_edge_index;
    if (same_sense != 1) {
	edge.Reverse();
    }

    return true;
}

bool
OrientedEdge::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; //already loaded

    if ((false) && (brep->m_E.Count() == 5)) {
	std::cerr << "Debug:LoadONBrep for OrientedEdge:" << brep->m_E.Count() << std::endl;
    }

    if (!edge_start->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_end->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }
    if (!edge_element->LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_id = edge_element->GetONId();

    //TODO: remove debugging code
    if ((false) && (ON_id == 31)) {
	std::cerr << "Debug:LoadONBrep for OrientedEdge:" << ON_id << std::endl;
    }
    return true;
}


ON_BoundingBox *
Path::GetEdgeBounds(ON_Brep *brep)
{
    ON_BoundingBox *u = NULL;
    LIST_OF_ORIENTED_EDGES::iterator i;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
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

    if ((false) && (id == 29429)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
    }

    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	if (!(*i)->LoadONBrep(brep)) {
	    std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	    return false;
	}
    }
    //TODO: remove debugging code
    if ((false) && (id == 26089)) {
	std::cerr << "Debug:LoadONBrep for Path:" << id << std::endl;
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
    double smin, smax;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (surface->IsCone() || surface->IsCylinder()) {
	if (surface->IsClosed(0)) {
	    surface->GetDomain(0, &smin, &smax);
	} else {
	    surface->GetDomain(1, &smin, &smax);
	}

	LIST_OF_ORIENTED_EDGES::iterator i;
	for (i = edge_list.begin(); i != edge_list.end(); i++) {
	    // grab the curve for this edge, face and surface
	    const ON_BrepEdge* edge = &brep->m_E[(*i)->GetONId()];
	    const ON_Curve* curve = edge->EdgeCurveOf();
	    double tmin, tmax;
	    curve->GetDomain(&tmin, &tmax);

	    if (((tmin < 0.0) && (tmax > 0.0)) && ((tmin > smin)) || (tmax < smax)) {
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

    if (!brep) {
	/* nothing to do */
	return false;
    }

    const ON_BrepLoop* loop = &brep->m_L[ON_path_index];
    const ON_BrepFace* face = loop->Face();
    const ON_Surface* surface = face->SurfaceOf();

    // build surface tree making sure not to remove trimmed subsurfaces
    // since currently building trims and need full tree
    bool removeTrimmed = false;
    brlcad::SurfaceTree* st = new brlcad::SurfaceTree((ON_BrepFace*) face, removeTrimmed);

    //TODO: remove debugging code
    if ((false) && (id == 24894)) {
	std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
    }
    PBCData *data = NULL;
    LIST_OF_ORIENTED_EDGES::iterator prev, next;
    for (i = edge_list.begin(); i != edge_list.end(); i++) {
	// grab the curve for this edge, face and surface
	const ON_BrepEdge* edge = &brep->m_E[(*i)->GetONId()];
	const ON_Curve* curve = edge->EdgeCurveOf();
	ON_BoundingBox bb = curve->BoundingBox();
	bool orientWithCurve = (*i)->OrientWithEdge();

	if ((false) && (id == 34193)) {
	    std::cerr << "Debug:LoadONTrimmingCurves for Path:" << id << std::endl;
	}
	data = pullback_samples(st, curve);
	if (data == NULL)
	    continue;

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
    }
    // check for seams and singularities
    if (!check_pullback_data(curve_pullback_samples)) {
	std::cerr << "Error: Can not resolve seam or singularity issues." << std::endl;
    }
    list<PBCData *>::iterator cs = curve_pullback_samples.begin();
    list<PBCData *>::iterator next_cs;

    cs = curve_pullback_samples.begin();
    while (cs != curve_pullback_samples.end()) {
	next_cs = cs;
	next_cs++;
	if (next_cs == curve_pullback_samples.end())
	    next_cs = curve_pullback_samples.begin();
	data = (*cs);
	list<ON_2dPointArray*>::iterator si;
	si = data->segments.begin();
	PBCData *ndata = (*next_cs);
	list<ON_2dPointArray*>::iterator nsi;
	nsi = ndata->segments.begin();
	ON_2dPointArray* nsamples = (*nsi);

	while (si != data->segments.end()) {
	    nsi = si;
	    nsi++;
	    if (nsi == data->segments.end()) {
		PBCData *nsidata = (*next_cs);
		nsi = nsidata->segments.begin();
	    }
	    ON_2dPointArray* samples = (*si);
	    nsamples = (*nsi);

	    //TODO:Fix this shouldn't have sample counts less than 2
	    if (samples->Count() < 2) {
		si++;
		continue;
	    }
	    int trimCurve = brep->m_C2.Count();
	    //TODO: remove debugging code
	    if ((false) && (trimCurve == 68)) {
		std::cerr << "Debug:LoadONTrimmingCurves for Path:" << trimCurve << std::endl;
	    }
	    ON_Curve* c2d = interpolateCurve(*samples);
	    brep->m_C2.Append(c2d);

	    ON_BrepTrim& trim = brep->NewTrim((ON_BrepEdge&) *data->edge, data->order_reversed, (ON_BrepLoop&) *loop, trimCurve);
	    trim.m_tolerance[0] = 1e-3; // FIXME: tolerance?
	    trim.m_tolerance[1] = 1e-3;
	    ON_Interval PD = trim.ProxyCurveDomain();
	    trim.m_iso = surface->IsIsoparametric(*c2d, &PD);

	    if (!trim.IsValid(&tl)) {
		//TODO: remove debugging code
		if (false) {
		    ON_NurbsCurve nurbs_curve;
		    c2d->GetNurbForm(nurbs_curve);
		    std::cerr << "Num_knots - " << nurbs_curve.KnotCount() << std::endl;
		    std::cerr << "CV count - " << nurbs_curve.CVCount() << std::endl;
		    for (int knot_index = 0; knot_index < nurbs_curve.KnotCount(); knot_index++) {
			std::cerr << "Knot[" << knot_index << "] - " << nurbs_curve.Knot(knot_index) << std::endl;
		    }
		    for (int cv_index = 0; cv_index < nurbs_curve.CVCount(); cv_index++) {
			ON_3dPoint p;
			nurbs_curve.GetCV(cv_index, p);
			std::cerr << "CV[" << cv_index << "] - " << p.x << ", " << p.y << std::endl;
		    }
		}
	    }

	    // check for bridging trim, trims along singularities
	    // are implicitly expected
	    ON_2dPoint end_current, start_next;
	    end_current = (*samples)[samples->Count() - 1];
	    start_next = (*nsamples)[0];

	    if (end_current.DistanceTo(start_next) > PBC_TOL) {
		// endpoints don't connect
		int is;
		const ON_Surface *surf = data->surftree->getSurface();
		if ((is = check_pullback_singularity_bridge(surf, end_current, start_next)) >= 0) {
		    // insert trim
		    // insert singular trim along
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    ON_Surface::ISO iso = ON_Surface::N_iso;
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

		    ON_Curve* sing_c2d = new ON_LineCurve(end_current, start_next);
		    trimCurve = brep->m_C2.Count();
		    brep->m_C2.Append(sing_c2d);

		    int vi;
		    if (data->order_reversed)
			vi = data->edge->m_vi[0];
		    else
			vi = data->edge->m_vi[1];

		    ON_BrepTrim& sing_trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop&) *loop, iso, trimCurve);

		    sing_trim.m_tolerance[0] = 1e-3; //TODO: need constant tolerance?
		    sing_trim.m_tolerance[1] = 1e-3;
		    ON_Interval sing_PD = sing_trim.ProxyCurveDomain();
		    sing_trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &sing_PD);
		    sing_trim.m_iso = iso;
		    //trim.Reverse();
		    sing_trim.IsValid(&tl);
		} /*else if ((is = check_pullback_seam_bridge(surf, end_current, start_next)) >= 0) {
		  // insert trim
		  // insert singular trim along
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

		  ON_Curve* c2d = new ON_LineCurve(end_current, start_next);
		  trimCurve = brep->m_C2.Count();
		  brep->m_C2.Append(c2d);

		  int vi;
		  int vo;
		  if (data->order_reversed) {
		  vi = data->edge->m_vi[0];
		  vo = data->edge->m_vi[1];
		  } else {
		  vi = data->edge->m_vi[1];
		  vo = data->edge->m_vi[0];
		  }
		  #ifdef TREATASBOUNDARY
		  ON_BrepEdge& e = (ON_BrepEdge&)*data->edge;
		  ON_BrepTrim& trim = brep->NewTrim(e, false, (ON_BrepLoop&) *loop, trimCurve);

		  trim.m_type = ON_BrepTrim::boundary;
		  trim.m_tolerance[0] = 1e-3; //TODO: need constant tolerance?
		  trim.m_tolerance[1] = 1e-3;
		  #else
		  ON_BrepTrim& trim = brep->NewSingularTrim(brep->m_V[vi], (ON_BrepLoop&) *loop, iso, trimCurve);

		  trim.m_tolerance[0] = 1e-3; //TODO: need constant tolerance?
		  trim.m_tolerance[1] = 1e-3;
		  ON_Interval PD = trim.ProxyCurveDomain();
		  trim.m_iso = surf->IsIsoparametric(*brep->m_C2[trimCurve], &PD);
		  trim.m_iso = iso;
		  #endif
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
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; // already loaded

    ON_3dPoint origin = GetOrigin();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();
    ON_3dVector norm = GetNormal();

    origin = origin * LocalUnits::length;

    ON_Plane p(origin, xaxis, yaxis);

    ON_PlaneSurface *s = new ON_PlaneSurface(p);

    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    // origin may not lie within face so include in extent
    double maxdist = origin.DistanceTo(trim_curve_3d_bbox->m_max);
    double mindist = origin.DistanceTo(trim_curve_3d_bbox->m_min);
    bbdiag += MAX(maxdist, mindist);

    //TODO: look into line curves that are just point and direction
    ON_Interval extents(-bbdiag, bbdiag);
    s->SetExtents(0, extents);
    s->SetExtents(1, extents);
    s->SetDomain(0, 0.0, 1.0);
    s->SetDomain(1, 0.0, 1.0);

    ON_id = brep->AddSurface(s);

    return true;
}


bool
CylindricalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // new surface if reused because of bounding
    //if (ON_id >= 0)
    //	return true; // already loaded

    if ((false) && (brep->m_S.Count() == 56)) {
	std::cerr << "LoadONBrep for CylindricalSurface: " << 55 << std::endl;
    }
    ON_3dPoint origin = GetOrigin();
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    origin = origin * LocalUnits::length;

    // make sure origin is part of the bbox
    trim_curve_3d_bbox->Set(origin, true);

    double bbdiag = trim_curve_3d_bbox->Diagonal().Length();
    origin = origin - bbdiag * norm;
    ON_Plane p(origin, xaxis, yaxis);

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, origin, radius * LocalUnits::length);

    //ON_Cylinder cyl(c, ON_DBL_MAX);
    ON_Cylinder cyl(c, 2.0 * bbdiag);

    ON_RevSurface* s = cyl.RevSurfaceForm();
    if (s) {
	double r = fabs(cyl.circle.radius);
	if (r <= ON_SQRT_EPSILON)
	    r = 1.0;
	s->SetDomain(0, 0.0, 2.0 * ON_PI * r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ConicalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; // already loaded

    ON_3dPoint origin = GetOrigin();
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    origin = origin * LocalUnits::length;

    double tan_semi_angle = tan(semi_angle * LocalUnits::planeangle);
    double height = (radius * LocalUnits::length) / tan_semi_angle;
    double hplus = height * 2.01;
    double r1 = hplus * tan_semi_angle;

    origin = origin + norm * (-height);
    ON_Plane p(origin, xaxis, yaxis);
    ON_Cone c(p, hplus, r1);

    ON_RevSurface* s = c.RevSurfaceForm();
    if (s) {
	double r = fabs(c.radius);
	if (r <= ON_SQRT_EPSILON)
	    r = 1.0;
	s->SetDomain(0, 0.0, 2.0 * ON_PI);
    }

    ON_id = brep->AddSurface(s);

    return true;
}


int
intersectLines(ON_Line &l1, ON_Line &l2, ON_3dPoint &out)
{
    fastf_t t, u;
    point_t p, a;
    vect_t d, c;
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    VMOVE(p, l1.from);
    VMOVE(a, l2.from);
    ON_3dVector l1dir = l1.Direction();
    l1dir.Unitize();
    VMOVE(d, l1dir);
    ON_3dVector l2dir = l2.Direction();
    l2dir.Unitize();
    VMOVE(c, l2dir);

    int i = bn_isect_line3_line3(&t, &u, p, d, a, c, &tol);
    if (i == 1) {
	VMOVE(out, l1.from);
	out = out + t * l1dir;
    }
    return i;
}


void
Circle::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin = GetOrigin();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();
    ON_Plane p(origin, xaxis, yaxis);
    ON_3dPoint center = origin * LocalUnits::length;

    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle c(p, center, radius* LocalUnits::length);

    ON_3dPoint P = c.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;


    P = c.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Circle::LoadONBrep(ON_Brep *brep) {
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    if ((false) && ((brep->m_C3.Count() == 3) || (id == 1723))) {
	std::cerr << "Debug:LoadONBrep for Circle:ID:" << id << std::endl;
    }

    ON_3dPoint origin = GetOrigin();
    ON_3dPoint center = origin * LocalUnits::length;
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();
    double r = radius * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    // Creates a circle parallel to the plane
    // with given center and radius.
    ON_Circle circle(plane, center, r);

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    double TOL = 1e-9;
    if (trimmed) { //explicitly trimmed
	if (parameter_trim) {
	    if (NEAR_ZERO(t, TOL)) {
		t = 0.0;
	    } else if (t < 0.0) {
		t = t + 2 * ON_PI;
	    }
	    if (NEAR_ZERO(s, TOL)) {
		s = 2 * ON_PI;
	    } else if (s < 0.0) {
		s = s + 2 * ON_PI;
	    }
	    if (s < t) {
		s = s + 2 * ON_PI;
	    }
	    pnt1 = circle.PointAt(t);
	    pnt2 = circle.PointAt(s);
	    //TODO: check sense agreement
	} else {

	    //must be point trim so calc t, s from points
	    pnt1 = trim_startpoint;
	    pnt2 = trim_endpoint;

	    // NOTE: point from point trim entity already converted to proper units

	    ON_3dVector fp = pnt1 - center;
	    double xdot = fp * xaxis;
	    double ydot = fp * yaxis;
	    t = atan2(ydot, xdot);
	    if (NEAR_ZERO(t, TOL)) {
		t = 0.0;
	    } else if (t < 0.0) {
		t = t + 2 * ON_PI;
	    }

	    fp = pnt2 - center;
	    xdot = fp * xaxis;
	    ydot = fp * yaxis;
	    s = atan2(ydot, xdot);
	    if (NEAR_ZERO(s, TOL)) {
		s = 2 * ON_PI;
	    } else if (s < 0.0) {
		s = s + 2 * ON_PI;
	    }

	    if (s < t) {
		s = s + 2 * ON_PI;
	    }
	}
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();

	pnt1 = pnt1 * LocalUnits::length;
	pnt2 = pnt2 * LocalUnits::length;

	ON_3dVector fp = pnt1 - center;
	double xdot = fp * xaxis;
	double ydot = fp * yaxis;
	t = atan2(ydot, xdot);
	if (NEAR_ZERO(t, TOL)) {
	    t = 0.0;
	} else if (t < 0.0) {
	    t = t + 2 * ON_PI;
	}

	fp = pnt2 - center;
	xdot = fp * xaxis;
	ydot = fp * yaxis;
	s = atan2(ydot, xdot);
	if (NEAR_ZERO(s, TOL)) {
	    s = 2 * ON_PI;
	} else if (s < 0.0) {
	    s = s + 2 * ON_PI;
	}

	if (s < t) {
	    s = s + 2 * ON_PI;
	    ;
	}
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep
		  << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint PB = pnt1;
    ON_3dPoint PE = pnt2;
    double theta = s - t;
    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }
    double dtheta = theta / narcs;
    double w = cos(dtheta / 2.0);
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; /* 2 * max narcs + 1 */
    ON_3dPoint P0, P1, P2, PM, PT;
    ON_3dVector T0, T2;

    P0 = PB;
    T0 = circle.TangentAt(t);

    for (int i = 0; i < narcs; i++) {
	angle = angle + dtheta;
	P2 = circle.PointAt(angle);
	T2 = circle.TangentAt(angle);
	ON_Line tangent1(P0, P0 + r * T0);
	ON_Line tangent2(P2, P2 + r * T2);
	if (intersectLines(tangent1, tangent2, P1) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}

	P1 = (w) * P1; // must pre-weight before putting into NURB
	cpts.Append(P0);
	W[2 * i] = 1.0;
	cpts.Append(P1);
	W[2 * i + 1] = w;

	P0 = P2;
	T0 = T2;
    }
    cpts.Append(PE);
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double) k) / narcs;
    }

    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Ellipse::SetParameterTrim(double start_param, double end_param) {
    double startpoint[3];
    double endpoint[3];

    t = start_param * LocalUnits::planeangle;
    s = end_param * LocalUnits::planeangle;

    if (s < t) {
	s = s + 2 * ON_PI;
    }
    ON_3dPoint origin = GetOrigin();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();
    ON_3dPoint center = origin * LocalUnits::length;

    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Plane plane(origin, xaxis, yaxis);
    ON_Ellipse ellipse(plane, a, b);

    ON_3dPoint P = ellipse.PointAt(t);

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    P = ellipse.PointAt(s);

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}

bool
Ellipse::LoadONBrep(ON_Brep *brep) {
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded
    ON_3dPoint origin = GetOrigin();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    origin = origin * LocalUnits::length;
    xaxis.Unitize();
    yaxis.Unitize();

    ON_Plane plane(origin, xaxis, yaxis);

    ON_3dPoint center = origin;
    double a = semi_axis_1 * LocalUnits::length;
    double b = semi_axis_2 * LocalUnits::length;
    ON_Ellipse ellipse(plane, a, b);

    double eccentricity = sqrt(1.0 - (b * b) / (a * a));
    ON_3dPoint focus_1 = center + (eccentricity * a) * xaxis;
    ON_3dPoint focus_2 = center - (eccentricity * a) * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	if (parameter_trim) {
	    if (s < t) {
		double tmp = s;
		s = t;
		t = tmp;
	    }
	    pnt1 = ellipse.PointAt(t);
	    pnt2 = ellipse.PointAt(s);
	    //TODO: check sense agreement
	} else {
	    double TOL = 1e-9;

	    //must be point trim so calc t, s from points
	    pnt1 = trim_startpoint;
	    pnt2 = trim_endpoint;

	    // NOTE: point from point trim entity already converted to proper units

	    ON_3dVector fp = pnt1 - center;
	    double xdot = fp * xaxis;
	    double ydot = fp * yaxis;
	    t = atan2(ydot / b, xdot / a);
	    if (NEAR_ZERO(t, TOL)) {
		t = 0.0;
	    } else if (t < 0.0) {
		t = t + 2 * ON_PI;
	    }

	    fp = pnt2 - center;
	    xdot = fp * xaxis;
	    ydot = fp * yaxis;
	    s = atan2(ydot / b, xdot / a);
	    if (NEAR_ZERO(s, TOL)) {
		s = 2 * ON_PI;
	    } else if (s < 0.0) {
		s = s + 2 * ON_PI;
	    }

	    if (s < t) {
		double tmp = s;
		s = t;
		t = tmp;
	    }
	}
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	double TOL = 1e-9;

	pnt1 = start->Point3d();
	pnt2 = end->Point3d();

	pnt1 = pnt1 * LocalUnits::length;
	pnt2 = pnt2 * LocalUnits::length;

	ON_3dVector fp = pnt1 - center;
	double xdot = fp * xaxis;
	double ydot = fp * yaxis;
	t = atan2(ydot / b, xdot / a);
	if (NEAR_ZERO(t, TOL)) {
	    t = 0.0;
	} else if (t < 0.0) {
	    t = t + 2 * ON_PI;
	}

	fp = pnt2 - center;
	xdot = fp * xaxis;
	ydot = fp * yaxis;
	s = atan2(ydot / b, xdot / a);
	if (NEAR_ZERO(s, TOL)) {
	    s = 2 * ON_PI;
	} else if (s < 0.0) {
	    s = s + 2 * ON_PI;
	}

	if (s < t) {
	    double tmp = s;
	    s = t;
	    t = tmp;
	}
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep
		  << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    double theta = s - t;

    int narcs = 1;
    if (theta < ON_PI / 2.0) {
	narcs = 1;
    } else if (theta < ON_PI) {
	narcs = 2;
    } else if (theta < ON_PI * 3.0 / 2.0) {
	narcs = 3;
    } else {
	narcs = 4;
    }
    double dtheta = theta / narcs;
    ON_3dPointArray cpts(2 * narcs + 1);
    double angle = t;
    double W[2 * 4 + 1]; // 2 * max narcs + 1
    ON_3dPoint Pnt[2 * 4 + 1];
    ON_3dPoint MP0, MP1, MP2, MPM, MPT, MPX;
    ON_3dVector Tangent1, Tangent2;
    ON_3dPoint P0, PX, P2, P1;

    P0 = ellipse.PointAt(angle);
    for (int i = 0; i < narcs; i++) {
	Tangent1 = ellipse.TangentAt(angle);
	PX = ellipse.PointAt(angle + dtheta / 2.0);
	// step angle
	angle = angle + dtheta;
	P2 = ellipse.PointAt(angle);
	Tangent2 = ellipse.TangentAt(angle);
	ON_Line tangent1(P0, P0 + Tangent1);
	ON_Line tangent2(P2, P2 + Tangent2);
	if (intersectLines(tangent1, tangent2, P1) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	ON_Line l1(P1, center);
	ON_Line l2(P0, P2);
	ON_3dPoint PM;
	if (intersectLines(l1, l2, PM) != 1) {
	    std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	    return false;
	}
	double mx = PM.DistanceTo(PX);
	double mp1 = PM.DistanceTo(P1);
	double R = mx / mp1;
	double w = R / (1 - R);

	cpts.Append(P0);
	Pnt[2 * i] = P0;
	W[2 * i] = 1.0;

	Pnt[2 * i + 1] = P1;
	P1 = (w) * P1; // must pre-weight before putting into NURB
	cpts.Append(P1);
	W[2 * i + 1] = w;

	P0 = P2;
    }
    cpts.Append(P2);
    Pnt[2 * narcs] = P2;
    W[2 * narcs] = 1.0;

    int degree = 2;
    int n = cpts.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    double u[4 + 1]; /* max narcs + 1 */

    for (int k = 0; k < narcs + 1; k++) {
	u[k] = ((double) k) / narcs;
    }

    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, true, degree + 1, n);

    c->ReserveKnotCapacity(m + 1);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < narcs; i++) {
	double knot_value = u[i] / u[narcs];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = n - 1; i < m; i++) {
	c->SetKnot(i, 1.0);
    }
    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = cpts[i];
	c->SetCV(i, pnt);
	c->SetWeight(i, W[i]);
    }

    ON_id = brep->AddEdgeCurve(c);

    return true;
}

void
Hyperbola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin = GetOrigin();
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    ON_3dPoint center = origin;
    double a = semi_axis;
    double b = semi_imag_axis;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }

    double y = b * tan(t);
    double x = a / cos(t);

    ON_3dVector X = x * xaxis;
    ON_3dVector Y = y * yaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    y = b * tan(s);
    x = a / cos(s);

    X = x * xaxis;
    Y = y * yaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Hyperbola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin = GetOrigin();
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    norm.Unitize();
    xaxis.Unitize();
    yaxis.Unitize();

    ON_3dPoint center = origin * LocalUnits::length;
    double a = semi_axis * LocalUnits::length;
    double b = semi_imag_axis * LocalUnits::length;

    double eccentricity = sqrt(1.0 + (b * b) / (a * a));
    ON_3dPoint vertex = center + a * xaxis;
    ON_3dPoint directrix = center + (a / eccentricity) * xaxis;
    ON_3dPoint focus = center + (eccentricity * a) * xaxis;
    ON_3dPoint focusprime = center - (eccentricity * a) * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) {
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 =  pnt2 * LocalUnits::length;

    // calc tangent P0, P2 intersection
    ON_3dVector ToFocus = focus - P0;
    ToFocus.Unitize();

    ON_3dVector ToFocusPrime = focusprime - P0;
    ToFocusPrime.Unitize();

    ON_3dVector bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs0(P0, P0 + bisector);

    ToFocus = focus - P2;
    ToFocus.Unitize();

    ToFocusPrime = focusprime - P2;
    ToFocusPrime.Unitize();

    bisector = ToFocus + ToFocusPrime;
    bisector.Unitize();

    ON_Line bs2(P2, P2 + bisector);
    ON_3dPoint P1;
    if (intersectLines(bs0, bs2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    ON_Line l1(focus, P1);
    ON_Line l2(P0, P2);
    ON_3dPoint M = (P0 + P2) / 2.0;

    ON_Line ctom(center, M);
    ON_3dVector dtom = ctom.Direction();
    dtom.Unitize();
    double m = (dtom * yaxis) / (dtom * xaxis);
    double x1 = a * b * sqrt(1.0 / (b * b - m * m * a * a));
    double y1 = b * sqrt((x1 * x1) / (a * a) - 1.0);
    if (m < 0.0) {
	y1 *= -1.0;
    }
    ON_3dVector X = x1 * xaxis;
    ON_3dVector Y = y1 * yaxis;
    ON_3dPoint Pv = center + X + Y;
    double mx = M.DistanceTo(Pv);
    double mp1 = M.DistanceTo(P1);
    double R = mx / mp1;
    double w = R / (1 - R);

    P1 = (w) * P1; // must pre-weight before putting into NURB

    // add hyperbola weightings
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();
    bcurve->SetWeight(1, w);

    ON_NurbsCurve* hypernurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*hypernurbscurve);

    ON_id = brep->AddEdgeCurve(hypernurbscurve);

    return true;
}


void
Parabola::SetParameterTrim(double start_param, double end_param)
{
    double startpoint[3];
    double endpoint[3];

    t = start_param;
    s = end_param;

    ON_3dPoint origin = GetOrigin();
    ON_3dVector xaxis = GetXAxis();
    xaxis.Unitize();
    ON_3dVector yaxis = GetYAxis();
    yaxis.Unitize();

    ON_3dPoint center = origin;
    double fd = focal_dist;

    if (s < t) {
	double tmp = s;
	s = t;
	t = tmp;
    }
    double x = 2.0 * fd * t; // tan(t);
    double y = (x * x) / (4 * fd);

    ON_3dVector X = x * yaxis;
    ON_3dVector Y = y * xaxis;
    ON_3dPoint P = center + X + Y;

    startpoint[0] = P.x;
    startpoint[1] = P.y;
    startpoint[2] = P.z;

    x = 2.0 * fd * s; //tan(s);
    y = (x * x) / (4 * fd);

    X = x * yaxis;
    Y = y * xaxis;
    P = center + X + Y;

    endpoint[0] = P.x;
    endpoint[1] = P.y;
    endpoint[2] = P.z;

    SetPointTrim(startpoint, endpoint);
}


bool
Parabola::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog dump;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint origin = GetOrigin();
    ON_3dVector normal = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    ON_3dPoint center = origin * LocalUnits::length;
    double fd = focal_dist * LocalUnits::length;
    ON_3dPoint focus = center + fd * xaxis;
    ON_3dPoint directrix = center - fd * xaxis;

    ON_3dPoint pnt1;
    ON_3dPoint pnt2;
    if (trimmed) { //explicitly trimmed
	pnt1 = trim_startpoint;
	pnt2 = trim_endpoint;
    } else if ((start != NULL) && (end != NULL)) { //not explicit let's try edge vertices
	pnt1 = start->Point3d();
	pnt2 = end->Point3d();
    } else {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not endpoints for specified for curve " << entityname << std::endl;
	return false;
    }

    ON_3dPoint P0 = pnt1 * LocalUnits::length;
    ON_3dPoint P2 = pnt2 * LocalUnits::length;

    // calc tang intersect with transverse axis
    ON_3dVector ToFocus;

    ToFocus = P0 - focus;
    ToFocus.Unitize();
    ON_3dVector bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent1(P0, P0 + 10.0 * bisector);

    ToFocus = P2 - focus;
    ToFocus.Unitize();
    bisector = ToFocus + xaxis;
    bisector.Unitize();
    ON_Line tangent2(P2, P2 + 10.0 * bisector);

    ON_3dPoint P1;
    if (intersectLines(tangent1, tangent2, P1) != 1) {
	std::cerr << entityname << ": Error: Control point can not be calculated." << std::endl;
	return false;
    }

    // make parabola from bezier
    ON_3dPointArray cpts(3);
    cpts.Append(P0);
    cpts.Append(P1);
    cpts.Append(P2);
    ON_BezierCurve *bcurve = new ON_BezierCurve(cpts);
    bcurve->MakeRational();

    ON_NurbsCurve* parabnurbscurve = ON_NurbsCurve::New();

    bcurve->GetNurbForm(*parabnurbscurve);
    ON_id = brep->AddEdgeCurve(parabnurbscurve);

    return true;
}


bool
Line::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    //if (ON_id >= 0)
    //	return true; // already loaded

    ON_3dPoint startpnt = start->Point3d();
    ON_3dPoint endpnt = end->Point3d();

    startpnt = startpnt * LocalUnits::length;
    endpnt = endpnt * LocalUnits::length;

    ON_LineCurve *l = new ON_LineCurve(startpnt, endpnt);

    ON_id = brep->AddEdgeCurve(l);

    return true;
}


bool
SurfaceOfLinearExtrusion::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; // already loaded

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_Curve *curve = brep->m_C3[swept_curve->GetONId()];

    // use trimming edge bounding box unioned with bounding box of
    // curve being extruded; calc diagonal to make sure extrusion
    // magnitude is well represented
    ON_BoundingBox curvebb = curve->BoundingBox();
    trim_curve_3d_bbox->Union(curvebb);
    double bbdiag = trim_curve_3d_bbox->Diagonal().Length(); // already converted to local units;

    ON_3dPoint dir = extrusion_axis->Orientation();
    double mag = extrusion_axis->Magnitude() * LocalUnits::length;
    mag = MAX(mag, bbdiag);

    ON_3dPoint startpnt;
    if (swept_curve->PointAtStart() == NULL) {
	startpnt = curve->PointAtStart();
    } else {
	startpnt = swept_curve->PointAtStart();
	startpnt = startpnt * LocalUnits::length;
    }

    // add a little buffer in the surface extrusion distance
    // by extruding "+/-mag" distance along "dir"
    ON_3dPoint extrusion_endpnt = startpnt + mag * dir;
    ON_3dPoint extrusion_startpnt = startpnt - mag * dir;

    ON_LineCurve *l = new ON_LineCurve(extrusion_startpnt, extrusion_endpnt);

    // the following extrude code lifted from OpenNURBS ON_BrepExtrude()
    ON_Line path_line;
    path_line.from = extrusion_startpnt;
    path_line.to = extrusion_endpnt;
    ON_3dVector path_vector = path_line.Direction();
    if (path_vector.IsZero())
	return false;

    ON_SumSurface* sum_srf = 0;

    ON_Curve* srf_base_curve = curve->Duplicate();
    srf_base_curve->Translate(-mag * dir);

    ON_3dPoint sum_basepoint = ON_origin - l->PointAtStart();
    sum_srf = new ON_SumSurface();
    sum_srf->m_curve[0] = srf_base_curve;
    sum_srf->m_curve[1] = l; //srf_path_curve;
    sum_srf->m_basepoint = sum_basepoint;
    sum_srf->BoundingBox(); // fills in sum_srf->m_bbox

    if (!sum_srf)
	return false;

    ON_id = brep->AddSurface(sum_srf);

    return true;
}


bool
SurfaceOfRevolution::LoadONBrep(ON_Brep *brep)
{
    ON_TextLog tl;

    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (ON_id >= 0)
	return true; // already loaded

    // load parent class
    if (!SweptSurface::LoadONBrep(brep)) {
	std::cerr << "Error: " << entityname << "::LoadONBrep() - Error loading openNURBS brep." << std::endl;
	return false;
    }

    ON_3dPoint start = axis_position->GetOrigin();
    ON_3dVector dir = axis_position->GetNormal();
    ON_3dPoint end = start + dir;

    ON_Line axisline(start, end);
    ON_RevSurface* revsurf = ON_RevSurface::New();
    revsurf->m_curve = brep->m_C3[swept_curve->GetONId()]->DuplicateCurve();
    revsurf->m_axis = axisline;
    revsurf->BoundingBox(); // fills in sum_srf->m_bbox

    //ON_Brep* b = ON_BrepRevSurface(revsurf, true, true);

    if (!revsurf)
	return false;

    ON_id = brep->AddSurface(revsurf);

    return true;
}


bool
SphericalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    // get sphere center
    ON_3dPoint center = GetOrigin();
    center = center * LocalUnits::length;

    // Creates a sphere with given center and radius.
    ON_Sphere sphere(center, radius * LocalUnits::length);

    ON_RevSurface* s = sphere.RevSurfaceForm();
    if (s) {
	double r = fabs(sphere.radius);
	if (r <= ON_SQRT_EPSILON)
	    r = 1.0;
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	s->SetDomain(1, -r, r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
ToroidalSurface::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    ON_3dPoint origin = GetOrigin();
    ON_3dVector norm = GetNormal();
    ON_3dVector xaxis = GetXAxis();
    ON_3dVector yaxis = GetYAxis();

    origin = origin * LocalUnits::length;

    ON_Plane p(origin, xaxis, yaxis);

    // Creates a torus parallel to the plane
    // with given major and minor radius.
    ON_Torus torus(p, major_radius * LocalUnits::length, minor_radius * LocalUnits::length);

    ON_RevSurface* s = torus.RevSurfaceForm();
    if (s) {
	double r = fabs(torus.major_radius);
	if (r <= ON_SQRT_EPSILON)
	    r = 1.0;
	r *= ON_PI;
	s->SetDomain(0, 0.0, 2.0 * r);
	r = fabs(torus.minor_radius);
	if (r <= ON_SQRT_EPSILON)
	    r = 1.0;
	r *= ON_PI;
	s->SetDomain(1, 0.0, 2.0 * r);
    }
    ON_id = brep->AddSurface(s);

    return true;
}


bool
VertexLoop::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

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
    corner[0] = surface->PointAt(U.m_t[0], V.m_t[0]);
    corner[1] = surface->PointAt(U.m_t[1], V.m_t[0]);
    corner[2] = surface->PointAt(U.m_t[0], V.m_t[1]);
    corner[3] = surface->PointAt(U.m_t[1], V.m_t[1]);

    ON_2dPoint start, end;
    ON_Surface::ISO iso = ON_Surface::N_iso;
    if (VNEAR_EQUAL(vertex, corner[0], POINT_CLOSENESS_TOLERANCE)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[1], POINT_CLOSENESS_TOLERANCE)) {
	    //south;
	    end = ON_2dPoint(U.m_t[1], V.m_t[0]);
	    iso = ON_Surface::S_iso;
	} else if (VNEAR_EQUAL(vertex, corner[2], POINT_CLOSENESS_TOLERANCE)) {
	    //west
	    end = ON_2dPoint(U.m_t[0], V.m_t[1]);
	    iso = ON_Surface::W_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[1], POINT_CLOSENESS_TOLERANCE)) {
	start = ON_2dPoint(U.m_t[1], V.m_t[0]);
	if (VNEAR_EQUAL(vertex, corner[3], POINT_CLOSENESS_TOLERANCE)) {
	    //east
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::E_iso;
	}
    } else if (VNEAR_EQUAL(vertex, corner[2], POINT_CLOSENESS_TOLERANCE)) {
	start = ON_2dPoint(U.m_t[0], V.m_t[1]);
	if (VNEAR_EQUAL(vertex, corner[3], POINT_CLOSENESS_TOLERANCE)) {
	    //north
	    end = ON_2dPoint(U.m_t[1], V.m_t[1]);
	    iso = ON_Surface::N_iso;
	}
    }
    ON_Curve* c2d = new ON_LineCurve(start, end);
    int trimCurve = brep->m_C2.Count();
    brep->m_C2.Append(c2d);

    (void)brep->NewSingularTrim(brep->m_V[loop_vertex->GetONId()], loop, iso, trimCurve);
    /* FIXME: do something with this trim */

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
