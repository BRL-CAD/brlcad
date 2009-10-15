/*                 Parabola.cpp
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
/** @file Curve.cpp
 *
 * Routines to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Parabola.h"

#define CLASSNAME "Parabola"
#define ENTITYNAME "Parabola"
string Parabola::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Parabola::Create);

Parabola::Parabola() {
	step = NULL;
	id = 0;
}

Parabola::Parabola(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

Parabola::~Parabola() {
}

bool
Parabola::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Conic::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Conic." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	focal_dist = step->getRealAttribute(sse,"focal_dist");

	return true;
}

void
Parabola::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "focal_dist:" << focal_dist << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	Conic::Print(level+1);
}
STEPEntity *
Parabola::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Parabola *object = new Parabola(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw,sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method." << endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}
