/*                 TopologicalRepresentationItem.cpp
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
/** @file step/TopologicalRepresentationItem.cpp
 *
 * Routines to convert STEP "TopologicalRepresentationItem" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "TopologicalRepresentationItem.h"

#define CLASSNAME "TopologicalRepresentationItem"
#define ENTITYNAME "Topological_Representation_Item"
string TopologicalRepresentationItem::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)TopologicalRepresentationItem::Create);

TopologicalRepresentationItem::TopologicalRepresentationItem() {
    step = NULL;
    id = 0;
}

TopologicalRepresentationItem::TopologicalRepresentationItem(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
}

TopologicalRepresentationItem::~TopologicalRepresentationItem() {
}

bool
TopologicalRepresentationItem::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if ( !RepresentationItem::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::RepresentationItem." << std::endl;
	return false;
    }

    return true;
}

void
TopologicalRepresentationItem::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    RepresentationItem::Print(level+1);
}

STEPEntity *
TopologicalRepresentationItem::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	TopologicalRepresentationItem *object = new TopologicalRepresentationItem(sw,sse->STEPfile_id);

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
TopologicalRepresentationItem::LoadONBrep(ON_Brep *brep)
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
