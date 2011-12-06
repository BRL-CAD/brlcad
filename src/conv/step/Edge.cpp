/*                 Edge.cpp
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
/** @file step/Edge.cpp
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

Edge::Edge(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
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
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
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
	    std::cout << CLASSNAME << ":Error loading attribute edge_start" << std::endl;
	    return false;
	}
    }
    if (edge_end == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"edge_end");
	if (entity) {
	    if (entity->STEPfile_id > 0)
		edge_end = dynamic_cast<Vertex *>(Factory::CreateObject(sw,entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute edge_end" << std::endl;
	    return false;
	}
    }
    return true;
}

void
Edge::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "edge_start:" << std::endl;
    if (edge_start) edge_start->Print(level+1);

    TAB(level+1); std::cout << "edge_end:" << std::endl;
    if (edge_end) edge_end->Print(level+1);
}

STEPEntity *
Edge::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	Edge *object = new Edge(sw,sse->STEPfile_id);

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

bool
Edge::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << ">) not implemented for " << entityname << std::endl;
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
