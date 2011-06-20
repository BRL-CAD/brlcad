/*                 NamedUnit.cpp
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
/** @file step/NamedUnit.cpp
 *
 * Routines to convert STEP "NamedUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "DimensionalExponents.h"
#include "NamedUnit.h"

#define CLASSNAME "NamedUnit"
#define ENTITYNAME "Named_Unit"
string NamedUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)NamedUnit::Create);

NamedUnit::NamedUnit() {
	step = NULL;
	id = 0;
	dimensions = NULL;

}

NamedUnit::NamedUnit(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	dimensions = NULL;
}

NamedUnit::~NamedUnit() {
	if (dimensions != NULL ) {
		delete dimensions;
	}
}

bool
NamedUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !Unit::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (dimensions == NULL) {
		SCLP23(Application_instance) *se = step->getEntityAttribute(sse,"dimensions");
		if (se != NULL ) {
			if (dimensions == NULL)
				dimensions = new DimensionalExponents();
			if (!dimensions->Load(step,se)) {
				return false;
			}
		}
	}

	return true;
}

void
NamedUnit::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Local Attributes:" << std::endl;
	TAB(level+1); std::cout << "dimensions:" << std::endl;
	dimensions->Print(level+1);

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Unit::Print(level+1);

}
STEPEntity *
NamedUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		NamedUnit *object = new NamedUnit(sw,sse->STEPfile_id);

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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
