/*                 ElementarySurface.cpp
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
/** @file step/ElementarySurface.cpp
 *
 * Routines to interface to STEP "ElementarySurface".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "ElementarySurface.h"

#include "Axis2Placement3D.h"

#define CLASSNAME "ElementarySurface"
#define ENTITYNAME "Elementary_Surface"
string ElementarySurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)ElementarySurface::Create);

ElementarySurface::ElementarySurface() {
    step = NULL;
    id = 0;
    position = NULL;
}

ElementarySurface::ElementarySurface(STEPWrapper *sw,int step_id) {
    step=sw;
    id = step_id;
    position = NULL;
}

ElementarySurface::~ElementarySurface() {
}

bool
ElementarySurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !Surface::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (position == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"position");
	if (entity) {
	    position = dynamic_cast<Axis2Placement3D *>(Factory::CreateObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'position' attribute." << std::endl;
	    return false;
	}
    }

    return true;
}

void
ElementarySurface::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    if (position != NULL) {
	position->Print(level+1);
    }
}

STEPEntity *
ElementarySurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	ElementarySurface *object = new ElementarySurface(sw,sse->STEPfile_id);

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
ElementarySurface::LoadONBrep(ON_Brep *brep)
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
