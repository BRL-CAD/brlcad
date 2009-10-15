/*                 SolidAngleSiUnit.cpp
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
/** @file SolidAngleSiUnit.cpp
 *
 * Routines to convert STEP "SolidAngleSiUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "SolidAngleSiUnit.h"

#define CLASSNAME "SolidAngleSiUnit"
#define ENTITYNAME "Solid_Angle_Si_Unit"
string SolidAngleSiUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SolidAngleSiUnit::Create);

SolidAngleSiUnit::SolidAngleSiUnit() {
	step = NULL;
	id = 0;
}

SolidAngleSiUnit::SolidAngleSiUnit(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

SolidAngleSiUnit::~SolidAngleSiUnit() {
}

bool
SolidAngleSiUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;


	// load base class attributes
	if ( !SolidAngleUnit::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Unit." << endl;
		return false;
	}
	if ( !SiUnit::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Unit." << endl;
		return false;
	}

	return true;
}

void
SolidAngleSiUnit::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	SolidAngleUnit::Print(level+1);
	SiUnit::Print(level+1);

}
STEPEntity *
SolidAngleSiUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SolidAngleSiUnit *object = new SolidAngleSiUnit(sw,sse->STEPfile_id);

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
