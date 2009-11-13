/*                 Direction.cpp
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
/** @file Direction.cpp
 *
 * Routines to convert STEP "Direction" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianPoint.h"

#include "Direction.h"

#define CLASSNAME "Direction"
#define ENTITYNAME "Direction"
string Direction::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Direction::Create);

Direction::Direction() {
	step = NULL;
	id = 0;
}

Direction::Direction(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

Direction::~Direction() {
}

bool
Direction::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !GeometricRepresentationItem::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::GeometricRepresentationItem." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	STEPattribute *attr = step->getAttribute(sse,"direction_ratios");
	if (attr != NULL) {
		STEPaggregate *sa = (STEPaggregate *)(attr->ptr.a);
		RealNode *rn = (RealNode *)sa->GetHead();
		int index = 0;
		while ( rn != NULL) {
			direction_ratios[index++] = rn->value;
			rn = (RealNode *)rn->NextNode();
		}
	} else {
		cout << CLASSNAME << ": error loading 'coordinate' attribute." << endl;
	}

	return true;
}

void
Direction::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Local Attributes:" << endl;
	TAB(level+1); cout << "direction_ratios:";
	cout << "(" << direction_ratios[0] << ",";
	cout << direction_ratios[1] << ",";
	cout << direction_ratios[2] << ")" << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	GeometricRepresentationItem::Print(level+1);
}

STEPEntity *
Direction::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Direction *object = new Direction(sw,sse->STEPfile_id);

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
Direction::LoadONBrep(ON_Brep *brep)
{
	cerr << "Error: ::LoadONBrep(ON_Brep *brep) not implemented for " << entityname << endl;
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
