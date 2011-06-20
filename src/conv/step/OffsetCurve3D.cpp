/*                 OffsetCurve3D.cpp
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
/** @file step/OffsetCurve3D.cpp
 *
 * Routines to convert STEP "OffsetCurve3D" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "Direction.h"
#include "CartesianPoint.h"
#include "CartesianTransformationOperator.h"

#include "OffsetCurve3D.h"

#define CLASSNAME "OffsetCurve3D"
#define ENTITYNAME "Offset_Curve_3d"
string OffsetCurve3D::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)OffsetCurve3D::Create);

OffsetCurve3D::OffsetCurve3D() {
	step = NULL;
	id = 0;
}

OffsetCurve3D::OffsetCurve3D(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

OffsetCurve3D::~OffsetCurve3D() {
	basis_curve = NULL;
	ref_direction = NULL;
}

bool
OffsetCurve3D::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Curve::Load(sw,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (basis_curve == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"basis_curve");
		if (entity) {
			basis_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": Error loading entity attribute 'basis_curve'." << std::endl;
			return false;
		}
	}

	distance = step->getRealAttribute(sse,"distance");
	self_intersect = step->getLogicalAttribute(sse,"self_intersect");

	if (ref_direction == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"ref_direction");
		if (entity) {
			ref_direction = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": Error loading entity attribute 'ref_direction'." << std::endl;
			return false;
		}
	}

	return true;
}

const double *
OffsetCurve3D::PointAtEnd() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtEnd() not implemented for this type of curve.";
	return NULL;
}

const double *
OffsetCurve3D::PointAtStart() {
	std::cerr << CLASSNAME << ": Error: virtual function PointAtStart() not implemented for this type of curve.";
	return NULL;
}

void
OffsetCurve3D::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	basis_curve->Print(level+1);
	TAB(level+1); std::cout << "distance:" << distance << std::endl;
	TAB(level+1); std::cout << "self_intersect:" << step->getLogicalString(self_intersect) << std::endl;
	ref_direction->Print(level+1);
}
STEPEntity *
OffsetCurve3D::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		OffsetCurve3D *object = new OffsetCurve3D(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw,sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
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
