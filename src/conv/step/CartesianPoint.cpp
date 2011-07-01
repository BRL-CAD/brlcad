/*                 CartesianPoint.cpp
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
/** @file step/CartesianPoint.cpp
 *
 * Routines to interface to STEP "CartesianPoint".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"

#define CLASSNAME "CartesianPoint"
#define ENTITYNAME "Cartesian_Point"
string CartesianPoint::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)CartesianPoint::Create);

CartesianPoint::CartesianPoint() {
	step = NULL;
	id = 0;
	vertex_index = -1;
}

CartesianPoint::CartesianPoint(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	vertex_index = -1;
}

CartesianPoint::~CartesianPoint() {
}

bool
CartesianPoint::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !Point::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Point." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	STEPattribute *attr = step->getAttribute(sse,"coordinates");
	if (attr != NULL) {
		STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
		RealNode *rn = (RealNode *)sa->GetHead();
		int index = 0;
		while ( rn != NULL) {
			coordinates[index++] = rn->value;
			rn = (RealNode *)rn->NextNode();
		}
	} else {
		std::cout << CLASSNAME << ": error loading 'coordinate' attribute." << std::endl;
	}
	return true;
}

void
CartesianPoint::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "coordinate:";
	std::cout << "(" << coordinates[0] << ",";
	std::cout << coordinates[1] << ",";
	std::cout << coordinates[2] << ")" << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Point::Print(level+1);
}

STEPEntity *
CartesianPoint::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CartesianPoint *object = new CartesianPoint(sw,sse->STEPfile_id);

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
CartesianPoint::LoadONBrep(ON_Brep *brep)
{
	AddVertex(brep);
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
