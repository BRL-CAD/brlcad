/*                 LuminousIntensityContextDependentUnit.cpp
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
/** @file step/LuminousIntensityContextDependentUnit.cpp
 *
 * Routines to convert STEP "LuminousIntensityContextDependentUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "LuminousIntensityContextDependentUnit.h"

#define CLASSNAME "LuminousIntensityContextDependentUnit"
#define ENTITYNAME "Luminous_Intensity_Context_Dependent_Unit"
string LuminousIntensityContextDependentUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)LuminousIntensityContextDependentUnit::Create);

LuminousIntensityContextDependentUnit::LuminousIntensityContextDependentUnit() {
	step = NULL;
	id = 0;
}

LuminousIntensityContextDependentUnit::LuminousIntensityContextDependentUnit(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

LuminousIntensityContextDependentUnit::~LuminousIntensityContextDependentUnit() {
}

bool
LuminousIntensityContextDependentUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !LuminousIntensityUnit::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
		return false;
	}
	if ( !ContextDependentUnit::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Unit." << std::endl;
		return false;
	}

	return true;
}

void
LuminousIntensityContextDependentUnit::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	LuminousIntensityUnit::Print(level+1);
	ContextDependentUnit::Print(level+1);

}
STEPEntity *
LuminousIntensityContextDependentUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		LuminousIntensityContextDependentUnit *object = new LuminousIntensityContextDependentUnit(sw,sse->STEPfile_id);

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
