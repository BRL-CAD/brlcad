/*                 SolidModel.cpp
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
/** @file SolidModel.cpp
 *
 * Routines to convert STEP "SolidModel" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "SolidModel.h"

#define CLASSNAME "SolidModel"
#define ENTITYNAME "Solid_Model"
string SolidModel::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SolidModel::Create);

SolidModel::SolidModel() {
	step = NULL;
	id = 0;
}

SolidModel::SolidModel(STEPWrapper *sw,int STEPid) {
	step=sw;
	id = STEPid;
}

SolidModel::~SolidModel() {
}

bool
SolidModel::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !GeometricRepresentationItem::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << endl;
		return false;
	}
	return true;
}

void
SolidModel::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	GeometricRepresentationItem::Print(level);
}

STEPEntity *
SolidModel::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		SolidModel *object = new SolidModel(sw,sse->STEPfile_id);

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
/*
void
SolidModel::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implememnted for " << entityname << endl;
	return; // false;
}
*/
