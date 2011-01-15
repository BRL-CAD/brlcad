/*                 Hyperbola.cpp
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
/** @file Curve.cpp
 *
 * Routines to convert STEP "Curve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Vertex.h"
#include "Hyperbola.h"

#define CLASSNAME "Hyperbola"
#define ENTITYNAME "Hyperbola"
string Hyperbola::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Hyperbola::Create);

Hyperbola::Hyperbola() {
	step = NULL;
	id = 0;
}

Hyperbola::Hyperbola(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

Hyperbola::~Hyperbola() {
}

bool
Hyperbola::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Conic::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::Conic." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	semi_axis = step->getRealAttribute(sse,"semi_axis");
	semi_imag_axis = step->getRealAttribute(sse,"semi_imag_axis");

	return true;
}

void
Hyperbola::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "semi_axis:" << semi_axis << std::endl;
	TAB(level+1); std::cout << "semi_imag_axis:" << semi_imag_axis << std::endl;

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	Conic::Print(level+1);
}
STEPEntity *
Hyperbola::Create(STEPWrapper *sw,SCLP23(Application_instance) *sse){
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Hyperbola *object = new Hyperbola(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw,sse)) {
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
