/*                    O P E N N U R B S S P L I N E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

/** @file step/OpenNurbsSplines.cpp
 *
 * Exact OpenNURBS construction for STEP spline curve and surface families.
 */

#include "common.h"

#include "sdai.h"
class SDAI_Application_instance;
#include "brep.h"

#include <iostream>

#include "STEPWrapper.h"
#include "LocalUnits.h"
#include "Point.h"
#include "CartesianPoint.h"

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

bool
BezierCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
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
    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
BSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, false, degree + 1, t_size);

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
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
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
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
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

    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
QuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
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

    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
    return false;
}


bool
RationalBSplineCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = t_size;
    int p = degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	curve->SetKnot(i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
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

    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
RationalBSplineCurveWithKnots::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    int t_size = control_points_list.size();

    ON_NurbsCurve *curve = ON_NurbsCurve::New(3, true, degree + 1, t_size);

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
	    if ((multiplicity > degree) || (n == knot_multiplicities.end())) {
		multiplicity = degree;
	    }
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
	    V_MIN(multiplicity, degree);

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

    SetONId(brep->AddEdgeCurve(curve));

    return true;
}


bool
RationalQuasiUniformCurve::LoadONBrep(ON_Brep *brep)
{
    if (!brep) {
	/* nothing to do */
	return false;
    }

    if (GetONId() >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
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

    if (GetONId() >= 0) {
	return true;
    }

    if (!RationalBSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
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

    if (GetONId() >= 0) {
	return true;
    }

    if (!BSplineCurve::LoadONBrep(brep)) {
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << " id: " << id << std::endl;
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
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
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

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
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
	double x = (double)j / (double)(n - p);
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
    SetONId(brep->AddSurface(surf));

    return true;
}

ON_Brep *
BSplineSurfaceWithKnots::GetONBrep()
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

    ON_Brep *b2 = ON_Brep::New();
    b2->NewFace(*brep->m_S[0]);
    b2->Flip();

    delete brep;

    return b2;
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

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

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

	    V_MIN(multiplicity, u_degree);

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

	    V_MIN(multiplicity, u_degree);

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

	    V_MIN(multiplicity, v_degree);

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

	    V_MIN(multiplicity, v_degree);

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
    SetONId(brep->AddSurface(surf));

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
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
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

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, false, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    // generate u-knots
    int n = u_size;
    int p = u_degree;
    int m = n + p - 1;
    for (int i = 0; i < p; i++) {
	surf->SetKnot(0, i, 0.0);
    }
    for (int j = 1; j < n - p; j++) {
	double x = (double)j / (double)(n - p);
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
	double x = (double)j / (double)(n - p);
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
    SetONId(brep->AddSurface(surf));

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

    ON_NurbsSurface *surf = ON_NurbsSurface::New(3, true, u_degree + 1, v_degree + 1, u_size, v_size);

    // knot index (>= 0 and < Order + CV_count - 2)
    LIST_OF_INTEGERS::iterator m = u_multiplicities.begin();
    LIST_OF_REALS::iterator r = u_knots.begin();
    int knot_index = 0;
    while (m != u_multiplicities.end()) {
	int multiplicity = (*m);
	double knot_value = (*r);

	V_MIN(multiplicity, u_degree);

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

	V_MIN(multiplicity, v_degree);

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

    SetONId(brep->AddSurface(surf));

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



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
