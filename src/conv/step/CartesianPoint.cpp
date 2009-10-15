/*                 CartesianPoint.cpp
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
/** @file CartesianPoint.cpp
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

CartesianPoint::CartesianPoint(STEPWrapper *sw, int STEPid) {
	step = sw;
	id = STEPid;
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
		cout << CLASSNAME << ":Error loading base class ::Point." << endl;
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
		cout << CLASSNAME << ": error loading 'coordinate' attribute." << endl;
	}
	return true;
}

void
CartesianPoint::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "coordinate:";
	cout << "(" << coordinates[0] << ",";
	cout << coordinates[1] << ",";
	cout << coordinates[2] << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	Point::Print(level+1);
}

STEPEntity *
CartesianPoint::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		CartesianPoint *object = new CartesianPoint(sw,sse->STEPfile_id);

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

bool
CartesianPoint::LoadONBrep(ON_Brep *brep)
{
	AddVertex(brep);
	return true;
}

