/*                 Path.cpp
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
/** @file step/Path.cpp
 *
 * Routines to convert STEP "Path" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Path.h"
#include "OrientedEdge.h"

#define CLASSNAME "Path"
#define ENTITYNAME "Path"
string Path::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)Path::Create);

Path::Path() {
	step = NULL;
	id = 0;
}

Path::Path(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

Path::~Path() {
	/*
	LIST_OF_ORIENTED_EDGES::iterator i = edge_list.begin();

	while(i != edge_list.end()) {
		delete (*i);
		i = edge_list.erase(i);
	}
	*/
	edge_list.clear();
}

LIST_OF_ORIENTED_EDGES::iterator
Path::getNext(LIST_OF_ORIENTED_EDGES::iterator i) {
	i++;
	if (i==edge_list.end()){
		i=edge_list.begin();
	}
	return i;
}

LIST_OF_ORIENTED_EDGES::iterator
Path::getPrev(LIST_OF_ORIENTED_EDGES::iterator i) {
	if (i==edge_list.begin()){
		i=edge_list.end();
	}
	i--;
	return i;
}

bool
Path::isSeam(LIST_OF_ORIENTED_EDGES::iterator i) {
	int edge_id = (*i)->GetONId();
	int cnt = 0;
	for(i=edge_list.begin();i!=edge_list.end();i++) {
		if (edge_id == (*i)->GetONId()) {
			cnt++;
			if (cnt == 2 )
				return true;
		}
	}
	return false;
}

bool
Path::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	if ( !TopologicalRepresentationItem::Load(step,sse) ) {
		std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
		return false;
	}

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	if (edge_list.empty()) {
		LIST_OF_ENTITIES *l = step->getListOfEntities(sse, "edge_list");
		LIST_OF_ENTITIES::iterator i;
		for (i = l->begin(); i != l->end(); i++) {
			SCLP23(Application_instance) *entity = (*i);
			if (entity) {
				OrientedEdge *aOE =dynamic_cast<OrientedEdge *>(Factory::CreateObject(sw, entity));
				edge_list.push_back(aOE);
			} else {
				std::cerr << CLASSNAME
						<< ": Unhandled entity in attribute 'edge_list'."
						<< std::endl;
				return false;
			}
		}
		l->clear();
		delete l;
	}

	return true;
}

void
Path::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << name << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	TAB(level+1); std::cout << "edge_list:" << std::endl;
	LIST_OF_ORIENTED_EDGES::iterator i;
	for(i=edge_list.begin();i!=edge_list.end();i++) {
		(*i)->Print(level+1);
	}

	TAB(level); std::cout << "Inherited Attributes:" << std::endl;
	TopologicalRepresentationItem::Print(level+1);
}

STEPEntity *
Path::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		Path *object = new Path(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
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
