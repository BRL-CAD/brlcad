/*                 DerivedUnit.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
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
/** @file DerivedUnit.cpp
 *
 * Routines to convert STEP "DerivedUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "NamedUnit.h"
#include "DerivedUnitElement.h"
#include "DerivedUnit.h"

#define CLASSNAME "DerivedUnit"
#define ENTITYNAME "Derived_Unit"
string DerivedUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)DerivedUnit::Create);

DerivedUnit::DerivedUnit() {
	step = NULL;
	id = 0;
}

DerivedUnit::DerivedUnit(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

DerivedUnit::~DerivedUnit() {
	elements.clear();
}

bool
DerivedUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Unit::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Unit." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (elements.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse,"elements");
		LIST_OF_ENTITIES::iterator i;
		for(i=l->begin();i!=l->end();i++) {
			DerivedUnitElement *aDUE = (DerivedUnitElement *)Factory::CreateObject(sw,(*i));

			elements.push_back(aDUE);
			if (!aDUE->Load(step,(*i))) {
				l->clear();
				delete l;
				return false;
			}
		}
		l->clear();
		delete l;
	}

	return true;
}

void
DerivedUnit::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "elements:" << endl;
	LIST_OF_DERIVED_UNIT_ELEMENTS::iterator i;
	for(i=elements.begin();i!=elements.end();i++) {
		(*i)->Print(level+1);
	}

	TAB(level); cout << "Inherited Attributes:" << endl;
	Unit::Print(level+1);
}
STEPEntity *
DerivedUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		DerivedUnit *object = new DerivedUnit(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method." << endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}
