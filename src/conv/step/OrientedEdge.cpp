/*                 OrientedEdge.cpp
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
/** @file step/OrientedEdge.cpp
 *
 * Routines to convert STEP "OrientedEdge" to BRL-CAD BREP
 * structures.
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "OrientedEdge.h"
#include "EdgeCurve.h"
#include "VertexPoint.h"

#define CLASSNAME "OrientedEdge"
#define ENTITYNAME "Oriented_Edge"
string OrientedEdge::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)OrientedEdge::Create);

OrientedEdge::OrientedEdge() {
    step = NULL;
    id = 0;
    edge_element = NULL;
}

OrientedEdge::OrientedEdge(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    edge_element = NULL;
}

OrientedEdge::~OrientedEdge() {
    edge_element = NULL;
}

bool
OrientedEdge::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;


    if ( !Edge::Load(sw,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Curve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    orientation = step->getBooleanAttribute(sse,"orientation");

    if (edge_element == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"edge_element");
	if (entity) {
	    edge_element = dynamic_cast<Edge *>(Factory::CreateObject(sw,entity));
	    if (orientation == BTrue) {
		edge_start = edge_element->GetEdgeStart();
		edge_end = edge_element->GetEdgeEnd();
	    } else {
		edge_start = edge_element->GetEdgeEnd();
		edge_end = edge_element->GetEdgeStart();
	    }
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'edge_element'." << std::endl;
	    return false;
	}
    }

    return true;
}

void
OrientedEdge::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "edge_element:" << std::endl;
    edge_element->Print(level+1);
    TAB(level+1); std::cout << "orientation:" << step->getBooleanString((Boolean)orientation) << std::endl;

}

STEPEntity *
OrientedEdge::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	OrientedEdge *object = new OrientedEdge(sw,sse->STEPfile_id);

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
OrientedEdge::OrientWithEdge() {
    if ((Boolean)orientation == BTrue) {
	return true;
    }
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
