/*                 GeometricRepresentationContext.cpp
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
/** @file GeometricRepresentationContext.cpp
 *
 * Routines to convert STEP "GeometricRepresentationContext" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "GeometricRepresentationContext.h"

#define CLASSNAME "GeometricRepresentationContext"
#define ENTITYNAME "Geometric_Representation_Context"
string GeometricRepresentationContext::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)GeometricRepresentationContext::Create);

GeometricRepresentationContext::GeometricRepresentationContext() {
	step = NULL;
	id = 0;
}

GeometricRepresentationContext::GeometricRepresentationContext(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
}

GeometricRepresentationContext::~GeometricRepresentationContext() {
}

bool
GeometricRepresentationContext::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// load base class attributes
	if ( !RepresentationContext::Load(sw,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::RepresentationContext." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	coordinate_space_dimension = step->getIntegerAttribute(sse,"coordinate_space_dimension");

	return true;
}

void
GeometricRepresentationContext::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "coordinate_space_dimension:" << coordinate_space_dimension << endl;

	TAB(level); cout << "Inherited Attributes:" << endl;
	RepresentationContext::Print(level+1);
}
STEPEntity *
GeometricRepresentationContext::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		GeometricRepresentationContext *object = new GeometricRepresentationContext(sw,sse->STEPfile_id);

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

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
