/*                 VertexLoop.cpp
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
/** @file VertexLoop.cpp
 *
 * Routines to convert STEP "VertexLoop" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Vertex.h"
#include "VertexLoop.h"

#define CLASSNAME "VertexLoop"
#define ENTITYNAME "Vertex_Loop"
string VertexLoop::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)VertexLoop::Create);

VertexLoop::VertexLoop() {
	step = NULL;
	id = 0;
	loop_vertex = NULL;
}

VertexLoop::VertexLoop(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	loop_vertex = NULL;
}

VertexLoop::~VertexLoop() {
}

bool
VertexLoop::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !Loop::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::Path." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (loop_vertex == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"loop_vertex");
		if (entity) {
			loop_vertex = dynamic_cast<Vertex *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
		} else {
			cerr << CLASSNAME << ": Error loading entity attribute 'loop_vertex'." << endl;
			return false;
		}
	}

	return true;
}

void
VertexLoop::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Local Attributes:" << endl;
	loop_vertex->Print(level+1);

	TAB(level); cout << "Inherited Attributes:" << endl;
	Loop::Print(level+1);
}

STEPEntity *
VertexLoop::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		VertexLoop *object = new VertexLoop(sw,sse->STEPfile_id);

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
