/*                 CartesianTransformationOperator.cpp
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
/** @file step/CartesianTransformationOperator.cpp
 *
 * Routines to convert STEP "CartesianTransformationOperator" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Direction.h"
#include "CartesianPoint.h"

#include "CartesianTransformationOperator.h"

#define CLASSNAME "CartesianTransformationOperator"
#define ENTITYNAME "Cartesian_Transformation_Operator"
string CartesianTransformationOperator::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CartesianTransformationOperator::Create);

CartesianTransformationOperator::CartesianTransformationOperator() {
	step = NULL;
	id = 0;
	axis1 = NULL;
	axis2 = NULL;
	local_origin = NULL;
	scale = 1.0;
}

CartesianTransformationOperator::CartesianTransformationOperator(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	axis1 = NULL;
	axis2 = NULL;
	local_origin = NULL;
	scale = 1.0;
}

CartesianTransformationOperator::~CartesianTransformationOperator() {
}

bool
CartesianTransformationOperator::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !GeometricRepresentationItem::Load(sw,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (axis1 == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis1");
		if (entity) { //this attribute is optional
			axis1 = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		}
	}

	if (axis2 == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis2");
		if (entity) { // this attribute is optional
			axis2 = dynamic_cast<Direction *>(Factory::CreateObject(sw,entity));
		}
	}

	if (local_origin == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"local_origin");
		if (entity) {
			local_origin = dynamic_cast<CartesianPoint *>(Factory::CreateObject(sw,entity));
		} else {
			std::cerr << CLASSNAME << ": error loading 'local_origin' attribute." << std::endl;
			return false;
		}
	}

	STEPattribute *attr = step->getAttribute(sse,"scale");
	if (attr) { //this attribute is optional
		scale = step->getRealAttribute(sse,"scale");
	} else {
		scale = 1.0;
	}

	return true;
}

void
CartesianTransformationOperator::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << GeometricRepresentationItem::name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	if (axis1) {
		TAB(level+1); std::cout << "axis1:" << std::endl;
		axis1->Print(level+1);
	}
	if (axis2) {
		TAB(level+1); std::cout << "axis2:" << std::endl;
		axis2->Print(level+1);
	}
	TAB(level+1); std::cout << "local_origin:" << std::endl;
	local_origin->Print(level+1);

	TAB(level+1); std::cout << "scale: " << scale << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	GeometricRepresentationItem::Print(level+1);
}

STEPEntity *
CartesianTransformationOperator::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CartesianTransformationOperator *object = new CartesianTransformationOperator(sw,sse->STEPfile_id);

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

bool
CartesianTransformationOperator::LoadONBrep(ON_Brep *brep)
{
	std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
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
