/*                 GeometricSetSelect.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2021 United States Government as represented by
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
/** @file step/GeometricSetSelect.cpp
 *
 * Routines to convert STEP "GeometricSetSelect" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Axis2Placement.h"

#include "GeometricSetSelect.h"
#include "Point.h"
#include "Curve.h"
#include "Surface.h"

#define CLASSNAME "GeometricSetSelect"
#define ENTITYNAME "Geometric_Set_Select"
string GeometricSetSelect::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)GeometricSetSelect::Create);

GeometricSetSelect::GeometricSetSelect() {
    step = NULL;
    id = 0;
    element = NULL;
    type = GeometricSetSelect::GEOMETRIC_SET_SELECT_UNKNOWN;
}

GeometricSetSelect::GeometricSetSelect(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    element = NULL;
    type = GeometricSetSelect::GEOMETRIC_SET_SELECT_UNKNOWN;
}

GeometricSetSelect::~GeometricSetSelect() {
}

Point *
GeometricSetSelect::GetPointElement() {
    return dynamic_cast<Point *>(element);
}

Curve *
GeometricSetSelect::GetCurveElement() {
    return dynamic_cast<Curve *>(element);
}

Surface *
GeometricSetSelect::GetSurfaceElement() {
    return dynamic_cast<Surface *>(element);
}

bool
GeometricSetSelect::Load(STEPWrapper *sw, SDAI_Application_instance *sse) {
    step=sw;

    if (element == NULL) {
	SdaiGeometric_set_select *v = (SdaiGeometric_set_select *)sse;

	if (v->IsPoint()) {
	    SdaiPoint *point_select = *v;
	    Point *aPoint = dynamic_cast<Point *>(Factory::CreateObject(sw, (SDAI_Application_instance *)point_select));
	    type = GeometricSetSelect::POINT;
	    element = aPoint;
	} else if (v->IsCurve()) {
	    SdaiCurve *curve_select = *v;
	    Curve *aCurve = dynamic_cast<Curve *>(Factory::CreateObject(sw, (SDAI_Application_instance *)curve_select));
	    type = GeometricSetSelect::CURVE;
	    element = aCurve;
	} else if (v->IsSurface()) {
	    SdaiSurface *surface_select = *v;
	    Surface *aSurface = dynamic_cast<Surface *>(Factory::CreateObject(sw, (SDAI_Application_instance *)surface_select));
	    type = GeometricSetSelect::SURFACE;
	    element = aSurface;
	} else {
	    type = GeometricSetSelect::GEOMETRIC_SET_SELECT_UNKNOWN;
	    element = NULL;
	}
    }

    return true;
}

void
GeometricSetSelect::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << entityname << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    //todo: expand out to print actual select type
    TAB(level+1); std::cout << "definition: ";

    if (type == GeometricSetSelect::POINT) {
	std::cout << "GeometricSetSelect::POINT" << std::endl;
	Point *aPoint = dynamic_cast<Point *>(element);
	if (aPoint) {
	    aPoint->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "POINT == NULL" << std::endl;
	}
    } else if (type == GeometricSetSelect::CURVE) {
	std::cout << "GeometricSetSelect::CURVE" << std::endl;
	Curve *aCurve = dynamic_cast<Curve *>(element);
	if (aCurve) {
	    aCurve->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "CURVE == NULL" << std::endl;
	}
    } else if (type == GeometricSetSelect::SURFACE) {
	std::cout << "GeometricSetSelect::SURFACE" << std::endl;
	Surface *aSurface = dynamic_cast<Surface *>(element);
	if (aSurface) {
	    aSurface->Print(level+2);
	} else {
	    TAB(level+2); std::cout << "SURFACE == NULL" << std::endl;
	}
    } else {
	std::cout << "GEOMETRIC_SET_SELECT_UNKNOWN" << std::endl;
    }
}

STEPEntity *
GeometricSetSelect::GetInstance(STEPWrapper *sw, int id)
{
    return new GeometricSetSelect(sw, id);
}

STEPEntity *
GeometricSetSelect::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
GeometricSetSelect::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
