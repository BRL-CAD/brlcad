/*                 RectangularCompositeSurface.cpp
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
/** @file step/RectangularCompositeSurface.cpp
 *
 * Routines to interface to STEP "RectangularCompositeSurface".
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "SurfacePatch.h"

#include "RectangularCompositeSurface.h"

#define CLASSNAME "RectangularCompositeSurface"
#define ENTITYNAME "Rectangular_Composite_Surface"
string RectangularCompositeSurface::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)RectangularCompositeSurface::Create);

RectangularCompositeSurface::RectangularCompositeSurface() {
    step=NULL;
    id = 0;
    segments = NULL;
}

RectangularCompositeSurface::RectangularCompositeSurface(STEPWrapper *sw,int step_id) {
    step=sw;
    id = step_id;
    segments = NULL;
}

RectangularCompositeSurface::~RectangularCompositeSurface() {
    LIST_OF_LIST_OF_PATCHES::iterator i = segments->begin();

    while(i != segments->end()) {
	(*i)->clear();
	delete (*i);
	i = segments->erase(i);
    }
    segments->clear();
    delete segments;
}

bool
RectangularCompositeSurface::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {

    step=sw;
    id = sse->STEPfile_id;

    if ( !BoundedSurface::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedSurface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (segments == NULL)
	segments = step->getListOfListOfPatches(sse,"segments");

    return true;
}

void
RectangularCompositeSurface::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    TAB(level+1); std::cout << "segments:" << std::endl;
    LIST_OF_LIST_OF_PATCHES::iterator i;
    int cnt=0;
    for(i=segments->begin(); i != segments->end(); ++i) {
	LIST_OF_PATCHES::iterator j;
	LIST_OF_PATCHES *p = *i;
	TAB(level+1); std::cout << "surface_patch " << cnt++ << ":" << std::endl;
	for(j=p->begin(); j != p->end(); ++j) {
	    (*j)->Print(level+1);
	}
    }

    TAB(level); std::cout << "Inherited Attributes:" << std::endl;
    BoundedSurface::Print(level+1);
}

STEPEntity *
RectangularCompositeSurface::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	RectangularCompositeSurface *object = new RectangularCompositeSurface(sw,sse->STEPfile_id);

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
RectangularCompositeSurface::LoadONBrep(ON_Brep *brep)
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
