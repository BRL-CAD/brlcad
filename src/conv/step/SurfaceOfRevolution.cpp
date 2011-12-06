/*                 SurfaceOfRevolution.cpp
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
/** @file step/SurfaceOfRevolution.cpp
 *
 * Routines to interface to STEP "SurfaceOfRevolution".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "SurfaceOfRevolution.h"

#include "Axis1Placement.h"

#define CLASSNAME "Surface_Of_Linear_Extrusion"
#define ENTITYNAME "Surface_Of_Revolution"
string SurfaceOfRevolution::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)SurfaceOfRevolution::Create);

SurfaceOfRevolution::SurfaceOfRevolution() {
    step = NULL;
    id = 0;
    axis_position = NULL;
}

SurfaceOfRevolution::SurfaceOfRevolution(STEPWrapper *sw,int step_id) {
    step=sw;
    id = step_id;
    axis_position = NULL;
}

SurfaceOfRevolution::~SurfaceOfRevolution() {
}

bool
SurfaceOfRevolution::Load(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !SweptSurface::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (axis_position == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"axis_position");
	if (entity) {
	    axis_position = dynamic_cast<Axis1Placement *>(Factory::CreateObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": error loading 'axis_position' attribute." << std::endl;
	    return false;
	}
    }

    return true;
}

void
SurfaceOfRevolution::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    if (axis_position != NULL)
	axis_position->Print(level+1);

    SweptSurface::Print(level+1);
}

STEPEntity *
SurfaceOfRevolution::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	SurfaceOfRevolution *object = new SurfaceOfRevolution(sw,sse->STEPfile_id);

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
