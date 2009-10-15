/*                 ThermodynamicTemperatureUnit.cpp
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
/** @file ThermodynamicTemperatureUnit.cpp
 *
 * Routines to convert STEP "ThermodynamicTemperatureUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "ThermodynamicTemperatureUnit.h"

#define CLASSNAME "ThermodynamicTemperatureUnit"
#define ENTITYNAME "Thermodynamic_Temperature_Unit"
string ThermodynamicTemperatureUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)ThermodynamicTemperatureUnit::Create);

ThermodynamicTemperatureUnit::ThermodynamicTemperatureUnit() {
	step = NULL;
	id = 0;
}

ThermodynamicTemperatureUnit::ThermodynamicTemperatureUnit(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

ThermodynamicTemperatureUnit::~ThermodynamicTemperatureUnit() {
}

bool
ThermodynamicTemperatureUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !NamedUnit::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Unit." << endl;
		return false;
	}

	return true;
}

void
ThermodynamicTemperatureUnit::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	NamedUnit::Print(level+1);

}
STEPEntity *
ThermodynamicTemperatureUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		ThermodynamicTemperatureUnit *object = new ThermodynamicTemperatureUnit(sw,sse->STEPfile_id);

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
