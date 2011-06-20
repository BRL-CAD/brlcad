/*                 DerivedUnitElement.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2010 DerivedUnitElemented States Government as represented by
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
/** @file step/DerivedUnitElement.cpp
 *
 * Routines to convert STEP "DerivedUnitElement" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "NamedUnit.h"
#include "DerivedUnitElement.h"

#define CLASSNAME "DerivedUnitElement"
#define ENTITYNAME "Derived_Unit_Element"
string DerivedUnitElement::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)DerivedUnitElement::Create);

DerivedUnitElement::DerivedUnitElement() {
	step = NULL;
	id = 0;
	unit = NULL;
}

DerivedUnitElement::DerivedUnitElement(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
	unit = NULL;
}

DerivedUnitElement::~DerivedUnitElement() {
}

bool
DerivedUnitElement::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (unit == NULL) {
		SCLP23(Application_instance) *se = step->getEntityAttribute(sse,"unit");

		//unit = dynamic_cast<NamedUnit*>(Factory::CreateNamedUnitObject(sw,se));
		unit = (NamedUnit*)Factory::CreateObject(sw,se);
	}
	exponent = step->getRealAttribute(sse,"exponent");

	return true;
}

void
DerivedUnitElement::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "unit:" << std::endl;
	unit->Print(level+1);
	TAB(level+1); std::cout << "exponent:" << exponent << std::endl;
}
STEPEntity *
DerivedUnitElement::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		DerivedUnitElement *object = new DerivedUnitElement(sw,sse->STEPfile_id);

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
