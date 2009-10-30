/*                 Edge.cpp
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
/** @file Edge.cpp
 *
 * Routines to convert STEP "Edge" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Edge.h"
#include "VertexPoint.h"

#define CLASSNAME "Edge"
#define ENTITYNAME "Edge"
string Edge::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Edge::Create);

Edge::Edge() {
	step = NULL;
	id = 0;
	edge_start = NULL;
	edge_end = NULL;
}

Edge::Edge(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	edge_start = NULL;
	edge_end = NULL;
}

Edge::~Edge() {
	edge_start = NULL;
	edge_end = NULL;
}

bool
Edge::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !TopologicalRepresentationItem::Load(step,sse) ) {
		cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (edge_start == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"edge_start");
		if (entity) {
			if (entity->STEPfile_id > 0)
				edge_start = dynamic_cast<Vertex *>(Factory::CreateObject(sw,entity));
		} else {
			cout << CLASSNAME << ":Error loading attribute edge_start" << endl;
			return false;
		}
	}
	if (edge_end == NULL) {
		SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"edge_end");
		if (entity) {
			if (entity->STEPfile_id > 0)
				edge_end = dynamic_cast<Vertex *>(Factory::CreateObject(sw,entity));
		} else {
			cout << CLASSNAME << ":Error loading attribute edge_end" << endl;
			return false;
		}
	}
	return true;
}

void
Edge::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << name << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "edge_start:" << endl;
	if (edge_start) edge_start->Print(level+1);

	TAB(level+1); cout << "edge_end:" << endl;
	if (edge_end) edge_end->Print(level+1);
}

STEPEntity *
Edge::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Edge *object = new Edge(sw,sse->STEPfile_id);

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
Edge::LoadONBrep(ON_Brep *brep)
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
