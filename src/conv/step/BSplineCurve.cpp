/*                 BSplineCurve.cpp
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
/** @file step/BSplineCurve.cpp
 *
 * Routines to convert STEP "BSplineCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "BSplineCurve.h"
#include "CartesianPoint.h"

#define CLASSNAME "BSplineCurve"
#define ENTITYNAME "B_Spline_Curve"
string BSplineCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)BSplineCurve::Create);

static const char *B_spline_curve_form_string[] = {
	"polyline_form",
	"circular_arc",
	"elliptic_arc",
	"parabolic_arc",
	"hyperbolic_arc",
	"unspecified",
	"unset"
};

BSplineCurve::BSplineCurve() {
	step = NULL;
	id = 0;
}

BSplineCurve::BSplineCurve(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

BSplineCurve::~BSplineCurve() {
	/*
	LIST_OF_POINTS::iterator i = control_points_list.begin();

	while(i != control_points_list.end()) {
		delete (*i);
		i = control_points_list.erase(i);
	}
	*/
	control_points_list.clear();
}

bool
BSplineCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	bool retValue = true;

	step=sw;
	id = sse->STEPfile_id;

	if ( !BoundedCurve::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (control_points_list.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"control_points_list");
		LIST_OF_ENTITIES::iterator i;
		for(i=l->begin();i!=l->end();i++) {
			SCLP23(Application_instance) *entity = (*i);
			if (entity) {
				CartesianPoint *aCP = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw,entity));

				control_points_list.push_back(aCP);
			} else {
				std::cerr << CLASSNAME  << ": Unhandled entity in attribute 'control_points_list'." << std::endl;
				return false;
			}
		}
		l->clear();
		delete l;
	}

	closed_curve = step->getLogicalAttribute(sse,"closed_curve");
	self_intersect = step->getLogicalAttribute(sse,"self_intersect");
	degree = step->getIntegerAttribute(sse,"degree");
	curve_form = (B_spline_curve_form)step->getEnumAttribute(sse,"curve_form");
	if (curve_form > B_spline_curve_form_unset)
		curve_form = B_spline_curve_form_unset;

	return retValue;
}
/*TODO: REMOVE

const double *
BSplineCurve::PointAtEnd() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
BSplineCurve::PointAtStart() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}
*/

void
BSplineCurve::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "control_points_list:" << std::endl;
	LIST_OF_POINTS::iterator i;
	for(i=control_points_list.begin();i!=control_points_list.end();i++) {
		(*i)->Print(level+1);
	}

	TAB(level+1); std::cout << "closed_curve:" << step->getLogicalString((Logical)closed_curve) << std::endl;
	TAB(level+1); std::cout << "self_intersect:" << step->getLogicalString((Logical)self_intersect) << std::endl;
	TAB(level+1); std::cout << "degree:" << degree << std::endl;
	TAB(level+1); std::cout << "curve_form:" << B_spline_curve_form_string[curve_form] << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	BoundedCurve::Print(level+1);
}

STEPEntity *
BSplineCurve::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		BSplineCurve *object = new BSplineCurve(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}
/*
bool
BSplineCurve::LoadONBrep(ON_Brep *brep)
{

	if (curve_index < 0) {
		if ( curve_form == B_spline_curve_form__polyline_form ) {
			AddPolyLine(brep);
			return true;
		} else if ( curve_form == B_spline_curve_form__circular_arc ) {
			std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
			return false;
		} else if ( curve_form == B_spline_curve_form__elliptic_arc ) {
			std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
			return false;
		} else if ( curve_form == B_spline_curve_form__parabolic_arc ) {
			std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
			return false;
		} else if ( curve_form == B_spline_curve_form__hyperbolic_arc ) {
			std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
			return false;
		} else {
			std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
			return false;
		}
	}

	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << " id: " << id << std::endl;
	return false;
}
*/

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
