/*                 OffsetSurface.cpp
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
/** @file step/OffsetSurface.cpp
 *
 * Routines to interface to STEP "OffsetSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "Surface.h"
#include "OffsetSurface.h"

#define CLASSNAME "OffsetSurface"
#define ENTITYNAME "Offset_Surface"
string OffsetSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)OffsetSurface::Create);

OffsetSurface::OffsetSurface() {
    step=NULL;
    id = 0;
    basis_surface = NULL;
}

OffsetSurface::OffsetSurface(STEPWrapper *sw,int step_id) {
    step=sw;
    id = step_id;
    basis_surface = NULL;
}

OffsetSurface::~OffsetSurface() {
}

bool
OffsetSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {

    step=sw;
    id = sse->STEPfile_id;

    if ( !Surface::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (basis_surface == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"basis_surface");
	if (entity) {
	    basis_surface = dynamic_cast<Surface *>(Factory::CreateObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'basis_surface' attribute." << std::endl;
	    return false;
	}
    }

    distance = step->getRealAttribute(sse,"distance");
    self_intersect = step->getLogicalAttribute(sse,"self_intersect");

    return true;
}

void
OffsetSurface::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    basis_surface->Print(level+1);

    TAB(level+1); std::cout << "distance:" << distance << std::endl;
    TAB(level+1); std::cout << "self_intersect:" << step->getLogicalString((Logical)self_intersect) << std::endl;

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    Surface::Print(level+1);
}

STEPEntity *
OffsetSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	OffsetSurface *object = new OffsetSurface(sw,sse->STEPfile_id);

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
OffsetSurface::LoadONBrep(ON_Brep *brep)
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
